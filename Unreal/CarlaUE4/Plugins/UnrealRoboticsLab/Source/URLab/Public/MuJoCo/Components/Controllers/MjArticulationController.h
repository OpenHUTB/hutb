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
#include <mujoco/mujoco.h>
#include "MjArticulationController.generated.h"

class UMjActuator;

/**
 * @struct FActuatorBinding
 * @brief Maps a MuJoCo actuator to its associated joint DOF addresses.
 * Built once at bind time, used every physics step.
 */
USTRUCT()
struct FActuatorBinding
{
	GENERATED_BODY()

	/** MuJoCo actuator ID — index into d->ctrl */
	int32 ActuatorMjID = -1;

	/** Index into d->qpos for the joint this actuator drives */
	int32 QposAddr = -1;

	/** Index into d->qvel (generalized velocity) for the joint */
	int32 QvelAddr = -1;

	/** The UMjActuator component (holds NetworkValue/InternalValue) */
	UPROPERTY()
	UMjActuator* Component = nullptr;
};

/**
 * @class UMjArticulationController
 * @brief Abstract base for custom articulation control laws.
 *
 * Derive from this class to implement custom controllers (PD, impedance,
 * operational-space, etc.). Override ComputeAndApply() with your control
 * logic. The base class handles actuator→DOF binding automatically.
 *
 * Add your controller as a component on an AMjArticulation Blueprint.
 * ApplyControls() will find it and delegate control computation to it.
 *
 * Example:
 * @code
 * UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
 * class UMyController : public UMjArticulationController
 * {
 *     GENERATED_BODY()
 * public:
 *     virtual void ComputeAndApply(mjModel* m, mjData* d, uint8 Source) override
 *     {
 *         for (int32 i = 0; i < Bindings.Num(); ++i)
 *         {
 *             float target = Bindings[i].Component->ResolveDesiredControl(Source);
 *             float pos = (float)d->qpos[Bindings[i].QposAddr];
 *             float vel = (float)d->qvel[Bindings[i].QvelAddr];
 *             // ... your control law ...
 *             d->ctrl[Bindings[i].ActuatorMjID] = torque;
 *         }
 *     }
 * };
 * @endcode
 */
UCLASS(Abstract, ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class URLAB_API UMjArticulationController : public UActorComponent
{
	GENERATED_BODY()

public:
	UMjArticulationController();

	/**
	 * Called once after the MuJoCo model compiles. Resolves actuator→DOF
	 * mappings via m->actuator_trnid and populates Bindings array.
	 * Override to add custom post-bind logic, but call Super::Bind() first.
	 */
	virtual void Bind(mjModel* m, mjData* d, const TMap<int32, UMjActuator*>& ActuatorIdMap);

	/**
	 * Called every physics step from AMjArticulation::ApplyControls().
	 * Must write to d->ctrl[Bindings[i].ActuatorMjID] for each actuator.
	 * Runs on the async physics thread — must be thread-safe.
	 *
	 * @param m      The compiled MuJoCo model
	 * @param d      The MuJoCo data (qpos, qvel, ctrl live here)
	 * @param Source Control source: 0=ZMQ, 1=UI
	 */
	virtual void ComputeAndApply(mjModel* m, mjData* d, uint8 Source) PURE_VIRTUAL(UMjArticulationController::ComputeAndApply, );

	/** Is this controller active? If false, ApplyControls falls through to default path. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MuJoCo|Controller")
	bool bEnabled = true;

	/** Has Bind() been called successfully? */
	bool IsBound() const { return bIsBound; }

	/** Number of bound actuators. */
	int32 GetNumBindings() const { return Bindings.Num(); }

	/** Access bindings (for gain configuration by name). */
	const TArray<FActuatorBinding>& GetBindings() const { return Bindings; }

protected:
	/** Actuator→DOF bindings, populated by Bind(). */
	UPROPERTY()
	TArray<FActuatorBinding> Bindings;

	bool bIsBound = false;
};
