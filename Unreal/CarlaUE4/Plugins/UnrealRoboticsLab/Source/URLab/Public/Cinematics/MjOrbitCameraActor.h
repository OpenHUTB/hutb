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
#include "MjOrbitCameraActor.generated.h"

class UCineCameraComponent;
class UBoxComponent;
class AMjArticulation;
class UMjBody;
class AMjReplayManager;

/**
 * @class AMjOrbitCameraActor
 * @brief A cinematic orbit camera that auto-detects MjArticulations via a box trigger
 *        and smoothly orbits around them. Designed for teaser/showcase video capture.
 *
 * Integrates with the replay system:
 * - During recording, writes camera position/rotation to each replay frame
 * - During playback, reads camera transforms back for consistent framing across scenes
 *
 * Place in the level, resize the box trigger to cover the area where robots operate.
 * When an articulation enters the trigger, the camera locks onto it and orbits.
 */
UCLASS(Blueprintable)
class URLAB_API AMjOrbitCameraActor : public AActor
{
    GENERATED_BODY()

public:
    AMjOrbitCameraActor();

    virtual void Tick(float DeltaTime) override;

protected:
    virtual void BeginPlay() override;

    // --- Components ---

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "MuJoCo|Orbit Camera")
    UBoxComponent* DetectionBox;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "MuJoCo|Orbit Camera")
    UCineCameraComponent* CineCamera;

    // --- Orbit Parameters ---

    /** @brief Optional: manually assign the target articulation. If empty, auto-detects from overlap. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MuJoCo|Orbit Camera|Target")
    AMjArticulation* ManualTarget = nullptr;

    /** @brief Orbit radius (distance from target) in cm. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MuJoCo|Orbit Camera|Orbit")
    float OrbitRadius = 300.0f;

    /** @brief Orbit speed in degrees per second. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MuJoCo|Orbit Camera|Orbit")
    float OrbitSpeed = 30.0f;

    /** @brief Base height offset above the target's origin in cm. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MuJoCo|Orbit Camera|Orbit")
    float HeightOffset = 80.0f;

    /** @brief Amplitude of vertical oscillation in cm (0 = no bobbing). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MuJoCo|Orbit Camera|Orbit")
    float HeightOscillationAmplitude = 20.0f;

    /** @brief Speed of vertical oscillation in cycles per orbit. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MuJoCo|Orbit Camera|Orbit")
    float HeightOscillationFrequency = 2.0f;

    /** @brief How quickly the camera lerps to its target position (higher = snappier). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MuJoCo|Orbit Camera|Orbit", meta=(ClampMin="0.5", ClampMax="20.0"))
    float SmoothingSpeed = 5.0f;

    /** @brief Vertical look offset — how far above the target root to aim (cm). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MuJoCo|Orbit Camera|Orbit")
    float LookAtHeightOffset = 50.0f;

    /** @brief Auto-adjusts orbit radius to keep the full robot in frame. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MuJoCo|Orbit Camera|Framing")
    bool bAutoFrameRobot = true;

    /** @brief Padding multiplier for auto-framing (1.0 = tight, 1.5 = 50% padding). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MuJoCo|Orbit Camera|Framing", meta=(ClampMin="1.0", ClampMax="3.0"))
    float FramingPadding = 1.4f;

    /** @brief Minimum orbit radius even when auto-framing (cm). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MuJoCo|Orbit Camera|Framing")
    float MinOrbitRadius = 150.0f;

    // --- Lens ---

    /** @brief Focal length in mm. 35mm = wide, 85mm = portrait, 135mm = telephoto. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MuJoCo|Orbit Camera|Lens")
    float FocalLength = 50.0f;

    /** @brief Aperture (f-stop). Lower = more bokeh. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MuJoCo|Orbit Camera|Lens")
    float Aperture = 2.8f;

    // --- Activation ---

    /** @brief Whether to activate this camera as the player's view on BeginPlay. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MuJoCo|Orbit Camera")
    bool bAutoActivate = false;

    /** @brief If true, writes camera transforms to replay frames during recording. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MuJoCo|Orbit Camera|Replay")
    bool bRecordCameraPath = true;

    /** @brief If true, reads camera transforms from replay frames during playback instead of orbiting live. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MuJoCo|Orbit Camera|Replay")
    bool bPlaybackCameraPath = true;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "MuJoCo|Orbit Camera")
    bool bIsOrbiting = false;

public:
    // --- Public API ---

    UFUNCTION(BlueprintCallable, Category = "MuJoCo|Orbit Camera")
    void SetTarget(AMjArticulation* NewTarget);

    /** @brief Toggle orbit on/off. */
    UFUNCTION(BlueprintCallable, CallInEditor, Category = "MuJoCo|Orbit Camera")
    void ToggleOrbit() { bIsOrbiting = !bIsOrbiting; }

    UFUNCTION(BlueprintCallable, Category = "MuJoCo|Orbit Camera")
    void ActivateCamera();

    UFUNCTION(BlueprintCallable, Category = "MuJoCo|Orbit Camera")
    void DeactivateCamera();

    /** @brief Get the current computed camera position (for external use). */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "MuJoCo|Orbit Camera")
    FVector GetCurrentCameraPosition() const;

    /** @brief Get the current computed camera rotation. */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "MuJoCo|Orbit Camera")
    FRotator GetCurrentCameraRotation() const;

private:
    UFUNCTION()
    void OnBoxBeginOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
        UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

    UPROPERTY()
    AMjArticulation* CurrentTarget = nullptr;

    UPROPERTY()
    UMjBody* TrackedBody = nullptr;

    UPROPERTY()
    AMjReplayManager* ReplayMgr = nullptr;

    float CurrentAngle = 0.0f;

    UPROPERTY()
    APlayerController* CachedPC = nullptr;

    /** @brief Computes the desired orbit radius to keep the articulation fully in frame. */
    float ComputeAutoFrameRadius() const;

    /** @brief Writes current camera transform to the replay manager's latest live frame. */
    void WriteCameraToReplayFrame();

    /** @brief Tracks the last replay frame index written to, to avoid duplicate writes. */
    int32 LastCameraWriteFrameIdx = 0;
};
