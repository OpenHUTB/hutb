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

#include "UI/MjSimulateWidget.h"
#include "UI/MjPropertyRow.h"
#include "UI/MjCameraFeedEntry.h"
#include "MuJoCo/Components/Sensors/MjCamera.h"
#include "MuJoCo/Core/MjPhysicsEngine.h"
#include "MuJoCo/Core/MjDebugVisualizer.h"
#include "MuJoCo/Net/MjNetworkManager.h"
#include "Utils/URLabLogging.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "Components/ComboBoxString.h"
#include "Components/ExpandableArea.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/Spacer.h"
#include "MuJoCo/Core/MjArticulation.h"
#include "MuJoCo/Input/MjTwistController.h"
#include "Styling/SlateTypes.h"
#include "Fonts/SlateFontInfo.h"
#include "MuJoCo/Utils/MjUtils.h"
#include "Replay/MjReplayManager.h"
#include "Kismet/GameplayStatics.h"
#include "Components/CheckBox.h"

void UMjSimulateWidget::NativeConstruct()
{
    Super::NativeConstruct();

    auto StyleButton = [](UButton* Btn, FLinearColor BGColor) {
        if (!Btn) return;
        FButtonStyle Style = Btn->GetStyle();
        Style.Normal.TintColor = FSlateColor(BGColor);
        Style.Hovered.TintColor = FSlateColor(BGColor * 1.5f);
        Style.Pressed.TintColor = FSlateColor(BGColor * 0.5f);
        Btn->SetStyle(Style);
    };

    StyleButton(PlayPauseButton, FLinearColor(0.2f, 0.6f, 0.2f, 0.9f));
    StyleButton(ResetButton, FLinearColor(0.6f, 0.2f, 0.2f, 0.9f));
    StyleButton(RecordButton, FLinearColor(0.8f, 0.3f, 0.1f, 0.9f));
    StyleButton(ReplayButton, FLinearColor(0.2f, 0.4f, 0.8f, 0.9f));
    StyleButton(SnapshotButton, FLinearColor(0.2f, 0.6f, 0.8f, 0.9f));
    StyleButton(RestoreButton, FLinearColor(0.2f, 0.6f, 0.8f, 0.9f));

    if (TimeText)
    {
        FSlateFontInfo FontInfo = TimeText->GetFont();
        FontInfo.Size = 14;
        FontInfo.TypefaceFontName = TEXT("Bold");
        TimeText->SetFont(FontInfo);
        TimeText->SetColorAndOpacity(FSlateColor(FLinearColor(1.0f, 0.9f, 0.3f, 1.0f)));
    }

    // Shrink Button Fonts
    if (PlayPauseButton)
    {
        if (UTextBlock* BtnText = Cast<UTextBlock>(PlayPauseButton->GetChildAt(0)))
        {
            FSlateFontInfo FontInfo = BtnText->GetFont();
            FontInfo.Size = 14;
            FontInfo.TypefaceFontName = TEXT("Bold");
            BtnText->SetFont(FontInfo);
        }
    }

    if (ResetButton)
    {
        if (UTextBlock* BtnText = Cast<UTextBlock>(ResetButton->GetChildAt(0)))
        {
            FSlateFontInfo FontInfo = BtnText->GetFont();
            FontInfo.Size = 14;
            FontInfo.TypefaceFontName = TEXT("Bold");
            BtnText->SetFont(FontInfo);
        }
    }

    // Dynamic Top Bar Layout
    if (TimeText && PlayPauseButton && ResetButton && ArticulationSelector)
    {
        // Find the top bar horizontal box
        if (UHorizontalBox* TopBar = Cast<UHorizontalBox>(TimeText->GetParent()))
        {
            // Remove everything to rebuild cleanly
            TimeText->RemoveFromParent();
            PlayPauseButton->RemoveFromParent();
            ResetButton->RemoveFromParent();
            ArticulationSelector->RemoveFromParent();
            if (PossessButton) PossessButton->RemoveFromParent();

            TopBar->ClearChildren(); 

            // Add TimeText
            if (UHorizontalBoxSlot* HSlot = TopBar->AddChildToHorizontalBox(TimeText))
            {
                HSlot->SetPadding(FMargin(10, 5, 20, 5)); // Give text breathing room
                HSlot->SetVerticalAlignment(VAlign_Center);
            }

            // Both buttons in identically-sized SizeBoxes
            const float ButtonWidth = 90.0f;
            const float ButtonHeight = 30.0f;

            USizeBox* PlaySizeBox = NewObject<USizeBox>(this);
            PlaySizeBox->SetWidthOverride(ButtonWidth);
            PlaySizeBox->SetHeightOverride(ButtonHeight);

            PlaySizeBox->AddChild(PlayPauseButton);
            if (UHorizontalBoxSlot* HSlot = TopBar->AddChildToHorizontalBox(PlaySizeBox))
            {
                HSlot->SetPadding(FMargin(0, 0, 5, 0));
                HSlot->SetVerticalAlignment(VAlign_Center);
            }

            USizeBox* ResetSizeBox = NewObject<USizeBox>(this);
            ResetSizeBox->SetWidthOverride(ButtonWidth);
            ResetSizeBox->SetHeightOverride(ButtonHeight);

            ResetSizeBox->AddChild(ResetButton);
            if (UHorizontalBoxSlot* HSlot = TopBar->AddChildToHorizontalBox(ResetSizeBox))
            {
                HSlot->SetPadding(FMargin(0, 0, 10, 0));
                HSlot->SetVerticalAlignment(VAlign_Center);
            }

            // Add a Spacer to eat up middle room
            USpacer* TopSpacer = NewObject<USpacer>(this);
            if (UHorizontalBoxSlot* HSlot = TopBar->AddChildToHorizontalBox(TopSpacer))
            {
                HSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
            }

            // Label for articulation selector
            UTextBlock* SelectorLabel = NewObject<UTextBlock>(this);
            SelectorLabel->SetText(FText::FromString(TEXT("Articulation:")));
            SelectorLabel->SetColorAndOpacity(FSlateColor(FLinearColor::White));
            if (UHorizontalBoxSlot* LabelSlot = TopBar->AddChildToHorizontalBox(SelectorLabel))
            {
                LabelSlot->SetPadding(FMargin(10, 5, 2, 5));
                LabelSlot->SetVerticalAlignment(VAlign_Center);
            }

            if (UHorizontalBoxSlot* HSlot = TopBar->AddChildToHorizontalBox(ArticulationSelector))
            {
                HSlot->SetPadding(FMargin(10, 5, 5, 5));
                HSlot->SetVerticalAlignment(VAlign_Center);
            }

            // Possess button next to selector
            if (PossessButton)
            {
                USizeBox* PossessSizeBox = NewObject<USizeBox>(this);
                PossessSizeBox->SetWidthOverride(ButtonWidth);
                PossessSizeBox->SetHeightOverride(ButtonHeight);
                PossessSizeBox->AddChild(PossessButton);
                if (UHorizontalBoxSlot* HSlot = TopBar->AddChildToHorizontalBox(PossessSizeBox))
                {
                    HSlot->SetPadding(FMargin(0, 0, 10, 0));
                    HSlot->SetVerticalAlignment(VAlign_Center);
                }
            }
        }
    }

    if (PlayPauseButton)
    {
        PlayPauseButton->OnClicked.AddDynamic(this, &UMjSimulateWidget::OnPlayPauseClicked);
    }

    if (ResetButton)
    {
        ResetButton->OnClicked.AddDynamic(this, &UMjSimulateWidget::OnResetClicked);
    }

    if (RecordButton)
    {
        RecordButton->OnClicked.AddDynamic(this, &UMjSimulateWidget::OnRecordClicked);
    }

    if (ReplayButton)
    {
        ReplayButton->OnClicked.AddDynamic(this, &UMjSimulateWidget::OnReplayClicked);
    }

    if (SnapshotButton)
    {
        SnapshotButton->OnClicked.AddDynamic(this, &UMjSimulateWidget::OnSnapshotClicked);
    }

    if (RestoreButton)
    {
        RestoreButton->OnClicked.AddDynamic(this, &UMjSimulateWidget::OnRestoreClicked);
    }

    if (PossessButton)
    {
        StyleButton(PossessButton, FLinearColor(0.1f, 0.6f, 0.6f, 0.9f));
        if (UTextBlock* BtnText = Cast<UTextBlock>(PossessButton->GetChildAt(0)))
        {
            BtnText->SetText(FText::FromString(TEXT("Possess")));
            FSlateFontInfo BtnFont = BtnText->GetFont();
            BtnFont.Size = 9;
            BtnText->SetFont(BtnFont);
            BtnText->SetAutoWrapText(false);
            BtnText->SetJustification(ETextJustify::Center);
        }
        PossessButton->OnClicked.AddDynamic(this, &UMjSimulateWidget::OnPossessClicked);
    }

    if (ArticulationSelector)
    {
        ArticulationSelector->OnSelectionChanged.AddDynamic(this, &UMjSimulateWidget::OnArticulationSelected);
        FTableRowStyle RowStyle = ArticulationSelector->GetItemStyle();
        FSlateColor RowBG(FLinearColor(0.15f, 0.15f, 0.18f, 1.0f));
        FSlateColor RowHover(FLinearColor(0.25f, 0.30f, 0.35f, 1.0f));
        RowStyle.SetEvenRowBackgroundBrush(FSlateRoundedBoxBrush(RowBG, 0.0f));
        RowStyle.SetOddRowBackgroundBrush(FSlateRoundedBoxBrush(RowBG, 0.0f));
        RowStyle.SetEvenRowBackgroundHoveredBrush(FSlateRoundedBoxBrush(RowHover, 0.0f));
        RowStyle.SetOddRowBackgroundHoveredBrush(FSlateRoundedBoxBrush(RowHover, 0.0f));
        ArticulationSelector->SetItemStyle(RowStyle);
    }

    // Attempt to find manager if not initialized
    if (!ManagerRef)
    {
        SetupDashboard(AAMjManager::GetManager());
    }
}

void UMjSimulateWidget::SetupDashboard(AAMjManager* InManager)
{
    ManagerRef = InManager;
    if (!ManagerRef) return;

    PopulateManagerSettings();

    // Populate Articulation Selector
    if (ArticulationSelector)
    {
        ArticulationSelector->ClearOptions();
        TArray<AMjArticulation*> Articulations = ManagerRef->GetAllArticulations();
        for (AMjArticulation* Art : Articulations)
        {
            if (Art->bAttachFailed) continue;
            ArticulationSelector->AddOption(MjUtils::PrettifyName(Art->GetName()));
        }
        if (Articulations.Num() > 0)
        {
            ArticulationSelector->SetSelectedIndex(0);
        }
        else
        {
            // If no articulations found, we still need to build the base UI (Physics, Visuals, Replay etc)
            RefreshArticulationControls();
        }
    }
}

void UMjSimulateWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);

    if (!ManagerRef)
    {
        SetupDashboard(AAMjManager::GetManager());
        if (!ManagerRef) return;

        // Start with UI hidden — user presses Tab to show
        if (!bIsMouseEnabled)
        {
            if (UWidget* RootCanvas = Cast<UWidget>(GetRootWidget()))
            {
                RootCanvas->SetVisibility(ESlateVisibility::Hidden);
            }
            if (APlayerController* PC = GetOwningPlayer())
            {
                PC->bShowMouseCursor = false;
                PC->SetInputMode(FInputModeGameOnly());
            }
        }
    }

    if (TimeText)
    {
        // Show replay time if replaying, otherwise sim time
        AMjReplayManager* ReplayMgr = Cast<AMjReplayManager>(
            UGameplayStatics::GetActorOfClass(GetWorld(), AMjReplayManager::StaticClass()));
        if (ReplayMgr && ReplayMgr->bIsReplaying)
        {
            TArray<FMjReplayFrame>& Frames = ReplayMgr->Sessions.FindOrAdd(ReplayMgr->ActiveSessionName).Frames;
            float Duration = Frames.Num() > 0 ? Frames.Last().Timestamp - Frames[0].Timestamp : 0.0f;
            TimeText->SetText(FText::FromString(FString::Printf(TEXT("REPLAY: %.2f / %.2f s  [%s]"),
                ReplayMgr->PlaybackTime, Duration, *ReplayMgr->ActiveSessionName)));
        }
        else
        {
            TimeText->SetText(FText::FromString(FString::Printf(TEXT("Time: %.3f s"), ManagerRef->GetSimTime())));
        }
    }

    // Locomotion section is now built inline in RefreshArticulationControls

    // Update Play/Pause Button Text based on real manager state
    if (PlayPauseButton)
    {
        if (UTextBlock* BtnText = Cast<UTextBlock>(PlayPauseButton->GetChildAt(0)))
        {
            bool bPaused = ManagerRef->PhysicsEngine ? ManagerRef->PhysicsEngine->bIsPaused : true;
            BtnText->SetText(FText::FromString(bPaused ? TEXT("Play") : TEXT("Pause")));
        }
    }

    // Update record/replay button text based on ReplayManager state
    {
        AMjReplayManager* ReplayMgr = Cast<AMjReplayManager>(
            UGameplayStatics::GetActorOfClass(GetWorld(), AMjReplayManager::StaticClass()));

        if (RecordButton)
        {
            if (UTextBlock* BtnText = Cast<UTextBlock>(RecordButton->GetChildAt(0)))
            {
                bool bRecording = ReplayMgr && ReplayMgr->bIsRecording;
                BtnText->SetText(FText::FromString(bRecording ? TEXT("Stop Recording") : TEXT("Record")));
            }
        }

        if (ReplayButton)
        {
            if (UTextBlock* BtnText = Cast<UTextBlock>(ReplayButton->GetChildAt(0)))
            {
                bool bReplaying = ReplayMgr && ReplayMgr->bIsReplaying;
                BtnText->SetText(FText::FromString(bReplaying ? TEXT("Stop Replay") : TEXT("Replay")));
            }
        }
    }

    if (SnapshotButton)
    {
        if (UTextBlock* BtnText = Cast<UTextBlock>(SnapshotButton->GetChildAt(0)))
        {
            BtnText->SetText(FText::FromString(TEXT("Snapshot")));
        }
    }

    if (RestoreButton)
    {
        if (UTextBlock* BtnText = Cast<UTextBlock>(RestoreButton->GetChildAt(0)))
        {
            BtnText->SetText(FText::FromString(TEXT("Restore")));
        }
    }

    // Refresh session dropdown and binding UI if session count or bindings changed
    if (ReplaySessionSelector)
    {
        AMjReplayManager* ReplayMgr = Cast<AMjReplayManager>(
            UGameplayStatics::GetActorOfClass(GetWorld(), AMjReplayManager::StaticClass()));
        if (ReplayMgr)
        {
            bool bNeedsRefresh = false;
            if (ReplayMgr->GetSessionCount() != CachedSessionCount)
            {
                bNeedsRefresh = true;
            }
            // Check if binding count changed (e.g. after CSV load)
            if (ReplayMgr->GetArticulationBindings().Num() != ReplayEnabledCheckBoxes.Num())
            {
                bNeedsRefresh = true;
            }
            if (bNeedsRefresh)
            {
                RefreshArticulationControls();
            }
        }
    }

    // Toggle Input Mode via Tab key
    if (APlayerController* PC = GetOwningPlayer())
    {
        if (PC->WasInputKeyJustPressed(EKeys::Tab))
        {
            bIsMouseEnabled = !bIsMouseEnabled;
            PC->bShowMouseCursor = bIsMouseEnabled;
            
            // We only hide the root inner panel of the UI, rather than the entire Widget itself.
            // If we hide the whole Widget, NativeTick stops firing and Tab can never wake it back up!
            if (UWidget* RootCanvas = Cast<UWidget>(GetRootWidget()))
            {
                if (bIsMouseEnabled)
                {
                    RootCanvas->SetVisibility(ESlateVisibility::Visible);
                    FInputModeGameAndUI InputMode;
                    InputMode.SetWidgetToFocus(TakeWidget());
                    PC->SetInputMode(InputMode);
                }
                else
                {
                    RootCanvas->SetVisibility(ESlateVisibility::Hidden);
                    FInputModeGameOnly InputMode;
                    PC->SetInputMode(InputMode);
                }
            }
        }
    }

    // Periodically update monitor values
    UpdateMonitorValues();

    // Update live camera feed thumbnails
    for (UMjCameraFeedEntry* Feed : ActiveCameraFeeds)
    {
        if (Feed) Feed->UpdateFeed();
    }
}

void UMjSimulateWidget::PopulateManagerSettings()
{
    if (!PropertyRowClass) { UE_LOG(LogURLab, Warning, TEXT("MjSimulateWidget: PropertyRowClass is NOT SET in BP Class Defaults.")); return; }
    if (!ManagerRef) return;

    if (ManagerSettingsList)
    {
        ManagerSettingsList->ClearChildren();
        ManagerSettingsList->SetVisibility(ESlateVisibility::Visible); // Re-show left panel
    }
}

void UMjSimulateWidget::OnPlayPauseClicked()
{
    if (ManagerRef)
    {
        if (ManagerRef->PhysicsEngine) ManagerRef->PhysicsEngine->SetPaused(!ManagerRef->PhysicsEngine->bIsPaused);
    }
}

void UMjSimulateWidget::OnResetClicked()
{
    if (ManagerRef)
    {
        ManagerRef->ResetSimulation();
    }
}

void UMjSimulateWidget::OnRecordClicked()
{
    if (!ManagerRef) return;
    AMjReplayManager* ReplayMgr = Cast<AMjReplayManager>(
        UGameplayStatics::GetActorOfClass(GetWorld(), AMjReplayManager::StaticClass()));
    if (!ReplayMgr) return;

    if (ReplayMgr->bIsRecording)
    {
        ManagerRef->StopRecording();
    }
    else
    {
        ManagerRef->StartRecording();
    }
}

void UMjSimulateWidget::OnReplayClicked()
{
    if (!ManagerRef) return;
    AMjReplayManager* ReplayMgr = Cast<AMjReplayManager>(
        UGameplayStatics::GetActorOfClass(GetWorld(), AMjReplayManager::StaticClass()));
    if (!ReplayMgr) return;

    if (ReplayMgr->bIsReplaying)
    {
        ManagerRef->StopReplay();
    }
    else
    {
        ManagerRef->StartReplay();
    }
}

void UMjSimulateWidget::OnSnapshotClicked()
{
    if (ManagerRef)
    {
        LastSnapshot = ManagerRef->CaptureSnapshot();
    }
}

void UMjSimulateWidget::OnRestoreClicked()
{
    if (ManagerRef && LastSnapshot)
    {
        ManagerRef->RestoreSnapshot(LastSnapshot);
    }
}

void UMjSimulateWidget::OnReplaySessionSelected(FString SelectedItem, ESelectInfo::Type SelectionType)
{
    if (!ManagerRef) return;
    AMjReplayManager* ReplayMgr = Cast<AMjReplayManager>(
        UGameplayStatics::GetActorOfClass(GetWorld(), AMjReplayManager::StaticClass()));
    if (ReplayMgr)
    {
        ReplayMgr->SetActiveSession(SelectedItem);
    }
}

void UMjSimulateWidget::OnLoadCSVClicked()
{
    UE_LOG(LogURLab, Log, TEXT("MjSimulateWidget: Load Replay clicked"));
    if (!ManagerRef)
    {
        UE_LOG(LogURLab, Warning, TEXT("MjSimulateWidget: No ManagerRef"));
        return;
    }
    AMjReplayManager* ReplayMgr = Cast<AMjReplayManager>(
        UGameplayStatics::GetActorOfClass(GetWorld(), AMjReplayManager::StaticClass()));
    if (!ReplayMgr)
    {
        UE_LOG(LogURLab, Warning, TEXT("MjSimulateWidget: No AMjReplayManager in scene! Add one to the level."));
        return;
    }
    if (ReplayMgr->BrowseAndLoadCSV())
    {
        RefreshReplaySessionDropdown();
        if (ReplaySessionSelector)
        {
            ReplaySessionSelector->SetSelectedOption(ReplayMgr->GetActiveSessionName());
        }
    }
}

void UMjSimulateWidget::HandleReplayBindingEnabledChanged(bool bIsChecked)
{
    AMjReplayManager* ReplayMgr = Cast<AMjReplayManager>(
        UGameplayStatics::GetActorOfClass(GetWorld(), AMjReplayManager::StaticClass()));
    if (!ReplayMgr) return;

    // Find which checkbox triggered this by checking all stored checkboxes
    for (int32 i = 0; i < ReplayEnabledCheckBoxes.Num(); ++i)
    {
        if (ReplayEnabledCheckBoxes[i] && ReplayEnabledCheckBoxes[i]->IsChecked() != ReplayMgr->GetArticulationBindings()[i].bEnabled)
        {
            if (i < ReplayMgr->GetArticulationBindings().Num())
            {
                ReplayMgr->GetArticulationBindings()[i].bEnabled = ReplayEnabledCheckBoxes[i]->IsChecked();
                UE_LOG(LogURLab, Log, TEXT("Replay binding '%s' Enabled=%d"),
                    *ReplayMgr->GetArticulationBindings()[i].Articulation->GetName(),
                    ReplayMgr->GetArticulationBindings()[i].bEnabled);
            }
        }
    }
}

void UMjSimulateWidget::HandleReplayBindingRelPosChanged(bool bIsChecked)
{
    AMjReplayManager* ReplayMgr = Cast<AMjReplayManager>(
        UGameplayStatics::GetActorOfClass(GetWorld(), AMjReplayManager::StaticClass()));
    if (!ReplayMgr) return;

    for (int32 i = 0; i < ReplayRelPosCheckBoxes.Num(); ++i)
    {
        if (ReplayRelPosCheckBoxes[i] && ReplayRelPosCheckBoxes[i]->IsChecked() != ReplayMgr->GetArticulationBindings()[i].bRelativePosition)
        {
            if (i < ReplayMgr->GetArticulationBindings().Num())
            {
                ReplayMgr->GetArticulationBindings()[i].bRelativePosition = ReplayRelPosCheckBoxes[i]->IsChecked();
                UE_LOG(LogURLab, Log, TEXT("Replay binding '%s' RelPos=%d"),
                    *ReplayMgr->GetArticulationBindings()[i].Articulation->GetName(),
                    ReplayMgr->GetArticulationBindings()[i].bRelativePosition);
            }
        }
    }
}

void UMjSimulateWidget::OnSaveRecordingClicked()
{
    UE_LOG(LogURLab, Log, TEXT("MjSimulateWidget: Save Recording clicked"));
    if (!ManagerRef) return;
    AMjReplayManager* ReplayMgr = Cast<AMjReplayManager>(
        UGameplayStatics::GetActorOfClass(GetWorld(), AMjReplayManager::StaticClass()));
    if (!ReplayMgr)
    {
        UE_LOG(LogURLab, Warning, TEXT("MjSimulateWidget: No AMjReplayManager in scene!"));
        return;
    }
    ReplayMgr->BrowseAndSaveRecording();
}

void UMjSimulateWidget::RefreshReplaySessionDropdown()
{
    if (!ReplaySessionSelector) return;

    AMjReplayManager* ReplayMgr = Cast<AMjReplayManager>(
        UGameplayStatics::GetActorOfClass(GetWorld(), AMjReplayManager::StaticClass()));
    if (!ReplayMgr) return;

    FString CurrentSelection = ReplaySessionSelector->GetSelectedOption();
    ReplaySessionSelector->ClearOptions();

    TArray<FString> Names = ReplayMgr->GetSessionNames();
    for (const FString& Name : Names)
    {
        ReplaySessionSelector->AddOption(Name);
    }

    // Restore selection or default to active session
    if (Names.Contains(CurrentSelection))
    {
        ReplaySessionSelector->SetSelectedOption(CurrentSelection);
    }
    else
    {
        ReplaySessionSelector->SetSelectedOption(ReplayMgr->GetActiveSessionName());
    }

    CachedSessionCount = ReplayMgr->GetSessionCount();
}

void UMjSimulateWidget::RebuildReplayBindingUI(UVerticalBox* ReplayBox)
{
    if (!ReplayBox) return;

    AMjReplayManager* ReplayMgr = Cast<AMjReplayManager>(
        UGameplayStatics::GetActorOfClass(GetWorld(), AMjReplayManager::StaticClass()));
    if (!ReplayMgr) return;

    TArray<FReplayArticulationBinding>& Bindings = ReplayMgr->GetArticulationBindings();
    if (Bindings.Num() == 0) return;

    // Header
    UTextBlock* HeaderLabel = NewObject<UTextBlock>(this);
    HeaderLabel->SetText(FText::FromString(TEXT("Articulations:")));
    FSlateFontInfo HeaderFont = HeaderLabel->GetFont();
    HeaderFont.Size = 9;
    HeaderLabel->SetFont(HeaderFont);
    HeaderLabel->SetColorAndOpacity(FSlateColor(FLinearColor(0.7f, 0.7f, 0.7f)));
    ReplayBox->AddChildToVerticalBox(HeaderLabel)->SetPadding(FMargin(0, 8, 0, 2));

    for (int32 Idx = 0; Idx < Bindings.Num(); ++Idx)
    {
        FReplayArticulationBinding& Binding = Bindings[Idx];
        if (!Binding.Articulation.IsValid()) continue;

        UHorizontalBox* Row = NewObject<UHorizontalBox>(this);

        // Enabled checkbox
        UCheckBox* EnabledCB = NewObject<UCheckBox>(this);
        EnabledCB->SetIsChecked(Binding.bEnabled);
        EnabledCB->OnCheckStateChanged.AddDynamic(this, &UMjSimulateWidget::HandleReplayBindingEnabledChanged);
        // Tag the checkbox with the binding index via its tooltip (hacky but simple)
        Row->AddChildToHorizontalBox(EnabledCB)->SetPadding(FMargin(0, 0, 4, 0));

        UTextBlock* EnabledLabel = NewObject<UTextBlock>(this);
        EnabledLabel->SetText(FText::FromString(TEXT("On")));
        FSlateFontInfo SmallFont = EnabledLabel->GetFont();
        SmallFont.Size = 8;
        EnabledLabel->SetFont(SmallFont);
        EnabledLabel->SetColorAndOpacity(FSlateColor(FLinearColor(0.6f, 0.6f, 0.6f)));
        Row->AddChildToHorizontalBox(EnabledLabel)->SetPadding(FMargin(0, 0, 8, 0));

        // RelPos checkbox
        UCheckBox* RelPosCB = NewObject<UCheckBox>(this);
        RelPosCB->SetIsChecked(Binding.bRelativePosition);
        RelPosCB->OnCheckStateChanged.AddDynamic(this, &UMjSimulateWidget::HandleReplayBindingRelPosChanged);
        Row->AddChildToHorizontalBox(RelPosCB)->SetPadding(FMargin(0, 0, 4, 0));

        UTextBlock* RelPosLabel = NewObject<UTextBlock>(this);
        RelPosLabel->SetText(FText::FromString(TEXT("RelPos")));
        RelPosLabel->SetFont(SmallFont);
        RelPosLabel->SetColorAndOpacity(FSlateColor(FLinearColor(0.6f, 0.6f, 0.6f)));
        Row->AddChildToHorizontalBox(RelPosLabel)->SetPadding(FMargin(0, 0, 8, 0));

        // Articulation name
        FString DisplayName = MjUtils::PrettifyName(Binding.Articulation->GetName());
        UTextBlock* NameLabel = NewObject<UTextBlock>(this);
        NameLabel->SetText(FText::FromString(DisplayName));
        NameLabel->SetFont(SmallFont);
        NameLabel->SetColorAndOpacity(FSlateColor(FLinearColor::White));
        Row->AddChildToHorizontalBox(NameLabel);

        ReplayBox->AddChildToVerticalBox(Row)->SetPadding(FMargin(4, 2, 0, 2));

        // Store checkbox references for the handler to find the binding index
        // We use a simple approach: store them in parallel arrays
        ReplayEnabledCheckBoxes.Add(EnabledCB);
        ReplayRelPosCheckBoxes.Add(RelPosCB);
    }
}

void UMjSimulateWidget::OnIntegratorSelected(FString SelectedItem, ESelectInfo::Type SelectionType)
{
    if (!ManagerRef) return;

    if (!ManagerRef->PhysicsEngine) return;
    ManagerRef->PhysicsEngine->Options.bOverride_Integrator = true;
    if      (SelectedItem == TEXT("Euler"))        ManagerRef->PhysicsEngine->Options.Integrator = EMjIntegrator::Euler;
    else if (SelectedItem == TEXT("RK4"))           ManagerRef->PhysicsEngine->Options.Integrator = EMjIntegrator::RK4;
    else if (SelectedItem == TEXT("Implicit"))      ManagerRef->PhysicsEngine->Options.Integrator = EMjIntegrator::Implicit;
    else if (SelectedItem == TEXT("ImplicitFast"))  ManagerRef->PhysicsEngine->Options.Integrator = EMjIntegrator::ImplicitFast;

    ManagerRef->PhysicsEngine->ApplyOptions();
}

void UMjSimulateWidget::OnArticulationSelected(FString SelectedItem, ESelectInfo::Type SelectionType)
{
    if (!ManagerRef) return;

    // Camera feed cleanup is now handled in RefreshArticulationControls.
    // Find the articulation whose prettified name matches the selection.
    SelectedArticulation = nullptr;

    for (AMjArticulation* Art : ManagerRef->GetAllArticulations())
    {
        if (MjUtils::PrettifyName(Art->GetName()) == SelectedItem)
        {
            SelectedArticulation = Art;
            break;
        }
    }
    
    // Refresh manager settings to show the selected articulation's toggles
    PopulateManagerSettings();

    RefreshArticulationControls();
    RefreshKeyframeDropdown();
}

void UMjSimulateWidget::RefreshArticulationControls()
{
    if (!ArticulationControlList || !PropertyRowClass) 
    {
        UE_LOG(LogURLab, Warning, TEXT("MjSimulateWidget: Refresh failed. Class=%s"), 
            PropertyRowClass ? TEXT("Valid") : TEXT("NULL"));
        return;
    }

    // Teardown camera feeds before clearing panels
    for (UMjCameraFeedEntry* OldFeed : ActiveCameraFeeds)
    {
        if (OldFeed) OldFeed->UnbindCamera();
    }
    ActiveCameraFeeds.Empty();

    if (ManagerSettingsList)
    {
        // Safe-unlink our persistent buttons so they don't get destroyed by ClearChildren
        if (SnapshotButton) SnapshotButton->RemoveFromParent();
        if (RestoreButton) RestoreButton->RemoveFromParent();
        if (RecordButton) RecordButton->RemoveFromParent();
        if (ReplayButton) ReplayButton->RemoveFromParent();
        if (ReplaySessionSelector) ReplaySessionSelector->RemoveFromParent();
        if (LoadCSVButton) LoadCSVButton->RemoveFromParent();
        if (SaveRecordingButton) SaveRecordingButton->RemoveFromParent();
        if (KeyframeSelector) KeyframeSelector->RemoveFromParent();
        if (ResetToKeyframeButton) ResetToKeyframeButton->RemoveFromParent();
        if (HoldKeyframeButton) HoldKeyframeButton->RemoveFromParent();
        ReplayEnabledCheckBoxes.Empty();
        ReplayRelPosCheckBoxes.Empty();

        ManagerSettingsList->ClearChildren();
    }
    ArticulationControlList->ClearChildren();


    if (SelectedArticulation)
    {
        UE_LOG(LogURLab, Log, TEXT("MjSimulateWidget: Refreshing controls for %s"), *SelectedArticulation->GetName());
    }
    else
    {
        UE_LOG(LogURLab, Log, TEXT("MjSimulateWidget: Refreshing global controls (No articulation selected)"));
    }

    auto CreateSection = [&](UVerticalBox* ParentList, const FString& Title, UVerticalBox*& OutContentBox)
    {
        UExpandableArea* ExpArea = NewObject<UExpandableArea>(this);
        UTextBlock* HeaderText = NewObject<UTextBlock>(this);
        HeaderText->SetText(FText::FromString(Title));

        FSlateFontInfo FontInfo = HeaderText->GetFont();
        FontInfo.Size = 11;
        FontInfo.TypefaceFontName = TEXT("Bold");
        HeaderText->SetFont(FontInfo);
        HeaderText->SetColorAndOpacity(FSlateColor(FLinearColor(0.85f, 0.9f, 1.0f, 1.0f)));

        FName HeaderName(TEXT("Header"));
        FName BodyName(TEXT("Body"));

        ExpArea->SetContentForSlot(HeaderName, HeaderText);

        OutContentBox = NewObject<UVerticalBox>(this);
        ExpArea->SetContentForSlot(BodyName, OutContentBox);

        ExpArea->SetIsExpanded(true);

        // Style the expandable area with a visible border
        FExpandableAreaStyle AreaStyle = ExpArea->GetStyle();
        AreaStyle.CollapsedImage.TintColor = FSlateColor(FLinearColor(0.3f, 0.35f, 0.4f, 1.0f));
        AreaStyle.ExpandedImage.TintColor = FSlateColor(FLinearColor(0.3f, 0.35f, 0.4f, 1.0f));
        ExpArea->SetStyle(AreaStyle);

        UVerticalBoxSlot* BoxSlot = ParentList->AddChildToVerticalBox(ExpArea);
        if (BoxSlot)
        {
            BoxSlot->SetPadding(FMargin(0, 3, 0, 5));
            BoxSlot->SetHorizontalAlignment(HAlign_Fill);
        }
    };

    auto AddRow = [&](UVerticalBox* List, const FString& Name, float Initial, EMjPropertyType Type, bool bIsActuator, FVector2D Range = FVector2D(0.0f, 1.0f), bool bIsManagerOption = false, UObject* AssociatedObject = nullptr)
    {
        UMjPropertyRow* Row = CreateWidget<UMjPropertyRow>(this, PropertyRowClass);
        if (Row)
        {
            FString DisplayName = Name;
            if (AssociatedObject && !bIsManagerOption)
            {
                FString ArtName = SelectedArticulation ? SelectedArticulation->GetName() : TEXT("Global");
                DisplayName = MjUtils::PrettifyName(Name, ArtName);
            }

            Row->InitializeProperty(Name, Type, Initial, Range, DisplayName);
            if (bIsManagerOption)
            {
                Row->OnValueChanged.AddDynamic(this, &UMjSimulateWidget::HandleManagerOptionChanged);
            }
            else if (bIsActuator)
            {
                Row->SetControllable(true);
                Row->OnValueChanged.AddDynamic(this, &UMjSimulateWidget::HandleActuatorChanged);
            }
            
            if (AssociatedObject)
            {
                Row->SetAssociatedObject(AssociatedObject);
            }
            
            UVerticalBoxSlot* VerticalSlot = List->AddChildToVerticalBox(Row);
            if (VerticalSlot)
            {
                 VerticalSlot->SetPadding(Type == EMjPropertyType::Header ? FMargin(0, 8, 0, 4) : FMargin(0, 2, 0, 2));
                 VerticalSlot->SetHorizontalAlignment(HAlign_Fill);
            }
        }
    };

    // Manager / Global Settings (Left Panel)
    if (ManagerSettingsList)
    {
        UVerticalBox* PhysicsBox = nullptr;
        CreateSection(ManagerSettingsList, TEXT("PHYSICS OPTIONS"), PhysicsBox);

        // Integrator dropdown (above timestep)
        {
            UComboBoxString* IntegratorCombo = NewObject<UComboBoxString>(this);
            IntegratorCombo->AddOption(TEXT("Euler"));
            IntegratorCombo->AddOption(TEXT("RK4"));
            IntegratorCombo->AddOption(TEXT("Implicit"));
            IntegratorCombo->AddOption(TEXT("ImplicitFast"));
            if (ManagerRef->PhysicsEngine) IntegratorCombo->SetSelectedIndex((int)ManagerRef->PhysicsEngine->Options.Integrator);
            IntegratorCombo->OnSelectionChanged.AddDynamic(this, &UMjSimulateWidget::OnIntegratorSelected);
            {
                FTableRowStyle RowStyle = IntegratorCombo->GetItemStyle();
                FSlateColor RowBG(FLinearColor(0.15f, 0.15f, 0.18f, 1.0f));
                FSlateColor RowHover(FLinearColor(0.25f, 0.30f, 0.35f, 1.0f));
                RowStyle.SetEvenRowBackgroundBrush(FSlateRoundedBoxBrush(RowBG, 0.0f));
                RowStyle.SetOddRowBackgroundBrush(FSlateRoundedBoxBrush(RowBG, 0.0f));
                RowStyle.SetEvenRowBackgroundHoveredBrush(FSlateRoundedBoxBrush(RowHover, 0.0f));
                RowStyle.SetOddRowBackgroundHoveredBrush(FSlateRoundedBoxBrush(RowHover, 0.0f));
                IntegratorCombo->SetItemStyle(RowStyle);
            }
            PhysicsBox->AddChildToVerticalBox(IntegratorCombo);
        }

        UMjPhysicsEngine* PE = ManagerRef->PhysicsEngine;
        UMjDebugVisualizer* DV = ManagerRef->DebugVisualizer;
        UMjNetworkManager* NM = ManagerRef->NetworkManager;

        AddRow(PhysicsBox, TEXT("Timestep"), PE ? PE->Options.Timestep : 0.002f, EMjPropertyType::Slider, false, FVector2D(0.0001f, 0.05f), true);
        AddRow(PhysicsBox, TEXT("Iterations"), PE ? (float)PE->Options.Iterations : 50.0f, EMjPropertyType::Slider, false, FVector2D(5.0f, 200.0f), true);
        AddRow(PhysicsBox, TEXT("Sim Speed %"), PE ? PE->SimSpeedPercent : 100.0f, EMjPropertyType::Slider, false, FVector2D(5.0f, 100.0f), true);
        AddRow(PhysicsBox, TEXT("Debug Enabled"), (DV && DV->bShowDebug) ? 1.0f : 0.0f, EMjPropertyType::Toggle, false, FVector2D(0,1), true);
        AddRow(PhysicsBox, TEXT("Internal Control"), (PE && PE->ControlSource == EControlSource::UI) ? 1.0f : 0.0f, EMjPropertyType::Toggle, false, FVector2D(0,1), true);

        UVerticalBox* VisualsBox = nullptr;
        CreateSection(ManagerSettingsList, TEXT("VISUALS"), VisualsBox);
        AddRow(VisualsBox, TEXT("Global Artic. Collision"), (DV && DV->bGlobalDrawDebugCollision) ? 1.0f : 0.0f, EMjPropertyType::Toggle, false, FVector2D(0,1), true);
        AddRow(VisualsBox, TEXT("Global Artic. Group 3"), (DV && DV->bGlobalShowGroup3) ? 1.0f : 0.0f, EMjPropertyType::Toggle, false, FVector2D(0,1), true);
        AddRow(VisualsBox, TEXT("Global Quick Collision"), (DV && DV->bGlobalQuickConvertCollision) ? 1.0f : 0.0f, EMjPropertyType::Toggle, false, FVector2D(0,1), true);

        if (SelectedArticulation)
        {
            AddRow(VisualsBox, TEXT("Selected Collision"), SelectedArticulation->bDrawDebugCollision ? 1.0f : 0.0f, EMjPropertyType::Toggle, false, FVector2D(0,1), true);
            AddRow(VisualsBox, TEXT("Selected Joint Axes"), SelectedArticulation->bDrawDebugJoints ? 1.0f : 0.0f, EMjPropertyType::Toggle, false, FVector2D(0,1), true);
            AddRow(VisualsBox, TEXT("Selected Sites"), SelectedArticulation->bDrawDebugSites ? 1.0f : 0.0f, EMjPropertyType::Toggle, false, FVector2D(0,1), true);
            AddRow(VisualsBox, TEXT("Selected Group 3"), SelectedArticulation->bShowGroup3 ? 1.0f : 0.0f, EMjPropertyType::Toggle, false, FVector2D(0,1), true);
            AddRow(VisualsBox, TEXT("Selected Internal Ctrl"), SelectedArticulation->ControlSource == (uint8)EControlSource::UI ? 1.0f : 0.0f, EMjPropertyType::Toggle, false, FVector2D(0,1), true);
        }

        UVerticalBox* NetworkBox = nullptr;
        CreateSection(ManagerSettingsList, TEXT("NETWORK"), NetworkBox);
        AddRow(NetworkBox, TEXT("Enable All Cameras"), (NM && NM->bEnableAllCameras) ? 1.0f : 0.0f, EMjPropertyType::Toggle, false, FVector2D(0,1), true);

        // Snapshots moved dynamically to panel
        UVerticalBox* SnapshotBox = nullptr;
        CreateSection(ManagerSettingsList, TEXT("SNAPSHOTS"), SnapshotBox);
        if (SnapshotButton)
        {
            if (UVerticalBoxSlot* BoxSlot = SnapshotBox->AddChildToVerticalBox(SnapshotButton)) { BoxSlot->SetPadding(FMargin(0, 5, 0, 5)); }
        }
        if (RestoreButton)
        {
            if (UVerticalBoxSlot* BoxSlot = SnapshotBox->AddChildToVerticalBox(RestoreButton)) { BoxSlot->SetPadding(FMargin(0, 5, 0, 5)); }
        }

        // Keyframes section
        UVerticalBox* KeyframeBox = nullptr;
        CreateSection(ManagerSettingsList, TEXT("KEYFRAMES"), KeyframeBox);

        if (!KeyframeSelector)
        {
            KeyframeSelector = NewObject<UComboBoxString>(this);
            KeyframeSelector->OnSelectionChanged.AddDynamic(this, &UMjSimulateWidget::OnKeyframeSelected);

            // Style matching replay dropdown
            FTableRowStyle RowStyle = KeyframeSelector->GetItemStyle();
            FSlateColor RowBG(FLinearColor(0.15f, 0.15f, 0.18f, 1.0f));
            FSlateColor RowHover(FLinearColor(0.25f, 0.30f, 0.35f, 1.0f));
            RowStyle.SetEvenRowBackgroundBrush(FSlateRoundedBoxBrush(RowBG, 0.0f));
            RowStyle.SetOddRowBackgroundBrush(FSlateRoundedBoxBrush(RowBG, 0.0f));
            RowStyle.SetEvenRowBackgroundHoveredBrush(FSlateRoundedBoxBrush(RowHover, 0.0f));
            RowStyle.SetOddRowBackgroundHoveredBrush(FSlateRoundedBoxBrush(RowHover, 0.0f));
            KeyframeSelector->SetItemStyle(RowStyle);
        }
        RefreshKeyframeDropdown();
        if (UVerticalBoxSlot* BoxSlot = KeyframeBox->AddChildToVerticalBox(KeyframeSelector))
        {
            BoxSlot->SetPadding(FMargin(0, 5, 0, 5));
        }

        // Reset to Keyframe button
        if (!ResetToKeyframeButton)
        {
            ResetToKeyframeButton = NewObject<UButton>(this);
            UTextBlock* BtnText = NewObject<UTextBlock>(ResetToKeyframeButton);
            BtnText->SetText(FText::FromString(TEXT("Reset to Keyframe")));
            BtnText->SetColorAndOpacity(FSlateColor(FLinearColor::White));
            BtnText->SetJustification(ETextJustify::Center);
            FSlateFontInfo FontInfo = BtnText->GetFont();
            FontInfo.Size = 14;
            BtnText->SetFont(FontInfo);
            ResetToKeyframeButton->AddChild(BtnText);
            ResetToKeyframeButton->SetBackgroundColor(FLinearColor(0.1f, 0.4f, 0.8f, 1.0f));
            ResetToKeyframeButton->OnClicked.AddDynamic(this, &UMjSimulateWidget::HandleResetToKeyframe);
        }
        if (UVerticalBoxSlot* BoxSlot = KeyframeBox->AddChildToVerticalBox(ResetToKeyframeButton))
        {
            BoxSlot->SetPadding(FMargin(0, 5, 0, 5));
        }

        // Hold Keyframe button
        if (!HoldKeyframeButton)
        {
            HoldKeyframeButton = NewObject<UButton>(this);
            UTextBlock* BtnText = NewObject<UTextBlock>(HoldKeyframeButton);
            BtnText->SetText(FText::FromString(TEXT("Hold Keyframe")));
            BtnText->SetColorAndOpacity(FSlateColor(FLinearColor::White));
            BtnText->SetJustification(ETextJustify::Center);
            FSlateFontInfo FontInfo = BtnText->GetFont();
            FontInfo.Size = 14;
            BtnText->SetFont(FontInfo);
            HoldKeyframeButton->AddChild(BtnText);
            HoldKeyframeButton->SetBackgroundColor(FLinearColor(0.1f, 0.6f, 0.3f, 1.0f));
            HoldKeyframeButton->OnClicked.AddDynamic(this, &UMjSimulateWidget::HandleHoldKeyframe);
        }
        if (UVerticalBoxSlot* BoxSlot = KeyframeBox->AddChildToVerticalBox(HoldKeyframeButton))
        {
            BoxSlot->SetPadding(FMargin(0, 5, 0, 5));
        }

        // Replays moved dynamically to panel
        UVerticalBox* ReplayBox = nullptr;
        CreateSection(ManagerSettingsList, TEXT("REPLAY SYSTEM"), ReplayBox);

        // Session dropdown
        if (!ReplaySessionSelector)
        {
            ReplaySessionSelector = NewObject<UComboBoxString>(this);
            ReplaySessionSelector->OnSelectionChanged.AddDynamic(this, &UMjSimulateWidget::OnReplaySessionSelected);

            // Lighten the dropdown row backgrounds for readability
            FTableRowStyle RowStyle = ReplaySessionSelector->GetItemStyle();
            FSlateColor RowBG(FLinearColor(0.15f, 0.15f, 0.18f, 1.0f));
            FSlateColor RowHover(FLinearColor(0.25f, 0.30f, 0.35f, 1.0f));
            RowStyle.SetEvenRowBackgroundBrush(FSlateRoundedBoxBrush(RowBG, 0.0f));
            RowStyle.SetOddRowBackgroundBrush(FSlateRoundedBoxBrush(RowBG, 0.0f));
            RowStyle.SetEvenRowBackgroundHoveredBrush(FSlateRoundedBoxBrush(RowHover, 0.0f));
            RowStyle.SetOddRowBackgroundHoveredBrush(FSlateRoundedBoxBrush(RowHover, 0.0f));
            RowStyle.SetTextColor(FSlateColor(FLinearColor::White));
            RowStyle.SetSelectedTextColor(FSlateColor(FLinearColor::White));
            ReplaySessionSelector->SetItemStyle(RowStyle);
        }
        ReplayBox->AddChildToVerticalBox(ReplaySessionSelector)->SetPadding(FMargin(0, 5, 0, 5));
        RefreshReplaySessionDropdown();

        // Articulation binding checkboxes
        RebuildReplayBindingUI(ReplayBox);

        if (RecordButton)
        {
            if (UVerticalBoxSlot* BoxSlot = ReplayBox->AddChildToVerticalBox(RecordButton)) { BoxSlot->SetPadding(FMargin(0, 5, 0, 5)); }
        }
        if (ReplayButton)
        {
            if (UVerticalBoxSlot* BoxSlot = ReplayBox->AddChildToVerticalBox(ReplayButton)) { BoxSlot->SetPadding(FMargin(0, 5, 0, 5)); }
        }

        // Load Replay button
        if (!LoadCSVButton)
        {
            LoadCSVButton = NewObject<UButton>(this);
            UTextBlock* BtnLabel = NewObject<UTextBlock>(this);
            BtnLabel->SetText(FText::FromString(TEXT("Load Replay")));
            FSlateFontInfo Font = BtnLabel->GetFont();
            Font.Size = 10;
            BtnLabel->SetFont(Font);
            BtnLabel->SetColorAndOpacity(FSlateColor(FLinearColor::White));
            LoadCSVButton->AddChild(BtnLabel);

            FButtonStyle BtnStyle = LoadCSVButton->GetStyle();
            FLinearColor BtnColor(0.4f, 0.3f, 0.7f, 0.9f);
            BtnStyle.Normal.TintColor = FSlateColor(BtnColor);
            BtnStyle.Hovered.TintColor = FSlateColor(BtnColor * 1.2f);
            BtnStyle.Pressed.TintColor = FSlateColor(BtnColor * 0.8f);
            LoadCSVButton->SetStyle(BtnStyle);

            LoadCSVButton->OnClicked.AddDynamic(this, &UMjSimulateWidget::OnLoadCSVClicked);
        }
        ReplayBox->AddChildToVerticalBox(LoadCSVButton)->SetPadding(FMargin(0, 5, 0, 5));

        // Save Recording button
        if (!SaveRecordingButton)
        {
            SaveRecordingButton = NewObject<UButton>(this);
            UTextBlock* SaveLabel = NewObject<UTextBlock>(this);
            SaveLabel->SetText(FText::FromString(TEXT("Save Recording")));
            FSlateFontInfo SaveFont = SaveLabel->GetFont();
            SaveFont.Size = 10;
            SaveLabel->SetFont(SaveFont);
            SaveLabel->SetColorAndOpacity(FSlateColor(FLinearColor::White));
            SaveRecordingButton->AddChild(SaveLabel);

            FButtonStyle SaveStyle = SaveRecordingButton->GetStyle();
            FLinearColor SaveColor(0.2f, 0.5f, 0.3f, 0.9f);
            SaveStyle.Normal.TintColor = FSlateColor(SaveColor);
            SaveStyle.Hovered.TintColor = FSlateColor(SaveColor * 1.2f);
            SaveStyle.Pressed.TintColor = FSlateColor(SaveColor * 0.8f);
            SaveRecordingButton->SetStyle(SaveStyle);

            SaveRecordingButton->OnClicked.AddDynamic(this, &UMjSimulateWidget::OnSaveRecordingClicked);
        }
        ReplayBox->AddChildToVerticalBox(SaveRecordingButton)->SetPadding(FMargin(0, 5, 0, 5));
    }

    // --- Articulation-Specific Sections ---
    if (!SelectedArticulation) return;

    // Actuators
    TArray<UMjActuator*> Actuators = SelectedArticulation->GetActuators();
    if (Actuators.Num() > 0)
    {
        UVerticalBox* SecBox = nullptr;
        CreateSection(ArticulationControlList, TEXT("ACTUATORS"), SecBox);
        for (UMjActuator* Act : Actuators)
        {
            if (!Act) continue;
            FString ActName = Act->GetMjName();
            if (ActName.IsEmpty()) ActName = Act->GetName();
            
            FVector2D Range = Act->GetControlRange();
            AddRow(SecBox, ActName, Act->GetControl(), EMjPropertyType::Slider, true, Range, false, Act);
        }
    }

    // Monitors: Joints
    TArray<UMjJoint*> Joints = SelectedArticulation->GetJoints();
    if (Joints.Num() > 0)
    {
        UVerticalBox* SecBox = nullptr;
        CreateSection(ArticulationControlList, TEXT("JOINTS"), SecBox);
        for (UMjJoint* Joint : Joints)
        {
            if (!Joint) continue;
            FString JointName = Joint->GetMjName();
            if (JointName.IsEmpty()) JointName = Joint->GetName();
            
            AddRow(SecBox, JointName, Joint->GetPosition(), EMjPropertyType::LabelOnly, false, FVector2D(0,0), false, Joint);
        }
    }

    // Monitors: Sensors
    TArray<UMjSensor*> Sensors = SelectedArticulation->GetSensors();
    if (Sensors.Num() > 0)
    {
        UVerticalBox* SecBox = nullptr;
        CreateSection(ArticulationControlList, TEXT("SENSORS"), SecBox);
        for (UMjSensor* Sensor : Sensors)
        {
            if (!Sensor) continue;
            FString SensorName = Sensor->GetMjName();
            if (SensorName.IsEmpty()) SensorName = Sensor->GetName();
            
            AddRow(SecBox, SensorName, Sensor->GetScalarReading(), EMjPropertyType::LabelOnly, false, FVector2D(0,0), false, Sensor);
        }
    }

    UE_LOG(LogURLab, Log, TEXT("MjSimulateWidget: Added %d actuators, %d joints, %d sensors for %s"), 
        Actuators.Num(), Joints.Num(), Sensors.Num(), *SelectedArticulation->GetName());

    // Camera Feeds (Left Panel)
    if (CameraFeedEntryClass && ManagerSettingsList)
    {
        TArray<UMjCamera*> Cameras;
        SelectedArticulation->GetComponents<UMjCamera>(Cameras);

        if (Cameras.Num() > 0)
        {
            UVerticalBox* SecBox = nullptr;
            CreateSection(ManagerSettingsList, TEXT("CAMERAS"), SecBox);

            for (UMjCamera* Cam : Cameras)
            {
                UMjCameraFeedEntry* Entry = CreateWidget<UMjCameraFeedEntry>(this, CameraFeedEntryClass);
                if (Entry)
                {
                    Entry->BindToCamera(Cam);
                    UVerticalBoxSlot* VerticalSlot = SecBox->AddChildToVerticalBox(Entry);
                    if (VerticalSlot)
                    {
                        VerticalSlot->SetPadding(FMargin(0, 5, 0, 5));
                    }
                    ActiveCameraFeeds.Add(Entry);
                }
            }
        }

        UE_LOG(LogURLab, Log, TEXT("MjSimulateWidget: Added %d camera feeds for %s"),
            Cameras.Num(), *SelectedArticulation->GetName());
    }

    // Locomotion sliders
    if (ArticulationControlList && PropertyRowClass)
    {
        UMjTwistController* TC = SelectedArticulation->FindComponentByClass<UMjTwistController>();
        if (TC)
        {
            auto AddTwistRow = [&](UVerticalBox* List, const FString& Name, float Initial, FVector2D Range)
            {
                UMjPropertyRow* Row = CreateWidget<UMjPropertyRow>(this, PropertyRowClass);
                if (Row)
                {
                    Row->InitializeProperty(Name, EMjPropertyType::Slider, Initial, Range, Name);
                    Row->OnValueChanged.AddDynamic(this, &UMjSimulateWidget::HandleTwistOptionChanged);
                    Row->SetAssociatedObject(TC);
                    if (UVerticalBoxSlot* Slot = List->AddChildToVerticalBox(Row))
                    {
                        Slot->SetPadding(FMargin(0, 2, 0, 2));
                        Slot->SetHorizontalAlignment(HAlign_Fill);
                    }
                }
            };

            UVerticalBox* LocoBox = nullptr;
            CreateSection(ArticulationControlList, TEXT("LOCOMOTION"), LocoBox);
            AddTwistRow(LocoBox, TEXT("Max Forward Speed"), TC->MaxVx, FVector2D(0.0f, 2.0f));
            AddTwistRow(LocoBox, TEXT("Max Strafe Speed"), TC->MaxVy, FVector2D(0.0f, 1.0f));
            AddTwistRow(LocoBox, TEXT("Max Turn Rate"), TC->MaxYawRate, FVector2D(0.0f, 3.14f));
        }
    }

    // Force layout recalculation after content changes (camera feeds change panel widths)
    InvalidateLayoutAndVolatility();
}

void UMjSimulateWidget::UpdateMonitorValues()
{
    if (!SelectedArticulation || !ArticulationControlList) return;

    // We now have ExpandableAreas containing VerticalBoxes all inside ArticulationControlList
    TArray<UWidget*> TopLevelChildren = ArticulationControlList->GetAllChildren();
    for (UWidget* TopLevelChild : TopLevelChildren)
    {
        UExpandableArea* ExpArea = Cast<UExpandableArea>(TopLevelChild);
        if (ExpArea)
        {
            FName BodyName(TEXT("Body"));
            UVerticalBox* Box = Cast<UVerticalBox>(ExpArea->GetContentForSlot(BodyName));
            if (Box)
            {
                for (UWidget* Child : Box->GetAllChildren())
                {
                    UMjPropertyRow* Row = Cast<UMjPropertyRow>(Child);
                    if (Row && Row->GetPropertyType() != EMjPropertyType::Header)
                    {
                        FString Name = Row->GetPropertyName();
                        float Val = 0.0f;

                        // Identify if this row is an actuator (controllable)
                        bool bIsActuator = Row->IsControllable();

                        // 1. Manage Interactivity (Disable sliders/toggles in ZMQ mode)
                        if (bIsActuator)
                        {
                            Row->SetRowEnabled(ManagerRef->PhysicsEngine && ManagerRef->PhysicsEngine->ControlSource == EControlSource::UI);
                        }

                        // 2. Manage Value Updates (Skip if user is dragging)
                        if (Row->IsBeingDragged()) continue;

                        // Identify the source of the value
                        if (UObject* RawObj = Row->GetAssociatedObject())
                        {
                            if (UMjActuator* Act = Cast<UMjActuator>(RawObj))
                            {
                                Val = Act->GetControl();
                            }
                            else if (UMjJoint* Joint = Cast<UMjJoint>(RawObj))
                            {
                                Val = Joint->GetPosition();
                            }
                            else if (UMjSensor* Sensor = Cast<UMjSensor>(RawObj))
                            {
                                Val = Sensor->GetScalarReading();
                            }
                            else if (UMjTwistController* TC = Cast<UMjTwistController>(RawObj))
                            {
                                // Twist sliders are user-controlled, don't override their value
                                continue;
                            }
                        }
                        else
                        {
                            // Fallback to name-based lookup for general articulation properties
                            Val = SelectedArticulation->GetJointAngle(Name);
                            if (Val == 0.0f) Val = SelectedArticulation->GetSensorScalar(Name);
                        }
                        
                        Row->SetValue(Val);
                    }
                }
            }
        }
    }
}

void UMjSimulateWidget::HandleManagerOptionChanged(float NewValue, const FString& OptionName)
{
    if (!ManagerRef) return;

    UMjPhysicsEngine* PE = ManagerRef->PhysicsEngine;
    UMjDebugVisualizer* DV = ManagerRef->DebugVisualizer;
    UMjNetworkManager* NM = ManagerRef->NetworkManager;

    if (OptionName == TEXT("Timestep"))
    {
        if (PE) { PE->Options.bOverride_Timestep = true; PE->Options.Timestep = NewValue; }
    }
    else if (OptionName == TEXT("Iterations"))
    {
        if (PE) { PE->Options.bOverride_Iterations = true; PE->Options.Iterations = (int)NewValue; }
    }
    else if (OptionName == TEXT("Sim Speed %"))
    {
        if (PE) PE->SimSpeedPercent = NewValue;
    }
    else if (OptionName == TEXT("Debug Enabled"))
    {
        if (DV) DV->bShowDebug = (NewValue > 0.5f);
    }
    else if (OptionName == TEXT("Internal Control"))
    {
        uint8 NewSource = (NewValue > 0.5f ? 1 : 0); // 1 = UI, 0 = ZMQ
        if (PE) PE->ControlSource = (EControlSource)NewSource;
        // Apply to ALL articulations, not just the selected one
        for (AMjArticulation* Art : ManagerRef->GetAllArticulations())
        {
            if (Art) Art->ControlSource = NewSource;
        }
        UpdateMonitorValues(); // Force immediate UI refresh of interactivity state
    }
    else if (OptionName == TEXT("Global Artic. Collision"))
    {
        if (DV) { DV->bGlobalDrawDebugCollision = (NewValue > 0.5f); DV->UpdateAllGlobalVisibility(); }
    }
    else if (OptionName == TEXT("Global Artic. Group 3"))
    {
        if (DV) { DV->bGlobalShowGroup3 = (NewValue > 0.5f); DV->UpdateAllGlobalVisibility(); }
    }
    else if (OptionName == TEXT("Global Quick Collision"))
    {
        if (DV) { DV->bGlobalQuickConvertCollision = (NewValue > 0.5f); DV->UpdateAllGlobalVisibility(); }
    }
    else if (OptionName == TEXT("Enable All Cameras"))
    {
        if (NM) { NM->bEnableAllCameras = (NewValue > 0.5f); NM->UpdateCameraStreamingState(); }
    }
    else if (OptionName == TEXT("Selected Collision") && SelectedArticulation)
    {
        SelectedArticulation->bDrawDebugCollision = (NewValue > 0.5f);
    }
    else if (OptionName == TEXT("Selected Joint Axes") && SelectedArticulation)
    {
        SelectedArticulation->bDrawDebugJoints = (NewValue > 0.5f);
    }
    else if (OptionName == TEXT("Selected Sites") && SelectedArticulation)
    {
        SelectedArticulation->bDrawDebugSites = (NewValue > 0.5f);
    }
    else if (OptionName == TEXT("Selected Group 3") && SelectedArticulation)
    {
        SelectedArticulation->bShowGroup3 = (NewValue > 0.5f);
        SelectedArticulation->UpdateGroup3Visibility();
    }
    else if (OptionName == TEXT("Selected Internal Ctrl") && SelectedArticulation)
    {
        SelectedArticulation->ControlSource = (NewValue > 0.5f) ? (uint8)EControlSource::UI : (uint8)EControlSource::ZMQ;
        UE_LOG(LogURLab, Log, TEXT("Set '%s' control source to %s"),
            *SelectedArticulation->GetName(),
            (NewValue > 0.5f) ? TEXT("Internal (UI)") : TEXT("External (ZMQ)"));
        UpdateMonitorValues();
    }

    // Locomotion twist settings
    if (OptionName == TEXT("Max Forward Speed") || OptionName == TEXT("Max Strafe Speed") || OptionName == TEXT("Max Turn Rate"))
    {
        HandleTwistOptionChanged(NewValue, OptionName);
    }

    // Only apply physics options when physics-related settings change
    if ((OptionName == TEXT("Timestep") || OptionName == TEXT("Iterations")) && PE)
    {
        PE->ApplyOptions();
    }
}

void UMjSimulateWidget::HandleActuatorChanged(float NewValue, const FString& OptionName)
{
    if (!SelectedArticulation) return;

    SelectedArticulation->SetActuatorControl(OptionName, NewValue);
}

void UMjSimulateWidget::OnPossessClicked()
{
    if (!SelectedArticulation) return;

    APlayerController* PC = GetOwningPlayer();
    if (!PC) return;

    if (!bIsPossessing)
    {
        OriginalPawn = PC->GetPawn();
        PC->Possess(SelectedArticulation);
        bIsPossessing = true;

        if (PossessButton)
        {
            if (UTextBlock* BtnText = Cast<UTextBlock>(PossessButton->GetChildAt(0)))
            {
                BtnText->SetText(FText::FromString(TEXT("Release")));
            }
        }

        UE_LOG(LogURLab, Log, TEXT("Possessed articulation: %s"), *SelectedArticulation->GetName());
    }
    else
    {
        PC->UnPossess();
        if (OriginalPawn)
        {
            PC->Possess(OriginalPawn);
        }
        bIsPossessing = false;
        OriginalPawn = nullptr;

        if (PossessButton)
        {
            if (UTextBlock* BtnText = Cast<UTextBlock>(PossessButton->GetChildAt(0)))
            {
                BtnText->SetText(FText::FromString(TEXT("Possess")));
            }
        }

        UE_LOG(LogURLab, Log, TEXT("Released articulation"));
    }
}

void UMjSimulateWidget::HandleTwistOptionChanged(float NewValue, const FString& OptionName)
{
    if (!SelectedArticulation)
    {
        UE_LOG(LogURLab, Warning, TEXT("HandleTwistOptionChanged: No SelectedArticulation"));
        return;
    }

    UMjTwistController* TwistCtrl = SelectedArticulation->FindComponentByClass<UMjTwistController>();
    if (!TwistCtrl)
    {
        UE_LOG(LogURLab, Warning, TEXT("HandleTwistOptionChanged: No TwistController on '%s'"), *SelectedArticulation->GetName());
        return;
    }

    if (OptionName == TEXT("Max Forward Speed"))
    {
        TwistCtrl->MaxVx = NewValue;
    }
    else if (OptionName == TEXT("Max Strafe Speed"))
    {
        TwistCtrl->MaxVy = NewValue;
    }
    else if (OptionName == TEXT("Max Turn Rate"))
    {
        TwistCtrl->MaxYawRate = NewValue;
    }

    UE_LOG(LogURLab, Log, TEXT("Twist option '%s' = %.3f"), *OptionName, NewValue);
}

void UMjSimulateWidget::OnKeyframeSelected(FString SelectedItem, ESelectInfo::Type SelectionType)
{
    // Just stores the selection — ResetToKeyframe/HoldKeyframe use it
    UE_LOG(LogURLab, Verbose, TEXT("Keyframe selected: %s"), *SelectedItem);
}

void UMjSimulateWidget::RefreshKeyframeDropdown()
{
    if (!KeyframeSelector) return;

    FString CurrentSelection = KeyframeSelector->GetSelectedOption();
    KeyframeSelector->ClearOptions();

    if (SelectedArticulation)
    {
        TArray<FString> Names = SelectedArticulation->GetKeyframeNames();
        for (const FString& Name : Names)
        {
            KeyframeSelector->AddOption(Name);
        }
        if (Names.Num() > 0)
        {
            if (Names.Contains(CurrentSelection))
                KeyframeSelector->SetSelectedOption(CurrentSelection);
            else
                KeyframeSelector->SetSelectedOption(Names[0]);
        }
    }
}

void UMjSimulateWidget::HandleResetToKeyframe()
{
    if (!SelectedArticulation || !KeyframeSelector) return;
    FString KeyframeName = KeyframeSelector->GetSelectedOption();
    SelectedArticulation->ResetToKeyframe(KeyframeName);
}

void UMjSimulateWidget::HandleHoldKeyframe()
{
    if (!SelectedArticulation) return;

    if (SelectedArticulation->IsHoldingKeyframe())
    {
        SelectedArticulation->StopHoldKeyframe();

        // Update button to "Hold Keyframe" (green)
        if (HoldKeyframeButton)
        {
            if (UTextBlock* Txt = Cast<UTextBlock>(HoldKeyframeButton->GetChildAt(0)))
                Txt->SetText(FText::FromString(TEXT("Hold Keyframe")));
            HoldKeyframeButton->SetBackgroundColor(FLinearColor(0.1f, 0.6f, 0.3f, 1.0f));
        }
    }
    else
    {
        if (!KeyframeSelector) return;
        FString KeyframeName = KeyframeSelector->GetSelectedOption();
        SelectedArticulation->HoldKeyframe(KeyframeName);

        // Update button to "Stop Hold" (red)
        if (HoldKeyframeButton)
        {
            if (UTextBlock* Txt = Cast<UTextBlock>(HoldKeyframeButton->GetChildAt(0)))
                Txt->SetText(FText::FromString(TEXT("Stop Hold")));
            HoldKeyframeButton->SetBackgroundColor(FLinearColor(0.6f, 0.2f, 0.2f, 1.0f));
        }
    }
}
