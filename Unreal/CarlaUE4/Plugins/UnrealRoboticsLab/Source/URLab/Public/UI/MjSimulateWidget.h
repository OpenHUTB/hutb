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
#include "Blueprint/UserWidget.h"
#include "MuJoCo/Core/AMjManager.h"
#include "MjSimulateWidget.generated.h"

class UVerticalBox;
class UTextBlock;
class UButton;
class UComboBoxString;
class UMjPropertyRow;
class UMjCameraFeedEntry;
class UCheckBox;

/**
 * @class UMjSimulateWidget
 * @brief Main dashboard mimicking MuJoCo's "simulate" UI.
 * 
 * Features:
 * - Top Bar: Time, Step, Real-time factor.
 * - Sidebars: Physics options, Articulation controls.
 */
UCLASS()
class URLAB_API UMjSimulateWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    /** @brief Initializes the widget with a reference to the manager. */
    UFUNCTION(BlueprintCallable, Category = "MuJoCo|UI")
    void SetupDashboard(AAMjManager* InManager);

    virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

protected:
    /** --- Widgets (BindWidget) --- */

    /** @brief Container for manager-wide physics settings. */
    UPROPERTY(meta = (BindWidget))
    UVerticalBox* ManagerSettingsList;

    /** @brief Container for the selected articulation's actuators. */
    UPROPERTY(meta = (BindWidget))
    UVerticalBox* ArticulationControlList;

    /** @brief Dropdown to select which articulation to control. */
    UPROPERTY(meta = (BindWidget))
    UComboBoxString* ArticulationSelector;

    /** @brief Text block for simulation time. */
    UPROPERTY(meta = (BindWidget))
    UTextBlock* TimeText;

    /** @brief Button to Play/Pause simulation. */
    UPROPERTY(meta = (BindWidget))
    UButton* PlayPauseButton;

    /** @brief Button to Reset simulation. */
    UPROPERTY(meta = (BindWidget))
    UButton* ResetButton;

    /** @brief Button to Start Recording. */
    UPROPERTY(meta = (BindWidget))
    UButton* RecordButton;

    /** @brief Button to Start Replay. */
    UPROPERTY(meta = (BindWidget))
    UButton* ReplayButton;

    /** @brief Button to Capture Simulation Snapshot. */
    UPROPERTY(meta = (BindWidget))
    UButton* SnapshotButton;

    /** @brief Button to Restore Simulation Snapshot. */
    UPROPERTY(meta = (BindWidget))
    UButton* RestoreButton;

    /** --- Blueprint Subclass Refs --- */

    /** @brief The MjPropertyRow class to spawn for each setting. */
    UPROPERTY(EditAnywhere, Category = "MuJoCo|UI")
    TSubclassOf<UMjPropertyRow> PropertyRowClass;

    /** @brief Blueprint class for a single camera feed entry row.
     *  Set this in BP class defaults to WBP_MjCameraFeedEntry. */
    UPROPERTY(EditAnywhere, Category = "MuJoCo|UI")
    TSubclassOf<UMjCameraFeedEntry> CameraFeedEntryClass;

    /** @brief Button to Possess/Release the selected articulation. */
    UPROPERTY(meta = (BindWidgetOptional))
    UButton* PossessButton;

    /** --- Internal State --- */

    UPROPERTY()
    AAMjManager* ManagerRef;

    UPROPERTY()
    AMjArticulation* SelectedArticulation;

    /** @brief Whether we're currently possessing an articulation. */
    bool bIsPossessing = false;

    /** @brief The pawn we were controlling before possession. */
    UPROPERTY()
    APawn* OriginalPawn = nullptr;

    /** @brief Cache of active camera feed entries for tick updates and teardown. */
    UPROPERTY()
    TArray<UMjCameraFeedEntry*> ActiveCameraFeeds;

    /** @brief The most recently captured simulation snapshot. */
    UPROPERTY()
    class UMjSimulationState* LastSnapshot;

    /** --- Handlers --- */

    UFUNCTION()
    void OnPlayPauseClicked();

    UFUNCTION()
    void OnResetClicked();

    UFUNCTION()
    void OnRecordClicked();

    UFUNCTION()
    void OnReplayClicked();

    UFUNCTION()
    void OnSnapshotClicked();

    UFUNCTION()
    void OnRestoreClicked();

    UFUNCTION()
    void OnArticulationSelected(FString SelectedItem, ESelectInfo::Type SelectionType);

    UFUNCTION()
    void OnIntegratorSelected(FString SelectedItem, ESelectInfo::Type SelectionType);

    /** @brief Fills the manager settings panel with rows. */
    void PopulateManagerSettings();

    /** @brief Updates the articulation controls and monitors when selection changes. */
    void RefreshArticulationControls();

    /** @brief Updates monitor values (Joints/Sensors) from public API. */
    void UpdateMonitorValues();

    /** @brief Specific handler for manager property changes. */
    UFUNCTION()
    void HandleManagerOptionChanged(float NewValue, const FString& OptionName);

    /** @brief Specific handler for actuator control changes. */
    UFUNCTION()
    void HandleActuatorChanged(float NewValue, const FString& ActuatorName);

    /** @brief Handles possess/release button click. */
    UFUNCTION()
    void OnPossessClicked();

    /** @brief Handles twist velocity slider changes. */
    UFUNCTION()
    void HandleTwistOptionChanged(float NewValue, const FString& OptionName);

    /** @brief Handles replay session dropdown selection. */
    UFUNCTION()
    void OnReplaySessionSelected(FString SelectedItem, ESelectInfo::Type SelectionType);

    /** @brief Handles Load CSV button click. */
    UFUNCTION()
    void OnLoadCSVClicked();

    /** @brief Handles Save Recording button click. */
    UFUNCTION()
    void OnSaveRecordingClicked();

    /** @brief Refreshes the replay session dropdown options. */
    void RefreshReplaySessionDropdown();

    /** @brief Rebuilds the articulation binding checkboxes in the replay section. */
    void RebuildReplayBindingUI(UVerticalBox* ReplayBox);

    virtual void NativeConstruct() override;

private:
    /** @brief Tracks whether the mouse is currently visible and allowed to interact with the UI. */
    bool bIsMouseEnabled = false;

    /** @brief Dropdown for selecting replay sessions (dynamically created). */
    UPROPERTY()
    UComboBoxString* ReplaySessionSelector = nullptr;

    /** @brief Load CSV button (dynamically created). */
    UPROPERTY()
    UButton* LoadCSVButton = nullptr;

    /** @brief Save Recording button (dynamically created). */
    UPROPERTY()
    UButton* SaveRecordingButton = nullptr;

    /** @brief Dropdown for selecting keyframes on the current articulation. */
    UPROPERTY()
    UComboBoxString* KeyframeSelector = nullptr;

    /** @brief Reset to Keyframe button. */
    UPROPERTY()
    UButton* ResetToKeyframeButton = nullptr;

    /** @brief Hold/Stop Hold toggle button. */
    UPROPERTY()
    UButton* HoldKeyframeButton = nullptr;

    /** @brief Handles keyframe dropdown selection. */
    UFUNCTION()
    void OnKeyframeSelected(FString SelectedItem, ESelectInfo::Type SelectionType);

    /** @brief Refreshes keyframe dropdown for the current articulation. */
    void RefreshKeyframeDropdown();

    /** @brief Handles Reset to Keyframe button click. */
    UFUNCTION()
    void HandleResetToKeyframe();

    /** @brief Handles Hold Keyframe button click. */
    UFUNCTION()
    void HandleHoldKeyframe();

    /** @brief Cached session count for detecting changes in NativeTick. */
    int32 CachedSessionCount = 0;

    /** @brief Parallel arrays of checkboxes for replay articulation bindings. */
    UPROPERTY()
    TArray<UCheckBox*> ReplayEnabledCheckBoxes;
    UPROPERTY()
    TArray<UCheckBox*> ReplayRelPosCheckBoxes;

    /** @brief Handles Enabled checkbox changes for replay bindings. */
    UFUNCTION()
    void HandleReplayBindingEnabledChanged(bool bIsChecked);

    /** @brief Handles RelPos checkbox changes for replay bindings. */
    UFUNCTION()
    void HandleReplayBindingRelPosChanged(bool bIsChecked);
};
