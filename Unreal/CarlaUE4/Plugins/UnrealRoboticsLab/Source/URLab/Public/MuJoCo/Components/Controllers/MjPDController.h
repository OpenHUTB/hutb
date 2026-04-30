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
#include "MuJoCo/Components/Controllers/MjArticulationController.h"
#include "MjPDController.generated.h"

/**
 * @class UMjPDController
 * @brief Proportional-Derivative controller running at MuJoCo physics rate.
 *
 * Treats incoming control values as position targets and computes:
 *   torque = Kp * (target - qpos) - Kv * qvel
 *   torque = clamp(torque, -TorqueLimit, +TorqueLimit)
 *
 * This runs every physics step (e.g. 1000Hz at dt=0.001), producing
 * fresh torque from live joint state. Use with motor actuators in the MJCF.
 *
 * Designed for sim2sim transfer from policies trained with motor+PD
 * (RoboJuDo, Isaac). Gains should match the training configuration.
 */
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class URLAB_API UMjPDController : public UMjArticulationController
{
	GENERATED_BODY()

public:
	UMjPDController();

	/** Proportional gains, one per bound actuator (in binding order). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MuJoCo|PD Controller")
	TArray<float> Kp;

	/** Derivative gains, one per bound actuator. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MuJoCo|PD Controller")
	TArray<float> Kv;

	/** Torque limits (symmetric), one per bound actuator. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MuJoCo|PD Controller")
	TArray<float> TorqueLimits;

	/** Default Kp for actuators without explicit gain. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MuJoCo|PD Controller")
	float DefaultKp = 100.0f;

	/** Default Kv for actuators without explicit gain. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MuJoCo|PD Controller")
	float DefaultKv = 5.0f;

	/** Default torque limit for actuators without explicit limit. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MuJoCo|PD Controller")
	float DefaultTorqueLimit = 200.0f;

	// --- UMjArticulationController interface ---

	virtual void Bind(mjModel* m, mjData* d, const TMap<int32, UMjActuator*>& ActuatorIdMap) override;
	virtual void ComputeAndApply(mjModel* m, mjData* d, uint8 Source) override;

	/** Set all gains at once. Arrays must match binding count. */
	UFUNCTION(BlueprintCallable, Category = "MuJoCo|PD Controller")
	void SetGains(const TArray<float>& NewKp, const TArray<float>& NewKv, const TArray<float>& NewTorqueLimits);
};
