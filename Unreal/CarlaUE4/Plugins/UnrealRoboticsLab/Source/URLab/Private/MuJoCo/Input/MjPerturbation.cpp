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

#include "MuJoCo/Input/MjPerturbation.h"

#include "MuJoCo/Core/AMjManager.h"
#include "MuJoCo/Core/MjPhysicsEngine.h"
#include "MuJoCo/Components/Geometry/MjGeom.h"
#include "MuJoCo/Components/QuickConvert/MjQuickConvertComponent.h"
#include "MuJoCo/Utils/MjUtils.h"
#include "Utils/URLabLogging.h"
#include "Components/StaticMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include <mujoco/mujoco.h>

// ---------------------------------------------------------------------------
// Utilities
// ---------------------------------------------------------------------------

namespace
{
    // UE direction → MuJoCo direction: unit Y-flip, no cm→m since directions
    // are unitless.
    FORCEINLINE void UEToMjDir(const FVector& V, mjtNum Out[3])
    {
        Out[0] = V.X;
        Out[1] = -V.Y;
        Out[2] = V.Z;
    }
}

UMjPerturbation::UMjPerturbation()
{
    PrimaryComponentTick.bCanEverTick = true;
}

void UMjPerturbation::BeginPlay()
{
    Super::BeginPlay();

    // Now that URLab's module init has loaded mujoco.dll, this is safe.
    mjv_defaultPerturb(&Perturb);

    Manager = Cast<AAMjManager>(GetOwner());
    if (!Manager || !Manager->PhysicsEngine)
    {
        UE_LOG(LogURLab, Warning, TEXT("[MjPerturbation] BeginPlay without AAMjManager + PhysicsEngine — disabling."));
        PrimaryComponentTick.bCanEverTick = false;
        return;
    }

    // Matches simulate.cc:2364-2371 — runs on the physics thread under CallbackMutex.
    Manager->PhysicsEngine->RegisterPreStepCallback(
        [this](mjModel* m, mjData* d)
        {
            if (!m || !d || !Manager || !Manager->PhysicsEngine) return;
            const bool bRunning = Manager->PhysicsEngine->IsRunning();

            if (bRunning)
            {
                // Zero xfrc on every body: otherwise the last force stays
                // latched after release and the body drifts indefinitely.
                mju_zero(d->xfrc_applied, 6 * m->nbody);
                mjv_applyPerturbPose(m, d, &Perturb, 0);
                mjv_applyPerturbForce(m, d, &Perturb);
            }
            else
            {
                // Paused: teleport to the reference pose. mj_forward propagates
                // qpos → xpos/xmat so the render side sees the move.
                mjv_applyPerturbPose(m, d, &Perturb, 1);
                if (Perturb.select > 0 && Perturb.active != 0)
                {
                    mj_forward(m, d);
                }
            }
        });
}

void UMjPerturbation::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    if (IsDragging())
    {
        DrawDebugSpring();
    }
}

// ---------------------------------------------------------------------------
// Body-id resolution
// ---------------------------------------------------------------------------

int32 UMjPerturbation::ResolveBodyIdFromActor(const AActor* Actor, const FVector& HitWorldUE) const
{
    if (!Actor) return -1;

    // Articulation path: find the nearest UMjGeom to the hit point (any geom
    // on the actor belongs to a body via Geom->GetMj().body_id).
    TArray<UMjGeom*> Geoms;
    const_cast<AActor*>(Actor)->GetComponents<UMjGeom>(Geoms);
    if (Geoms.Num() > 0)
    {
        UMjGeom* Best = nullptr;
        float BestDist = TNumericLimits<float>::Max();
        for (UMjGeom* G : Geoms)
        {
            if (!G || !G->IsBound()) continue;
            const float D = FVector::DistSquared(G->GetComponentLocation(), HitWorldUE);
            if (D < BestDist) { BestDist = D; Best = G; }
        }
        if (Best)
        {
            return Best->GetMj().body_id;
        }
    }

    // Quick-convert path: the hit actor has a UMjQuickConvertComponent.
    if (UMjQuickConvertComponent* QC = const_cast<AActor*>(Actor)->FindComponentByClass<UMjQuickConvertComponent>())
    {
        return QC->GetMjBodyId();
    }

    return -1;
}

// ---------------------------------------------------------------------------
// Public input API
// ---------------------------------------------------------------------------

void UMjPerturbation::HandleSelect(const FVector& CursorOrigin, const FVector& CursorDirection)
{
    if (!Manager || !Manager->PhysicsEngine || !Manager->PhysicsEngine->m_model || !Manager->PhysicsEngine->m_data)
    {
        return;
    }

    // Line trace from cursor. Distance is generous — 1 km covers typical
    // scene scales. Use Multi rather than Single because imported visual
    // meshes are configured with ECR_Overlap responses (see the importer):
    // SingleByChannel only returns blocking hits, so it would pass straight
    // through go1-style mesh-geom-only articulations. Multi collects both
    // block and overlap hits; we pick the first that resolves to an MjBody.
    const FVector TraceEnd = CursorOrigin + CursorDirection.GetSafeNormal() * 100000.0f;
    TArray<FHitResult> Hits;
    FCollisionQueryParams Params;
    Params.bTraceComplex = false;
    GetWorld()->LineTraceMultiByChannel(Hits, CursorOrigin, TraceEnd, ECC_Visibility, Params);

    int32 BodyId = -1;
    FHitResult SelectedHit;
    for (const FHitResult& H : Hits)
    {
        if (!H.GetActor()) continue;
        const int32 Resolved = ResolveBodyIdFromActor(H.GetActor(), H.ImpactPoint);
        if (Resolved > 0)
        {
            BodyId = Resolved;
            SelectedHit = H;
            break;
        }
    }

    FScopeLock Lock(&Manager->PhysicsEngine->CallbackMutex);
    const mjModel* m = Manager->PhysicsEngine->m_model;
    const mjData*  d = Manager->PhysicsEngine->m_data;

    if (BodyId > 0 && BodyId < m->nbody)
    {
        // Compute localpos = xmat[sel]^T * (world_hit - xpos[sel])
        mjtNum HitMj[3];
        MjUtils::UEToMjPosition(SelectedHit.ImpactPoint, HitMj);

        mjtNum Diff[3];
        mju_sub3(Diff, HitMj, d->xpos + 3 * BodyId);
        mju_mulMatTVec(Perturb.localpos, d->xmat + 9 * BodyId, Diff, 3, 3);

        Perturb.select      = BodyId;
        Perturb.flexselect  = -1;
        Perturb.skinselect  = -1;
        Perturb.active      = 0;

        ClickHitWorldUE = SelectedHit.ImpactPoint;

        UE_LOG(LogURLab, Log, TEXT("[MjPerturbation] Selected body %d (%s) at world %s"),
            BodyId, *SelectedHit.GetActor()->GetName(), *SelectedHit.ImpactPoint.ToString());
    }
    else
    {
        // Double-click on empty space deselects, matching simulate.
        Perturb.select     = 0;
        Perturb.flexselect = -1;
        Perturb.skinselect = -1;
        Perturb.active     = 0;
    }
}

// Snapshots the click-time pose into Perturb so drag updates are relative.
// Caller must hold CallbackMutex (reads mjData).
static void InitPerturbFieldsLocked(mjvPerturb& Perturb, const mjModel* m, const mjData* d,
                                    mjtNum OutBaseRefPos[3], mjtNum OutBaseRefSelPos[3], mjtNum OutBaseRefQuat[4])
{
    const int Sel = Perturb.select;
    if (Sel <= 0 || Sel >= m->nbody) return;

    mju_copy3(Perturb.refpos, d->xipos + 3 * Sel);
    mju_mulQuat(Perturb.refquat, d->xquat + 4 * Sel, m->body_iquat + 4 * Sel);

    mjtNum SelWorld[3];
    mju_mulMatVec3(SelWorld, d->xmat + 9 * Sel, Perturb.localpos);
    mju_addTo3(SelWorld, d->xpos + 3 * Sel);
    mju_copy3(Perturb.refselpos, SelWorld);

    // simulate uses a Jacobian-based inverse mass; body mass is a decent proxy.
    Perturb.localmass = (m->body_mass[Sel] > 0) ? m->body_mass[Sel] : 1.0;

    mju_copy3(OutBaseRefPos,    Perturb.refpos);
    mju_copy3(OutBaseRefSelPos, Perturb.refselpos);
    mju_copy4(OutBaseRefQuat,   Perturb.refquat);
}

void UMjPerturbation::StartTranslate(const FVector& CursorOrigin, const FVector& CursorDirection)
{
    if (!HasSelection() || !Manager || !Manager->PhysicsEngine) return;
    if (!Manager->PhysicsEngine->m_model || !Manager->PhysicsEngine->m_data) return;

    const FVector NormDir = CursorDirection.GetSafeNormal();
    ClickCamForward = NormDir;

    {
        FScopeLock Lock(&Manager->PhysicsEngine->CallbackMutex);
        InitPerturbFieldsLocked(Perturb, Manager->PhysicsEngine->m_model, Manager->PhysicsEngine->m_data,
                                BaseRefPos, BaseRefSelPos, BaseRefQuat);
        Perturb.active = mjPERT_TRANSLATE;
    }

    // Anchor click reference onto the cursor ray at the body's depth, so the
    // first UpdateDrag tick produces DeltaUE = 0 (no snap-under-cursor if
    // the mouse moved between select and Ctrl+RMB, or the body drifted).
    const FVector SelWorldUE(
        static_cast<float>(Perturb.refselpos[0]) * 100.0f,
        -static_cast<float>(Perturb.refselpos[1]) * 100.0f,
        static_cast<float>(Perturb.refselpos[2]) * 100.0f);
    ClickDepthCm = FVector::DotProduct(SelWorldUE - CursorOrigin, NormDir);
    if (ClickDepthCm < 1.0f) ClickDepthCm = 1.0f;
    ClickHitWorldUE = CursorOrigin + NormDir * ClickDepthCm;
}

void UMjPerturbation::StartRotate()
{
    if (!HasSelection() || !Manager || !Manager->PhysicsEngine) return;
    if (!Manager->PhysicsEngine->m_model || !Manager->PhysicsEngine->m_data) return;

    FScopeLock Lock(&Manager->PhysicsEngine->CallbackMutex);
    InitPerturbFieldsLocked(Perturb, Manager->PhysicsEngine->m_model, Manager->PhysicsEngine->m_data,
                            BaseRefPos, BaseRefSelPos, BaseRefQuat);
    Perturb.active = mjPERT_ROTATE;
}

void UMjPerturbation::UpdateDrag(const FVector& CursorOrigin, const FVector& CursorDirection,
                                 float MouseDx, float MouseDy)
{
    if (!HasSelection() || !IsDragging() || !Manager || !Manager->PhysicsEngine) return;

    if ((Perturb.active & mjPERT_TRANSLATE) != 0)
    {
        const FVector CurTargetUE = CursorOrigin + CursorDirection.GetSafeNormal() * ClickDepthCm;
        const FVector DeltaUE     = CurTargetUE - ClickHitWorldUE;

        mjtNum DeltaMj[3];
        MjUtils::UEToMjPosition(DeltaUE, DeltaMj);

        FScopeLock Lock(&Manager->PhysicsEngine->CallbackMutex);
        for (int i = 0; i < 3; ++i)
        {
            Perturb.refpos[i]    = BaseRefPos[i]    + DeltaMj[i];
            Perturb.refselpos[i] = BaseRefSelPos[i] + DeltaMj[i];
        }
    }
    else if ((Perturb.active & mjPERT_ROTATE) != 0)
    {
        // simulate.cc::convert2D + mjv_alignToCamera in MuJoCo coords.
        // ROTATE_V pre-aligned axis = (dy, 0, dx).
        mjtNum CamForwardMj[3];
        UEToMjDir(ClickCamForward.GetSafeNormal(), CamForwardMj);

        const mjtNum VecLocal[3] = { (mjtNum)MouseDy, 0.0, (mjtNum)MouseDx };
        mjtNum AxisMj[3];
        mjv_alignToCamera(AxisMj, VecLocal, CamForwardMj);

        const mjtNum Scl = mju_normalize3(AxisMj);
        if (Scl <= 0.0) return;

        mjtNum QDelta[4];
        mju_axisAngle2Quat(QDelta, AxisMj, Scl * mjPI * 2.0);

        FScopeLock Lock(&Manager->PhysicsEngine->CallbackMutex);
        const mjModel* m = Manager->PhysicsEngine->m_model;
        const mjData*  d = Manager->PhysicsEngine->m_data;
        if (!m || !d) return;

        // Accumulate onto refquat (simulate.cc::mjv_movePerturb:452).
        mjtNum Result[4];
        mju_mulQuat(Result, QDelta, Perturb.refquat);
        mju_copy4(Perturb.refquat, Result);
        mju_normalize4(Perturb.refquat);

        // ±90° limit (simulate.cc::mjv_movePerturb:455-481). Without it the
        // refquat drifts far from the body's pose and the spring misbehaves.
        const int32 Sel = Perturb.select;
        if (Sel > 0 && Sel < m->nbody)
        {
            mjtNum xiquat[4];
            mju_mulQuat(xiquat, d->xquat + 4 * Sel, m->body_iquat + 4 * Sel);

            mjtNum q1[4];
            mju_negQuat(q1, xiquat);

            mjtNum q2[4];
            mju_mulQuat(q2, q1, Perturb.refquat);

            mjtNum dif[3];
            mju_quat2Vel(dif, q2, 1);
            mjtNum scl = mju_normalize3(dif);

            if (scl < -mjPI * 0.5 || scl > mjPI * 0.5)
            {
                scl = FMath::Clamp<double>(scl, -mjPI * 0.5, mjPI * 0.5);
                mju_axisAngle2Quat(q2, dif, scl);
                mju_mulQuat(Perturb.refquat, xiquat, q2);
            }
        }
    }
}

void UMjPerturbation::StopDrag()
{
    if (!Manager || !Manager->PhysicsEngine) return;
    FScopeLock Lock(&Manager->PhysicsEngine->CallbackMutex);
    Perturb.active = 0;
}

void UMjPerturbation::DrawDebugSpring() const
{
    if (!HasSelection() || !Manager || !Manager->PhysicsEngine) return;
    const mjModel* m = Manager->PhysicsEngine->m_model;
    const mjData*  d = Manager->PhysicsEngine->m_data;
    if (!m || !d) return;
    const int32 Sel = Perturb.select;
    if (Sel <= 0 || Sel >= m->nbody) return;
    UWorld* World = GetWorld();
    if (!World) return;

    mjtNum SelWorldMj[3];
    mju_mulMatVec3(SelWorldMj, d->xmat + 9 * Sel, Perturb.localpos);
    mju_addTo3(SelWorldMj, d->xpos + 3 * Sel);

    auto MjPosToUE = [](const mjtNum P[3]) -> FVector {
        return FVector((float)P[0] * 100.0f, -(float)P[1] * 100.0f, (float)P[2] * 100.0f);
    };
    const FVector SelWorldUE = MjPosToUE(SelWorldMj);
    const FVector RefSelUE   = MjPosToUE(Perturb.refselpos);

    DrawDebugSphere(World, SelWorldUE, 3.0f, 12, FColor::Red, false, -1.0f, 0, 0.5f);

    if ((Perturb.active & mjPERT_TRANSLATE) != 0)
    {
        const float Extent = FVector::Dist(SelWorldUE, RefSelUE);
        const float HeadSize = FMath::Clamp(Extent * 0.25f, 8.0f, 60.0f);
        DrawDebugDirectionalArrow(World, SelWorldUE, RefSelUE, HeadSize,
            FColor::Green, false, -1.0f, 0, 1.5f);
    }
    else if ((Perturb.active & mjPERT_ROTATE) != 0)
    {
        // Tangent arrow (ω × r) — shrinks as the body catches up so the
        // arrow doesn't wobble when the rotation axis is noisy.
        mjtNum XIQuat[4];
        mju_mulQuat(XIQuat, d->xquat + 4 * Sel, m->body_iquat + 4 * Sel);
        mjtNum NegXI[4];
        mju_negQuat(NegXI, XIQuat);
        mjtNum QRel[4];
        mju_mulQuat(QRel, Perturb.refquat, NegXI);

        mjtNum OmegaMj[3];
        mju_quat2Vel(OmegaMj, QRel, 1.0);

        mjtNum Rvec[3];
        mju_sub3(Rvec, SelWorldMj, d->xipos + 3 * Sel);

        mjtNum TangentMj[3];
        mju_cross(TangentMj, OmegaMj, Rvec);

        // Boost m/rad → cm for visibility.
        constexpr float TangentVisualGainCm = 200.0f;
        const FVector TangentUE(
            (float)TangentMj[0] * TangentVisualGainCm,
            -(float)TangentMj[1] * TangentVisualGainCm,
            (float)TangentMj[2] * TangentVisualGainCm);

        const FVector ArrowEnd = SelWorldUE + TangentUE;
        const float Extent = TangentUE.Size();
        if (Extent > 2.0f)
        {
            const float HeadSize = FMath::Clamp(Extent * 0.25f, 8.0f, 60.0f);
            DrawDebugDirectionalArrow(World, SelWorldUE, ArrowEnd, HeadSize,
                FColor::Yellow, false, -1.0f, 0, 1.5f);
        }
    }
}
