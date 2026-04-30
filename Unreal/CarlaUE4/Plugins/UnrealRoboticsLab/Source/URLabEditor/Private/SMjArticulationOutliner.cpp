// Copyright (c) 2026 Jonathan Embley-Riches. All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// --- LEGAL DISCLAIMER ---
// UnrealRoboticsLab is an independent software plugin. It is NOT affiliated with,
// endorsed by, or sponsored by Epic Games, Inc. "Unreal" and "Unreal Engine" are
// trademarks or registered trademarks of Epic Games, Inc. in the US and elsewhere.
//
// This plugin incorporates third-party software: MuJoCo (Apache 2.0),
// CoACD (MIT), and libzmq (MPL 2.0). See ThirdPartyNotices.txt for details.

#include "SMjArticulationOutliner.h"
#include "Widgets/Input/SSearchBox.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SWrapBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Engine/SimpleConstructionScript.h"
#include "Engine/SCS_Node.h"
#include "Engine/Blueprint.h"
#include "Editor.h"
#include "Subsystems/AssetEditorSubsystem.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "BlueprintEditor.h"

#include "MuJoCo/Core/MjArticulation.h"
#include "MuJoCo/Components/Bodies/MjBody.h"
#include "MuJoCo/Components/Geometry/MjGeom.h"
#include "MuJoCo/Components/Geometry/MjSite.h"
#include "MuJoCo/Components/Joints/MjJoint.h"
#include "MuJoCo/Components/Sensors/MjSensor.h"
#include "MuJoCo/Components/Actuators/MjActuator.h"
#include "MuJoCo/Components/Defaults/MjDefault.h"

void SMjArticulationOutliner::Construct(const FArguments& InArgs)
{
	auto MakeFilterToggle = [this](const FString& Label, bool& bFilterVar) -> TSharedRef<SWidget>
	{
		return SNew(SCheckBox)
			.IsChecked_Lambda([&bFilterVar]() { return bFilterVar ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
			.OnCheckStateChanged_Lambda([this, &bFilterVar](ECheckBoxState NewState)
			{
				bFilterVar = (NewState == ECheckBoxState::Checked);
				ApplyFilters();
			})
			[
				SNew(STextBlock).Text(FText::FromString(Label))
			];
	};

	ChildSlot
	[
		SNew(SVerticalBox)

		// Header: BP selector dropdown
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(4.f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(0.f, 0.f, 4.f, 0.f)
			[
				SNew(STextBlock)
				.Text(FText::FromString(TEXT("Articulation:")))
				.Font(FCoreStyle::GetDefaultFontStyle("Bold", 10))
			]
			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			[
				SAssignNew(BPComboBox, SComboBox<TSharedPtr<FString>>)
				.OptionsSource(&AvailableBPNames)
				.OnSelectionChanged(this, &SMjArticulationOutliner::OnBPSelected)
				.OnGenerateWidget_Lambda([](TSharedPtr<FString> Item) -> TSharedRef<SWidget>
				{
					return SNew(STextBlock).Text(FText::FromString(*Item));
				})
				.Content()
				[
					SNew(STextBlock)
					.Text_Lambda([this]() -> FText
					{
						if (SelectedBPName.IsValid()) return FText::FromString(*SelectedBPName);
						return FText::FromString(TEXT("No articulation BPs open"));
					})
				]
			]
		]

		// Search bar
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(4.f)
		[
			SNew(SSearchBox)
			.HintText(FText::FromString(TEXT("Search components...")))
			.OnTextChanged_Lambda([this](const FText& NewText)
			{
				SearchText = NewText.ToString();
				ApplyFilters();
			})
		]

		// Filter toggles
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(4.f)
		[
			SNew(SWrapBox)
			.UseAllottedSize(true)
			+ SWrapBox::Slot().Padding(2.f) [ MakeFilterToggle(TEXT("Bodies"), bShowBodies) ]
			+ SWrapBox::Slot().Padding(2.f) [ MakeFilterToggle(TEXT("Geoms"), bShowGeoms) ]
			+ SWrapBox::Slot().Padding(2.f) [ MakeFilterToggle(TEXT("Joints"), bShowJoints) ]
			+ SWrapBox::Slot().Padding(2.f) [ MakeFilterToggle(TEXT("Sensors"), bShowSensors) ]
			+ SWrapBox::Slot().Padding(2.f) [ MakeFilterToggle(TEXT("Actuators"), bShowActuators) ]
			+ SWrapBox::Slot().Padding(2.f) [ MakeFilterToggle(TEXT("Defaults"), bShowDefaults) ]
			+ SWrapBox::Slot().Padding(2.f) [ MakeFilterToggle(TEXT("Sites"), bShowSites) ]
			+ SWrapBox::Slot().Padding(2.f) [ MakeFilterToggle(TEXT("Other"), bShowOther) ]
		]

		// Tree view
		+ SVerticalBox::Slot()
		.FillHeight(1.0f)
		[
			SAssignNew(TreeView, STreeView<TSharedPtr<FMjOutlinerEntry>>)
			.TreeItemsSource(&FilteredRootEntries)
			.OnGenerateRow(this, &SMjArticulationOutliner::OnGenerateRow)
			.OnGetChildren(this, &SMjArticulationOutliner::OnGetChildren)
			.OnSelectionChanged(this, &SMjArticulationOutliner::OnSelectionChanged)
		]

		// Summary bar
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(4.f)
		[
			SNew(STextBlock)
			.Text(this, &SMjArticulationOutliner::GetSummaryText)
			.ColorAndOpacity(FSlateColor(FLinearColor(0.6f, 0.6f, 0.6f)))
		]
	];
}

void SMjArticulationOutliner::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);

	TimeSinceLastRefresh += InDeltaTime;
	if (TimeSinceLastRefresh < 1.0f) return;
	TimeSinceLastRefresh = 0.f;

	RefreshAvailableBPs();

	// Auto-select first BP if none selected
	if (!CurrentBP.IsValid() && AvailableBPs.Num() > 0)
	{
		RefreshFromBlueprint(AvailableBPs[0].Get());
		SelectedBPName = AvailableBPNames[0];
		if (BPComboBox.IsValid()) BPComboBox->SetSelectedItem(SelectedBPName);
	}
}

void SMjArticulationOutliner::RefreshAvailableBPs()
{
	TArray<TWeakObjectPtr<UBlueprint>> NewBPs;

	if (GEditor)
	{
		UAssetEditorSubsystem* Sub = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>();
		if (Sub)
		{
			for (UObject* Asset : Sub->GetAllEditedAssets())
			{
				UBlueprint* BP = Cast<UBlueprint>(Asset);
				if (BP && BP->GeneratedClass && BP->GeneratedClass->IsChildOf(AMjArticulation::StaticClass()))
				{
					NewBPs.Add(BP);
				}
			}
		}
	}

	// Only rebuild if changed
	bool bChanged = (NewBPs.Num() != AvailableBPs.Num());
	if (!bChanged)
	{
		for (int32 i = 0; i < NewBPs.Num(); i++)
		{
			if (NewBPs[i] != AvailableBPs[i]) { bChanged = true; break; }
		}
	}

	if (bChanged)
	{
		AvailableBPs = NewBPs;
		AvailableBPNames.Empty();
		for (const auto& WBP : AvailableBPs)
		{
			if (UBlueprint* BP = WBP.Get())
				AvailableBPNames.Add(MakeShared<FString>(BP->GetName()));
		}
		if (BPComboBox.IsValid()) BPComboBox->RefreshOptions();
	}
}

void SMjArticulationOutliner::OnBPSelected(TSharedPtr<FString> Selected, ESelectInfo::Type SelectInfo)
{
	SelectedBPName = Selected;
	if (!Selected.IsValid()) return;

	for (const auto& WBP : AvailableBPs)
	{
		UBlueprint* BP = WBP.Get();
		if (BP && BP->GetName() == *Selected)
		{
			RefreshFromBlueprint(BP);
			return;
		}
	}
}

void SMjArticulationOutliner::RefreshFromBlueprint(UBlueprint* BP)
{
	CurrentBP = BP;
	RebuildTree();
}

void SMjArticulationOutliner::RebuildTree()
{
	RootEntries.Empty();

	UBlueprint* BP = CurrentBP.Get();
	if (!BP || !BP->SimpleConstructionScript) return;

	for (USCS_Node* RootNode : BP->SimpleConstructionScript->GetRootNodes())
	{
		TSharedPtr<FMjOutlinerEntry> RootEntry = MakeShared<FMjOutlinerEntry>();
		RootEntry->Name = RootNode->GetVariableName().ToString();
		RootEntry->TypeLabel = GetTypeLabel(RootNode->ComponentTemplate);
		RootEntry->ComponentTemplate = RootNode->ComponentTemplate;
		RootEntry->SCSNode = RootNode;

		CollectNodes(RootNode, RootEntry);
		RootEntries.Add(RootEntry);
	}

	ApplyFilters();
}

void SMjArticulationOutliner::CollectNodes(USCS_Node* Node, TSharedPtr<FMjOutlinerEntry> ParentEntry)
{
	for (USCS_Node* Child : Node->GetChildNodes())
	{
		TSharedPtr<FMjOutlinerEntry> Entry = MakeShared<FMjOutlinerEntry>();
		Entry->Name = Child->GetVariableName().ToString();
		Entry->TypeLabel = GetTypeLabel(Child->ComponentTemplate);
		Entry->ComponentTemplate = Child->ComponentTemplate;
		Entry->SCSNode = Child;

		CollectNodes(Child, Entry);
		ParentEntry->Children.Add(Entry);
	}
}

void SMjArticulationOutliner::ApplyFilters()
{
	FilteredRootEntries.Empty();

	for (const auto& RootEntry : RootEntries)
	{
		TSharedPtr<FMjOutlinerEntry> Filtered = FilterTree(RootEntry);
		if (Filtered.IsValid())
		{
			FilteredRootEntries.Add(Filtered);
		}
	}

	if (TreeView.IsValid())
	{
		TreeView->RequestTreeRefresh();

		// Auto-expand the first two levels for visibility
		for (const auto& Entry : FilteredRootEntries)
		{
			TreeView->SetItemExpansion(Entry, true);
			for (const auto& Child : Entry->Children)
			{
				TreeView->SetItemExpansion(Child, true);
			}
		}
	}
}

bool SMjArticulationOutliner::PassesFilter(const TSharedPtr<FMjOutlinerEntry>& Entry) const
{
	UObject* Comp = Entry->ComponentTemplate.Get();
	if (!Comp) return false;

	// Type filter
	bool bPassType = false;
	if (IsBody(Comp))         bPassType = bShowBodies;
	else if (IsGeom(Comp))    bPassType = bShowGeoms;
	else if (IsJoint(Comp))   bPassType = bShowJoints;
	else if (IsSensor(Comp))  bPassType = bShowSensors;
	else if (IsActuator(Comp)) bPassType = bShowActuators;
	else if (IsDefault(Comp)) bPassType = bShowDefaults;
	else if (IsSite(Comp))    bPassType = bShowSites;
	else                      bPassType = bShowOther;

	if (!bPassType) return false;

	// Text search
	if (!SearchText.IsEmpty())
	{
		if (!Entry->Name.Contains(SearchText, ESearchCase::IgnoreCase) &&
			!Entry->TypeLabel.Contains(SearchText, ESearchCase::IgnoreCase))
		{
			return false;
		}
	}

	return true;
}

TSharedPtr<FMjOutlinerEntry> SMjArticulationOutliner::FilterTree(const TSharedPtr<FMjOutlinerEntry>& Entry) const
{
	// Recursively filter: keep a node if it passes OR if any descendant passes
	TArray<TSharedPtr<FMjOutlinerEntry>> FilteredChildren;
	for (const auto& Child : Entry->Children)
	{
		TSharedPtr<FMjOutlinerEntry> FilteredChild = FilterTree(Child);
		if (FilteredChild.IsValid())
		{
			FilteredChildren.Add(FilteredChild);
		}
	}

	bool bSelfPasses = PassesFilter(Entry);

	if (!bSelfPasses && FilteredChildren.IsEmpty())
		return nullptr;

	// Create a shallow copy with filtered children
	TSharedPtr<FMjOutlinerEntry> Result = MakeShared<FMjOutlinerEntry>();
	Result->Name = Entry->Name;
	Result->TypeLabel = Entry->TypeLabel;
	Result->ComponentTemplate = Entry->ComponentTemplate;
	Result->SCSNode = Entry->SCSNode;
	Result->Children = FilteredChildren;
	return Result;
}

TSharedRef<ITableRow> SMjArticulationOutliner::OnGenerateRow(
	TSharedPtr<FMjOutlinerEntry> Entry,
	const TSharedRef<STableViewBase>& OwnerTable)
{
	// Color code by type
	FLinearColor TypeColor(0.5f, 0.7f, 1.0f);
	UObject* Comp = Entry->ComponentTemplate.Get();
	if (Comp)
	{
		if (IsBody(Comp))         TypeColor = FLinearColor(0.9f, 0.7f, 0.3f);
		else if (IsGeom(Comp))    TypeColor = FLinearColor(0.5f, 0.8f, 0.5f);
		else if (IsJoint(Comp))   TypeColor = FLinearColor(0.5f, 0.6f, 1.0f);
		else if (IsSensor(Comp))  TypeColor = FLinearColor(0.8f, 0.5f, 0.8f);
		else if (IsActuator(Comp)) TypeColor = FLinearColor(1.0f, 0.5f, 0.4f);
		else if (IsDefault(Comp)) TypeColor = FLinearColor(0.6f, 0.6f, 0.6f);
		else if (IsSite(Comp))    TypeColor = FLinearColor(0.4f, 0.9f, 0.9f);
	}

	return SNew(STableRow<TSharedPtr<FMjOutlinerEntry>>, OwnerTable)
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(2.f, 1.f)
		[
			SNew(STextBlock)
			.Text(FText::FromString(FString::Printf(TEXT("[%s]"), *Entry->TypeLabel)))
			.ColorAndOpacity(FSlateColor(TypeColor))
			.Font(FCoreStyle::GetDefaultFontStyle("Regular", 8))
		]
		+ SHorizontalBox::Slot()
		.FillWidth(1.0f)
		.Padding(4.f, 1.f)
		[
			SNew(STextBlock)
			.Text(FText::FromString(Entry->Name))
		]
	];
}

void SMjArticulationOutliner::OnGetChildren(
	TSharedPtr<FMjOutlinerEntry> Entry,
	TArray<TSharedPtr<FMjOutlinerEntry>>& OutChildren)
{
	OutChildren = Entry->Children;
}

void SMjArticulationOutliner::OnSelectionChanged(
	TSharedPtr<FMjOutlinerEntry> Entry, ESelectInfo::Type SelectInfo)
{
	if (!Entry.IsValid() || !Entry->SCSNode || !GEditor) return;

	UBlueprint* BP = CurrentBP.Get();
	if (!BP) return;

	// Open/focus the BP editor and select the SCS node
	UAssetEditorSubsystem* Sub = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>();
	if (!Sub) return;

	IAssetEditorInstance* EditorInstance = Sub->FindEditorForAsset(BP, /*bFocusIfOpen=*/true);
	if (!EditorInstance)
	{
		Sub->OpenEditorForAsset(BP);
		EditorInstance = Sub->FindEditorForAsset(BP, true);
	}

	if (FBlueprintEditor* BPEditor = static_cast<FBlueprintEditor*>(EditorInstance))
	{
		BPEditor->FindAndSelectSubobjectEditorTreeNode(
			Cast<UActorComponent>(Entry->SCSNode->ComponentTemplate), /*IsCntrlDown=*/false);
	}
}

FString SMjArticulationOutliner::GetTypeLabel(UObject* Comp)
{
	if (!Comp) return TEXT("Unknown");

	// Use the specific class name for joints/geoms
	if (IsJoint(Comp))
	{
		FString ClassName = Comp->GetClass()->GetName();
		ClassName.RemoveFromStart(TEXT("Mj"));
		return ClassName;
	}
	if (IsGeom(Comp))
	{
		FString ClassName = Comp->GetClass()->GetName();
		ClassName.RemoveFromStart(TEXT("Mj"));
		if (ClassName == TEXT("Geom")) return TEXT("Geom");
		return ClassName;
	}

	if (IsBody(Comp))     return TEXT("Body");
	if (IsSensor(Comp))   return TEXT("Sensor");
	if (IsActuator(Comp)) return TEXT("Actuator");
	if (IsDefault(Comp))  return TEXT("Default");
	if (IsSite(Comp))     return TEXT("Site");
	return Comp->GetClass()->GetName();
}

bool SMjArticulationOutliner::IsBody(UObject* Comp)     { return Comp && Comp->IsA<UMjBody>(); }
bool SMjArticulationOutliner::IsGeom(UObject* Comp)     { return Comp && Comp->IsA<UMjGeom>(); }
bool SMjArticulationOutliner::IsJoint(UObject* Comp)    { return Comp && Comp->IsA<UMjJoint>(); }
bool SMjArticulationOutliner::IsSensor(UObject* Comp)   { return Comp && Comp->IsA<UMjSensor>(); }
bool SMjArticulationOutliner::IsActuator(UObject* Comp) { return Comp && Comp->IsA<UMjActuator>(); }
bool SMjArticulationOutliner::IsDefault(UObject* Comp)  { return Comp && Comp->IsA<UMjDefault>(); }
bool SMjArticulationOutliner::IsSite(UObject* Comp)     { return Comp && Comp->IsA<UMjSite>(); }

FText SMjArticulationOutliner::GetSummaryText() const
{
	int32 Bodies = 0, Geoms = 0, Joints = 0, Sensors = 0, Actuators = 0;

	// Count from the unfiltered tree
	TFunction<void(const TArray<TSharedPtr<FMjOutlinerEntry>>&)> CountAll =
		[&](const TArray<TSharedPtr<FMjOutlinerEntry>>& Entries)
	{
		for (const auto& Entry : Entries)
		{
			UObject* Comp = Entry->ComponentTemplate.Get();
			if (Comp)
			{
				if (IsBody(Comp))          Bodies++;
				else if (IsGeom(Comp))     Geoms++;
				else if (IsJoint(Comp))    Joints++;
				else if (IsSensor(Comp))   Sensors++;
				else if (IsActuator(Comp)) Actuators++;
			}
			CountAll(Entry->Children);
		}
	};
	CountAll(RootEntries);

	return FText::FromString(FString::Printf(
		TEXT("%d Bodies  |  %d Geoms  |  %d Joints  |  %d Sensors  |  %d Actuators"),
		Bodies, Geoms, Joints, Sensors, Actuators));
}
