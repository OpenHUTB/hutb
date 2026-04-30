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
#include "GameFramework/Actor.h"
#include "CineCameraComponent.h"
#include "Components/SplineComponent.h"
#include "Components/BillboardComponent.h"
#include "MjKeyframeCameraActor.generated.h"

/**
 * @struct FMjCameraWaypoint
 * @brief A single camera keyframe: position, rotation, and time.
 */
USTRUCT(BlueprintType)
struct FMjCameraWaypoint
{
    GENERATED_BODY()

    /** World-space position. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FVector Position = FVector::ZeroVector;

    /** World-space rotation. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FRotator Rotation = FRotator::ZeroRotator;

    /** Time in seconds when the camera should arrive at this waypoint. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0.0"))
    float Time = 0.0f;
};

/**
 * @class AMjKeyframeCameraActor
 * @brief A cinematic camera that lerps through manually placed waypoints.
 *
 * Place this actor in the level, add waypoints to the Waypoints array,
 * and position each one using the transform gizmos. The camera smoothly
 * interpolates between them during PIE based on their Time values.
 *
 * Hotkey: O toggles play/pause (shared with orbit camera).
 */
UCLASS(ClassGroup=(Custom), meta=(DisplayName="MjKeyframeCamera"))
class URLAB_API AMjKeyframeCameraActor : public AActor
{
    GENERATED_BODY()

public:
    AMjKeyframeCameraActor();

protected:
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaTime) override;

#if WITH_EDITOR
    virtual void OnConstruction(const FTransform& Transform) override;
#endif

public:
    // --- Camera ---

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "MuJoCo|Keyframe Camera")
    UCineCameraComponent* CineCamera;

    // --- Waypoints ---

    /** @brief Camera waypoints. Each defines a position, rotation, and arrival time. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MuJoCo|Keyframe Camera|Path")
    TArray<FMjCameraWaypoint> Waypoints;

    /** @brief If true, loops back to the first waypoint after the last. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MuJoCo|Keyframe Camera|Path")
    bool bLoop = false;

    /** @brief If true, starts playing automatically on BeginPlay. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MuJoCo|Keyframe Camera|Path")
    bool bAutoPlay = true;

    /** @brief Delay before playback starts (seconds). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MuJoCo|Keyframe Camera|Path",
        meta = (ClampMin = "0.0", EditCondition = "bAutoPlay"))
    float StartDelay = 0.0f;

    /** @brief If true, activates this camera as the player's view on BeginPlay. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MuJoCo|Keyframe Camera|Path")
    bool bAutoActivate = true;

    /** @brief Use cubic interpolation for smoother motion. If false, uses linear lerp. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MuJoCo|Keyframe Camera|Path")
    bool bSmoothInterp = true;

    // --- Spline Preview ---

    /** @brief Shows the camera path as a spline in the editor. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "MuJoCo|Keyframe Camera|Preview")
    USplineComponent* PathSpline;

    // --- Controls ---

    UFUNCTION(BlueprintCallable, Category = "MuJoCo|Keyframe Camera")
    void Play();

    UFUNCTION(BlueprintCallable, Category = "MuJoCo|Keyframe Camera")
    void Pause();

    UFUNCTION(BlueprintCallable, Category = "MuJoCo|Keyframe Camera")
    void TogglePlayPause();

    UFUNCTION(BlueprintCallable, Category = "MuJoCo|Keyframe Camera")
    void Reset();

    UFUNCTION(BlueprintPure, Category = "MuJoCo|Keyframe Camera")
    bool IsPlaying() const { return bIsPlaying; }

    /** @brief Capture current viewport camera as a new waypoint at the given time. */
    UFUNCTION(BlueprintCallable, CallInEditor, Category = "MuJoCo|Keyframe Camera")
    void CaptureCurrentView();

private:
    bool bIsPlaying = false;
    float PlaybackTime = 0.0f;
    bool bDelayPending = false;
    float DelayTimer = 0.0f;

    void UpdateCameraTransform();
    void RebuildSplinePreview();
};
