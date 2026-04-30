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
#include "MjImpulseLauncher.generated.h"

class UMjBody;
class UArrowComponent;
class AMjArticulation;

/**
 * @class AMjImpulseLauncher
 * @brief Applies a one-shot MuJoCo force impulse to a target MjBody.
 *
 * Place in level, set TargetBody reference and ImpulseStrength.
 * Fire via Blueprint (FireImpulse), input action, or auto-fire on BeginPlay.
 *
 * The impulse direction is taken from the actor's forward vector,
 * so aim the actor at the target.
 *
 * For the teaser video perturbation shot: attach a static mesh (crate)
 * as a Quick Convert body, then use this actor to launch it at the robot.
 */
UCLASS(Blueprintable)
class URLAB_API AMjImpulseLauncher : public AActor
{
    GENERATED_BODY()

public:
    AMjImpulseLauncher();

    virtual void BeginPlay() override;
    virtual void Tick(float DeltaTime) override;

    // --- Configuration ---

    /** The target actor to apply the impulse to. Must be an MjArticulation or have an MjQuickConvertComponent.
     *  The impulse is applied to the first MjBody found on the actor (or TargetBodyName if specified). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MuJoCo|Impulse")
    AActor* TargetActor = nullptr;

    /** Optional: name of a specific MjBody component on the target actor. If empty, uses the first MjBody found. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MuJoCo|Impulse")
    FName TargetBodyName;

    /** Launch speed in m/s. Sets the body's velocity directly (like throwing it). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MuJoCo|Impulse",
        meta = (ClampMin = "0.1", ClampMax = "100.0"))
    float LaunchSpeed = 10.0f;

    /** Direction override. If zero, uses the actor's forward vector. In Unreal world space. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MuJoCo|Impulse",
        meta = (EditCondition = "!LaunchTarget"))
    FVector DirectionOverride = FVector::ZeroVector;

    /** Optional: an actor to launch AT. Overrides direction — computes a ballistic arc toward this target.
     *  Can be an MjArticulation, Quick Convert actor, or any actor with a world position. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MuJoCo|Impulse")
    AActor* LaunchTarget = nullptr;

    /** Arc height as a fraction of the distance to target. 0 = flat, 0.5 = high lob. Only used with LaunchTarget. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MuJoCo|Impulse",
        meta = (ClampMin = "0.0", ClampMax = "2.0", EditCondition = "LaunchTarget != nullptr"))
    float ArcHeight = 0.3f;

    /** If true, fires automatically on BeginPlay after the delay. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MuJoCo|Impulse")
    bool bAutoFire = false;

    /** Delay in seconds before auto-fire (only used if bAutoFire is true). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MuJoCo|Impulse",
        meta = (ClampMin = "0.0", EditCondition = "bAutoFire"))
    float AutoFireDelay = 2.0f;

    // --- Actions ---

    /** Fire the impulse. Can be called from Blueprint, input binding, or Sequencer event track. */
    UFUNCTION(BlueprintCallable, Category = "MuJoCo|Impulse")
    void FireImpulse();

    /** Teleport the projectile back to the launcher's position and fire.
     *  Use this in PIE — tick the bool in the Details panel to reset and re-launch. */
    UFUNCTION(BlueprintCallable, Category = "MuJoCo|Impulse")
    void ResetAndFire();

    /** Tick this in the editor Details panel during PIE to reset + fire. Auto-clears. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MuJoCo|Impulse",
        meta = (DisplayName = "Reset & Fire (click to launch)"))
    bool bResetAndFire = false;

protected:

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "MuJoCo|Impulse")
    UArrowComponent* DirectionArrow;

private:
    /** Resolve TargetActor into an MjBody pointer. Called once on first fire. */
    UMjBody* ResolveTargetBody();

    UPROPERTY()
    UMjBody* ResolvedBody = nullptr;

    bool bAutoFirePending = false;
    float AutoFireTimer = 0.0f;
};