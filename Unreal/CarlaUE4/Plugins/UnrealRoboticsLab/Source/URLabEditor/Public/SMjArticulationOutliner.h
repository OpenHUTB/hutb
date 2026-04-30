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

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/STreeView.h"
#include "Widgets/Input/SComboBox.h"

class UBlueprint;
class USCS_Node;

/**
 * @brief A single entry in the MuJoCo Outliner tree.
 */
struct FMjOutlinerEntry
{
	FString Name;
	FString TypeLabel;   // e.g. "Body", "HingeJoint", "Sensor"
	TWeakObjectPtr<UObject> ComponentTemplate;
	USCS_Node* SCSNode = nullptr;
	TArray<TSharedPtr<FMjOutlinerEntry>> Children;
};

/**
 * @class SMjArticulationOutliner
 * @brief Dockable editor panel that shows a filtered, searchable tree view
 *        of AMjArticulation Blueprints. Auto-detects the currently open
 *        articulation BP and refreshes when it changes.
 */
class SMjArticulationOutliner : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SMjArticulationOutliner) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;

private:
	/** Rebuild tree from the active BP. */
	void RefreshFromBlueprint(UBlueprint* BP);
	void RebuildTree();
	void CollectNodes(USCS_Node* Node, TSharedPtr<FMjOutlinerEntry> ParentEntry);

	/** Filter toggles */
	bool bShowBodies = true;
	bool bShowGeoms = true;
	bool bShowJoints = true;
	bool bShowSensors = true;
	bool bShowActuators = true;
	bool bShowDefaults = false;  // Off by default — usually not what you're navigating
	bool bShowSites = true;
	bool bShowOther = false;

	FString SearchText;
	float TimeSinceLastRefresh = 0.f;

	TWeakObjectPtr<UBlueprint> CurrentBP;
	TArray<TSharedPtr<FMjOutlinerEntry>> RootEntries;
	TArray<TSharedPtr<FMjOutlinerEntry>> FilteredRootEntries;

	TSharedPtr<STreeView<TSharedPtr<FMjOutlinerEntry>>> TreeView;

	/** BP selector */
	TArray<TWeakObjectPtr<UBlueprint>> AvailableBPs;
	TArray<TSharedPtr<FString>> AvailableBPNames;
	TSharedPtr<FString> SelectedBPName;
	TSharedPtr<class SComboBox<TSharedPtr<FString>>> BPComboBox;
	void RefreshAvailableBPs();
	void OnBPSelected(TSharedPtr<FString> Selected, ESelectInfo::Type SelectInfo);

	/** Tree view callbacks */
	TSharedRef<ITableRow> OnGenerateRow(TSharedPtr<FMjOutlinerEntry> Entry, const TSharedRef<STableViewBase>& OwnerTable);
	void OnGetChildren(TSharedPtr<FMjOutlinerEntry> Entry, TArray<TSharedPtr<FMjOutlinerEntry>>& OutChildren);
	void OnSelectionChanged(TSharedPtr<FMjOutlinerEntry> Entry, ESelectInfo::Type SelectInfo);

	/** Apply search and type filters, producing FilteredRootEntries */
	void ApplyFilters();
	bool PassesFilter(const TSharedPtr<FMjOutlinerEntry>& Entry) const;
	TSharedPtr<FMjOutlinerEntry> FilterTree(const TSharedPtr<FMjOutlinerEntry>& Entry) const;

	/** Type classification */
	static FString GetTypeLabel(UObject* ComponentTemplate);
	static bool IsBody(UObject* Comp);
	static bool IsGeom(UObject* Comp);
	static bool IsJoint(UObject* Comp);
	static bool IsSensor(UObject* Comp);
	static bool IsActuator(UObject* Comp);
	static bool IsDefault(UObject* Comp);
	static bool IsSite(UObject* Comp);

	FText GetSummaryText() const;
};
