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

#include "MuJoCo/Components/Controllers/MjPDController.h"
#include "MuJoCo/Components/Actuators/MjActuator.h"
#include "Utils/URLabLogging.h"

UMjPDController::UMjPDController()
{
}

void UMjPDController::Bind(mjModel* m, mjData* d, const TMap<int32, UMjActuator*>& ActuatorIdMap)
{
	Super::Bind(m, d, ActuatorIdMap);

	// Initialize gain arrays to defaults if not pre-configured
	int32 N = Bindings.Num();
	if (Kp.Num() != N) { Kp.SetNum(N); for (auto& v : Kp) v = DefaultKp; }
	if (Kv.Num() != N) { Kv.SetNum(N); for (auto& v : Kv) v = DefaultKv; }
	if (TorqueLimits.Num() != N) { TorqueLimits.SetNum(N); for (auto& v : TorqueLimits) v = DefaultTorqueLimit; }

	UE_LOG(LogURLab, Log, TEXT("[PDController] Bound %d actuators with Kp[0]=%.2f, Kv[0]=%.2f, TorqueLimit[0]=%.1f"),
		N, N > 0 ? Kp[0] : 0.f, N > 0 ? Kv[0] : 0.f, N > 0 ? TorqueLimits[0] : 0.f);
}

void UMjPDController::ComputeAndApply(mjModel* m, mjData* d, uint8 Source)
{
	if (!bIsBound) return;

	for (int32 i = 0; i < Bindings.Num(); ++i)
	{
		const FActuatorBinding& B = Bindings[i];

		// Get the desired position target from ZMQ or UI
		float Target = B.Component->ResolveDesiredControl(Source);

		// Clamp target to joint range (matches position actuator ctrlrange behavior)
		int32 JntId = m->actuator_trnid[B.ActuatorMjID * 2];
		if (JntId >= 0 && JntId < m->njnt && m->jnt_limited[JntId])
		{
			float Lo = (float)m->jnt_range[JntId * 2];
			float Hi = (float)m->jnt_range[JntId * 2 + 1];
			Target = FMath::Clamp(Target, Lo, Hi);
		}

		// Read live joint state
		float Pos = (float)d->qpos[B.QposAddr];
		float Vel = (float)d->qvel[B.QvelAddr];

		// PD control law: torque = Kp * (target - pos) - Kv * vel
		float kp = (i < Kp.Num()) ? Kp[i] : DefaultKp;
		float kv = (i < Kv.Num()) ? Kv[i] : DefaultKv;
		float limit = (i < TorqueLimits.Num()) ? TorqueLimits[i] : DefaultTorqueLimit;

		float Torque = kp * (Target - Pos) - kv * Vel;
		Torque = FMath::Clamp(Torque, -limit, limit);

		d->ctrl[B.ActuatorMjID] = (mjtNum)Torque;
	}
}

void UMjPDController::SetGains(const TArray<float>& NewKp, const TArray<float>& NewKv, const TArray<float>& NewTorqueLimits)
{
	if (NewKp.Num() > 0) Kp = NewKp;
	if (NewKv.Num() > 0) Kv = NewKv;
	if (NewTorqueLimits.Num() > 0) TorqueLimits = NewTorqueLimits;
}
