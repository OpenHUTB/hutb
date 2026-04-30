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
#include "mujoco/mujoco.h"
#include "MjZmqComponent.generated.h"

/**
 * @class UMjZmqComponent
 * @brief Abstract base class for components that communicate with the MuJoCo simulation via ZeroMQ (or similar sockets).
 * 
 * Provides lifecycle hooks (Init/Shutdown) and simulation step hooks (PreStep/PostStep)
 * that are executed on the dedicated MuJoCo physics thread.
 */
UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent), Abstract )
class URLAB_API UMjZmqComponent : public UActorComponent
{
	GENERATED_BODY()

public:	
    /** @brief Default constructor. */
	UMjZmqComponent();

    // --- ZMQ Lifecycle (Called from Async Thread) ---
    /** @brief Called to initialize ZMQ sockets/contexts. Runs on Async thread. */
	virtual void InitZmq() {};
    
    /** @brief Called to shutdown ZMQ resources. Runs on Async thread. */
	virtual void ShutdownZmq() {};

    // --- Simulation Hooks (Called from Async Thread) ---
    /** 
     * @brief Executed before mj_step(). 
     * Use this to apply controls (m_data->ctrl) or external forces (xfrc_applied) based on ZMQ input.
     */
	virtual void PreStep(mjModel* m, mjData* d) {};
    
    /** 
     * @brief Executed after mj_step(). 
     * Use this to read simulation state (qpos, qvel, sensordata) and send via ZMQ.
     */
	virtual void PostStep(mjModel* m, mjData* d) {};

protected:
    bool bIsInitialized = false;
};
