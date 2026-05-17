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
#include "UObject/Interface.h"
#include <mujoco/mjspec.h>
#include <mujoco/mujoco.h>
#include "MjSpecElement.generated.h"

// Forward declarations
class FMujocoSpecWrapper;

// This class does not need to be modified.
UINTERFACE(MinimalAPI)
class UMjSpecElement : public UInterface
{
	GENERATED_BODY()
};

/**
 * Interface for components that can register themselves to a MuJoCo spec.
 */
class URLAB_API IMjSpecElement
{
	GENERATED_BODY()

public:
	/**
	 * Registers this component to the MuJoCo spec.
	 * @param Wrapper The spec wrapper instance.
	 * @param ParentBody The parent MuJoCo body (optional, depends on component type).
	 *
	 * Most components create their spec element here and call ExportTo().
	 * UMjBody is the exception: it creates its mjsBody in Setup() (called by
	 * FMujocoSpecWrapper::CreateBody) because child components need a valid
	 * ParentBody pointer before they can register. UMjBody::RegisterToSpec()
	 * therefore only calls ExportTo() on the already-created body.
	 */
	virtual void RegisterToSpec(FMujocoSpecWrapper& Wrapper, mjsBody* ParentBody = nullptr) = 0;

    /**
     * @brief Binds the component to the runtime MuJoCo simulation data.
     * @param model Pointer to the compiled mjModel.
     * @param data Pointer to the active mjData.
     * @param Prefix Optional prefix applied to names during lookup.
     */
    virtual void Bind(mjModel* model, mjData* data, const FString& Prefix = TEXT("")) = 0;
};
