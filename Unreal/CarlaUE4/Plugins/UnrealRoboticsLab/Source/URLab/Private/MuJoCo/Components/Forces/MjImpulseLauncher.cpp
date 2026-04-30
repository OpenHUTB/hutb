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
#include "MuJoCo/Components/Forces/MjImpulseLauncher.h"
#include "MuJoCo/Components/Bodies/MjBody.h"
#include "MuJoCo/Components/QuickConvert/MjQuickConvertComponent.h"
#include "MuJoCo/Core/AMjManager.h"
#include "MuJoCo/Core/MjPhysicsEngine.h"
#include "MuJoCo/Utils/MjUtils.h"
#include "Components/ArrowComponent.h"
#include "mujoco/mujoco.h"

DEFINE_LOG_CATEGORY_STATIC(LogMjImpulse, Log, All);

AMjImpulseLauncher::AMjImpulseLauncher()
{
    PrimaryActorTick.bCanEverTick = true;

    USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    SetRootComponent(Root);

    DirectionArrow = CreateDefaultSubobject<UArrowComponent>(TEXT("DirectionArrow"));
    DirectionArrow->SetupAttachment(Root);
    DirectionArrow->SetArrowColor(FLinearColor::Red);
    DirectionArrow->ArrowSize = 2.0f;
    DirectionArrow->ArrowLength = 150.0f;
}

void AMjImpulseLauncher::BeginPlay()
{
    Super::BeginPlay();

    if (bAutoFire)
    {
        bAutoFirePending = true;
        AutoFireTimer = AutoFireDelay;
    }
}

void AMjImpulseLauncher::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    if (bAutoFirePending)
    {
        AutoFireTimer -= DeltaTime;
        if (AutoFireTimer <= 0.0f)
        {
            bAutoFirePending = false;
            FireImpulse();
        }
    }

    // Editor "button" — tick the bool in Details panel to fire
    if (bResetAndFire)
    {
        bResetAndFire = false;
        ResetAndFire();
    }
}

void AMjImpulseLauncher::ResetAndFire()
{
	if (!ResolvedBody)
	{
		ResolvedBody = ResolveTargetBody();
	}

	if (!ResolvedBody)
	{
		UE_LOG(LogMjImpulse, Warning, TEXT("ResetAndFire: Could not resolve target body"));
		return;
	}

	AAMjManager* Manager = AAMjManager::GetManager();
	if (!Manager || !Manager->PhysicsEngine || !Manager->PhysicsEngine->m_model || !Manager->PhysicsEngine->m_data)
	{
		UE_LOG(LogMjImpulse, Warning, TEXT("ResetAndFire: MjManager not ready"));
		return;
	}

	int BodyId = ResolvedBody->GetBodyView().id;
	if (BodyId < 0) return;

	const mjModel* m = Manager->PhysicsEngine->m_model;
	mjData* d = Manager->PhysicsEngine->m_data;

	// Find the free joint to get qpos/qvel addresses
	for (int j = 0; j < m->njnt; ++j)
	{
		if (m->jnt_bodyid[j] == BodyId && m->jnt_type[j] == mjJNT_FREE)
		{
			int qpos_adr = m->jnt_qposadr[j];
			int qvel_adr = m->jnt_dofadr[j];

			// Teleport to launcher position
			// Free joint qpos layout: [x, y, z, qw, qx, qy, qz]
			FVector LaunchPos = GetActorLocation();
			mjtNum mjPos[3];
			MjUtils::UEToMjPosition(LaunchPos, mjPos);

			d->qpos[qpos_adr + 0] = mjPos[0];
			d->qpos[qpos_adr + 1] = mjPos[1];
			d->qpos[qpos_adr + 2] = mjPos[2];

			// Reset orientation to identity
			d->qpos[qpos_adr + 3] = 1.0;
			d->qpos[qpos_adr + 4] = 0.0;
			d->qpos[qpos_adr + 5] = 0.0;
			d->qpos[qpos_adr + 6] = 0.0;

			// Zero out all velocities before firing
			for (int k = 0; k < 6; ++k)
				d->qvel[qvel_adr + k] = 0.0;

			UE_LOG(LogMjImpulse, Log, TEXT("Reset '%s' to launcher position (%.0f, %.0f, %.0f)"),
				*ResolvedBody->GetName(), LaunchPos.X, LaunchPos.Y, LaunchPos.Z);
			break;
		}
	}

	// Now fire
	FireImpulse();
}

UMjBody* AMjImpulseLauncher::ResolveTargetBody()
{
    if (!TargetActor)
    {
        UE_LOG(LogMjImpulse, Warning, TEXT("ResolveTargetBody: No TargetActor set on '%s'"), *GetName());
        return nullptr;
    }

    if (!TargetBodyName.IsNone())
    {
        TArray<UMjBody*> Bodies;
        TargetActor->GetComponents<UMjBody>(Bodies);
        for (UMjBody* Body : Bodies)
        {
            if (Body->GetFName() == TargetBodyName || Body->GetName() == TargetBodyName.ToString())
            {
                UE_LOG(LogMjImpulse, Log, TEXT("Resolved body '%s' on actor '%s'"),
                    *Body->GetName(), *TargetActor->GetName());
                return Body;
            }
        }
        UE_LOG(LogMjImpulse, Warning, TEXT("Body '%s' not found on '%s', using first MjBody"),
            *TargetBodyName.ToString(), *TargetActor->GetName());
    }

    UMjBody* Body = TargetActor->FindComponentByClass<UMjBody>();
    if (Body)
    {
        UE_LOG(LogMjImpulse, Log, TEXT("Resolved first MjBody '%s' on actor '%s'"),
            *Body->GetName(), *TargetActor->GetName());
    }
    else
    {
        UE_LOG(LogMjImpulse, Warning, TEXT("No MjBody found on actor '%s'"), *TargetActor->GetName());
    }
    return Body;
}

void AMjImpulseLauncher::FireImpulse()
{
    if (!ResolvedBody)
    {
        ResolvedBody = ResolveTargetBody();
    }

    if (!ResolvedBody)
    {
        UE_LOG(LogMjImpulse, Warning, TEXT("FireImpulse: Could not resolve target body on '%s'"), *GetName());
        return;
    }

    AAMjManager* Manager = AAMjManager::GetManager();
    if (!Manager || !Manager->PhysicsEngine || !Manager->PhysicsEngine->m_model || !Manager->PhysicsEngine->m_data)
    {
        UE_LOG(LogMjImpulse, Warning, TEXT("FireImpulse: MjManager not ready"));
        return;
    }

    int BodyId = ResolvedBody->GetBodyView().id;
    if (BodyId < 0)
    {
        UE_LOG(LogMjImpulse, Warning, TEXT("FireImpulse: Body '%s' not bound (id=%d)"),
            *ResolvedBody->GetName(), BodyId);
        return;
    }

    // Find the free joint on this body to get qvel address
    const mjModel* m = Manager->PhysicsEngine->m_model;
    mjData* d = Manager->PhysicsEngine->m_data;
    int qvel_adr = -1;

    for (int j = 0; j < m->njnt; ++j)
    {
        if (m->jnt_bodyid[j] == BodyId && m->jnt_type[j] == mjJNT_FREE)
        {
            qvel_adr = m->jnt_dofadr[j];
            break;
        }
    }

    if (qvel_adr < 0)
    {
        UE_LOG(LogMjImpulse, Warning, TEXT("FireImpulse: No free joint found on body '%s' (id=%d). "
            "Only free-joint bodies can be launched."),
            *ResolvedBody->GetName(), BodyId);
        return;
    }

    // Compute launch velocity
    FVector LaunchVelocityUE; // in cm/s (Unreal units)

    if (LaunchTarget)
    {
        // Targeted mode: compute ballistic velocity to hit the target
        // Start = launcher position (where we just teleported the projectile)
        FVector StartPos = GetActorLocation();

        // End = target's live MuJoCo position if it has an MjBody, else Unreal position
        FVector EndPos;
        UMjBody* TargetMjBody = LaunchTarget->FindComponentByClass<UMjBody>();
        if (TargetMjBody && TargetMjBody->GetBodyView().id >= 0)
        {
            EndPos = TargetMjBody->GetBodyView().GetWorldPosition();
        }
        else
        {
            EndPos = LaunchTarget->GetActorLocation();
        }
        FVector Delta = EndPos - StartPos;
        float HorizDist = FVector(Delta.X, Delta.Y, 0.0f).Size(); // cm
        float VertDist = Delta.Z; // cm

        // Gravity in UE units: MuJoCo default gravity = 9.81 m/s² = 981 cm/s²
        const float Gravity = 981.0f; // cm/s²

        // Desired peak height above the straight line (cm)
        float PeakHeight = HorizDist * ArcHeight;

        // Solve for initial vertical velocity to reach peak:
        // v_z = sqrt(2 * g * (peakHeight + max(0, -vertDist)))
        float EffectiveHeight = PeakHeight + FMath::Max(0.0f, -VertDist);
        float Vz = FMath::Sqrt(2.0f * Gravity * FMath::Max(EffectiveHeight, 1.0f));

        // Time to reach peak then fall to target height:
        // total time from launch to target: t = (Vz + sqrt(Vz² + 2*g*vertDist)) / g
        float Discriminant = Vz * Vz + 2.0f * Gravity * VertDist;
        float FlightTime = (Vz + FMath::Sqrt(FMath::Max(Discriminant, 0.0f))) / Gravity;
        FlightTime = FMath::Max(FlightTime, 0.1f); // safety min

        // Horizontal velocity to cover distance in that time
        FVector HorizDir = FVector(Delta.X, Delta.Y, 0.0f).GetSafeNormal();
        float Vh = HorizDist / FlightTime;

        LaunchVelocityUE = HorizDir * Vh + FVector(0, 0, Vz);

        float SpeedMs = LaunchVelocityUE.Size() / 100.0f;
        UE_LOG(LogMjImpulse, Log, TEXT("Targeted launch at '%s': dist=%.0fcm, arc=%.1f, "
            "flightTime=%.2fs, speed=%.1fm/s, Vh=%.0f Vz=%.0f"),
            *LaunchTarget->GetName(), HorizDist, ArcHeight,
            FlightTime, SpeedMs, Vh, Vz);
    }
    else
    {
        // Direct mode: use arrow direction + LaunchSpeed
        FVector Direction = DirectionOverride.IsNearlyZero()
            ? GetActorForwardVector()
            : DirectionOverride.GetSafeNormal();
        LaunchVelocityUE = Direction * LaunchSpeed * 100.0f; // m/s → cm/s
    }

    // Convert UE velocity (cm/s) to MuJoCo velocity (m/s) with Y-flip
    mjtNum mjVel[3];
    MjUtils::UEToMjPosition(LaunchVelocityUE, mjVel);

    // Set linear velocity on the free joint
    // Free joint qvel layout: [vx, vy, vz, wx, wy, wz]
    d->qvel[qvel_adr + 0] = mjVel[0];
    d->qvel[qvel_adr + 1] = mjVel[1];
    d->qvel[qvel_adr + 2] = mjVel[2];

    float FinalSpeedMs = FMath::Sqrt(mjVel[0]*mjVel[0] + mjVel[1]*mjVel[1] + mjVel[2]*mjVel[2]);
    UE_LOG(LogMjImpulse, Log, TEXT("Launched '%s': mjVel=(%.2f, %.2f, %.2f) m/s, speed=%.1f m/s, qvel_adr=%d"),
        *ResolvedBody->GetName(), mjVel[0], mjVel[1], mjVel[2], FinalSpeedMs, qvel_adr);
}
