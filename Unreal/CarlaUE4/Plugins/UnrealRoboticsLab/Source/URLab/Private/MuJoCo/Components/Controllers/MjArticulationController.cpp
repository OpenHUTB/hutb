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

#include "MuJoCo/Components/Controllers/MjArticulationController.h"
#include "MuJoCo/Components/Actuators/MjActuator.h"
#include "Utils/URLabLogging.h"

UMjArticulationController::UMjArticulationController()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UMjArticulationController::Bind(mjModel* m, mjData* d, const TMap<int32, UMjActuator*>& ActuatorIdMap)
{
	Bindings.Empty();

	for (auto& Elem : ActuatorIdMap)
	{
		int32 ActId = Elem.Key;
		UMjActuator* Actuator = Elem.Value;
		if (!Actuator || ActId < 0 || ActId >= m->nu) continue;

		// Only handle joint-transmission actuators (hinge, slide)
		if (m->actuator_trntype[ActId] != mjTRN_JOINT &&
		    m->actuator_trntype[ActId] != mjTRN_JOINTINPARENT)
		{
			UE_LOG(LogURLab, Warning, TEXT("[ArticulationController] Actuator %d has non-joint transmission type %d, skipping"),
				ActId, m->actuator_trntype[ActId]);
			continue;
		}

		int32 JointId = m->actuator_trnid[ActId * 2];
		if (JointId < 0 || JointId >= m->njnt) continue;

		// Skip free and ball joints (multi-DOF)
		int32 JointType = m->jnt_type[JointId];
		if (JointType == mjJNT_FREE || JointType == mjJNT_BALL)
		{
			UE_LOG(LogURLab, Warning, TEXT("[ArticulationController] Joint %d is free/ball (type %d), skipping"),
				JointId, JointType);
			continue;
		}

		FActuatorBinding Binding;
		Binding.ActuatorMjID = ActId;
		Binding.QposAddr = m->jnt_qposadr[JointId];
		Binding.QvelAddr = m->jnt_dofadr[JointId];
		Binding.Component = Actuator;
		Bindings.Add(Binding);
	}

	bIsBound = Bindings.Num() > 0;
	UE_LOG(LogURLab, Log, TEXT("[ArticulationController] Bound %d actuators to DOFs"), Bindings.Num());
}
