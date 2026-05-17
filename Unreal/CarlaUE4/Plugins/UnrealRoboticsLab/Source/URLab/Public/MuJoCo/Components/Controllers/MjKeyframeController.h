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
#include "Components/ActorComponent.h"
#include "MjKeyframeController.generated.h"

class AMjArticulation;

/**
 * @struct FMjKeyframePose
 * @brief A named pose with joint targets and a hold duration.
 */
USTRUCT(BlueprintType)
struct FMjKeyframePose
{
    GENERATED_BODY()

    /** Display name for this pose (e.g., "Home", "Reach", "Grasp"). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString Name = TEXT("Pose");

    /** Actuator target values, in order matching the articulation's actuator list. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<float> Targets;

    /** How long to hold this pose before transitioning to the next (seconds). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0.1"))
    float HoldTime = 2.0f;

    /** Time to interpolate from the previous pose to this one (seconds). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0.0"))
    float BlendTime = 1.0f;
};

/**
 * @class UMjKeyframeController
 * @brief Cycles through a list of keyframe poses on an MjArticulation.
 *
 * Add this component to any actor, set the TargetArticulation reference,
 * define Keyframes in the Details panel, and hit Play. The component
 * lerps between poses automatically.
 *
 * Works for any robot — Franka Panda, UR5e, or anything with actuators.
 */
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class URLAB_API UMjKeyframeController : public UActorComponent
{
    GENERATED_BODY()

public:
    UMjKeyframeController();

    virtual void BeginPlay() override;
    virtual void TickComponent(float DeltaTime, ELevelTick TickType,
        FActorComponentTickFunction* ThisTickFunction) override;

    // --- Configuration ---

    /** List of poses to cycle through. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MuJoCo|Keyframes")
    TArray<FMjKeyframePose> Keyframes;

    /** Load a built-in preset sequence. Overwrites the Keyframes array. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MuJoCo|Keyframes",
        meta = (GetOptions = "GetPresetNames"))
    FString Preset;

    /** Tick this to apply the selected Preset to the Keyframes array. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MuJoCo|Keyframes",
        meta = (DisplayName = "Load Preset (click to apply)"))
    bool bLoadPreset = false;

    /** If true, loops back to the first keyframe after the last. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MuJoCo|Keyframes")
    bool bLoop = true;

    /** If true, starts playing automatically on BeginPlay. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MuJoCo|Keyframes")
    bool bAutoPlay = true;

    /** Delay before starting playback (seconds). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MuJoCo|Keyframes",
        meta = (ClampMin = "0.0", EditCondition = "bAutoPlay"))
    float StartDelay = 0.5f;

    // --- Actions ---

    UFUNCTION(BlueprintCallable, Category = "MuJoCo|Keyframes")
    void Play();

    UFUNCTION(BlueprintCallable, Category = "MuJoCo|Keyframes")
    void Stop();

    UFUNCTION(BlueprintCallable, Category = "MuJoCo|Keyframes")
    void GoToKeyframe(int32 Index);

    /** Load a preset by name into the Keyframes array. */
    UFUNCTION(BlueprintCallable, Category = "MuJoCo|Keyframes")
    void LoadPreset(const FString& PresetName);

    /** Returns available preset names (for editor dropdown). */
    UFUNCTION()
    TArray<FString> GetPresetNames() const;

private:
    bool bPlaying = false;
    int32 CurrentKeyframe = 0;
    float PhaseTimer = 0.0f; // time within current keyframe (blend + hold)
    float DelayTimer = 0.0f;
    bool bDelayPending = false;

    UPROPERTY()
    AMjArticulation* OwnerArticulation = nullptr;

    TArray<FString> ActuatorNames; // cached actuator names in order

    AMjArticulation* ResolveOwnerArticulation();
    void CacheActuatorNames();
    void ApplyInterpolatedTargets(const TArray<float>& From, const TArray<float>& To, float Alpha);
};
