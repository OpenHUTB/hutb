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

#include <mujoco/mjvisualize.h>

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "MjPerturbation.generated.h"

class AAMjManager;

/**
 * @class UMjPerturbation
 * @brief Mouse-driven body perturbation matching MuJoCo `simulate`'s behaviour.
 *
 * Input gestures (mirrored from upstream simulate):
 *  - Double-click LMB on a body → select (no force applied).
 *  - Ctrl + RMB drag with selection → translate (virtual position spring).
 *  - Ctrl + LMB drag with selection → rotate (virtual orientation spring).
 *  - Release button → stop drag; selection persists.
 *
 * Uses MuJoCo's own `mjv_applyPerturbForce` every physics pre-step to convert
 * `refpos`/`refquat`/`refselpos` into `xfrc_applied` — identical spring feel
 * to simulate. The plugin-side code only has to manage the reference
 * positions/orientations from UE mouse input.
 */
UCLASS(ClassGroup = (MuJoCo), meta = (BlueprintSpawnableComponent))
class URLAB_API UMjPerturbation : public UActorComponent
{
    GENERATED_BODY()

public:
    UMjPerturbation();

    virtual void BeginPlay() override;
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

    /** Line-trace from the cursor and, if a body is hit, latch selection.
     *  Called on double-click LMB (without Ctrl). */
    void HandleSelect(const FVector& CursorOrigin, const FVector& CursorDirection);

    /** Begin a translate drag (Ctrl+RMB press with a body selected). */
    void StartTranslate(const FVector& CursorOrigin, const FVector& CursorDirection);

    /** Begin a rotate drag (Ctrl+LMB press with a body selected). */
    void StartRotate();

    /** Update drag refpos/refselpos/refquat from current cursor + per-frame
     *  mouse delta. Safe to call every game-tick while active. */
    void UpdateDrag(const FVector& CursorOrigin, const FVector& CursorDirection,
                    float MouseDx, float MouseDy);

    /** Stop the current drag (mouse release). Selection persists. */
    void StopDrag();

    /** Is a body currently latched (pert.select > 0)? */
    bool HasSelection() const { return Perturb.select > 0; }

    /** Is a drag currently active (LMB/RMB held)? */
    bool IsDragging() const { return Perturb.active != 0; }

private:
    friend class UMjInputHandler;

    /** Cached manager (owner). */
    UPROPERTY(Transient)
    AAMjManager* Manager = nullptr;

    /** The actual mjv perturb struct passed to mjv_applyPerturbForce. */
    mjvPerturb Perturb{};

    /** Click-time state used to re-derive refpos each frame. */
    FVector ClickHitWorldUE = FVector::ZeroVector;   // hit point in UE world space
    float   ClickDepthCm    = 0.0f;                  // distance from camera to hit along ray
    FVector ClickCamForward = FVector::ForwardVector;// UE-space at click time
    FVector ClickCamUp      = FVector::UpVector;
    FVector ClickCamRight   = FVector::RightVector;
    mjtNum  BaseRefPos[3]   = {0, 0, 0};
    mjtNum  BaseRefSelPos[3]= {0, 0, 0};
    mjtNum  BaseRefQuat[4]  = {1, 0, 0, 0};

    /** Resolve a hit actor to a MuJoCo body_id. Returns <=0 if no match. */
    int32 ResolveBodyIdFromActor(const class AActor* Actor, const FVector& HitWorldUE) const;

    /** Draw a debug line from the world selection point to the ref target. */
    void DrawDebugSpring() const;
};
