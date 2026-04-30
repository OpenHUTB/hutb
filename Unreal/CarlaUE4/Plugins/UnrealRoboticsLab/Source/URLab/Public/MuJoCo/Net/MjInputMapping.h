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
#include "InputAction.h"
#include "InputMappingContext.h"
#include "InputActionValue.h"
#include "MjInputMapping.generated.h"

class UMjActuator;
class AMjArticulation;
struct FInputActionInstance;

struct FCachedMjBinding
{
    TWeakObjectPtr<UMjActuator> Actuator;
    float Scale;
    bool bAccumulate;
};

USTRUCT(BlueprintType)
struct FMjInputBinding
{
    GENERATED_BODY()

    /** @brief The Input Action to listen for (e.g. MoveForward). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    UInputAction* Action = nullptr;

    /** @brief Map this action to a specific MuJoCo Actuator by name. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString ActuatorName;

    /** @brief Scale the input value before applying to the actuator. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Scale = 1.0f;
    
    /** @brief If true, adds to existing control signal. If false, overwrites. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bAccumulate = false;
};

/**
 * @class UMjInputMapping
 * @brief Binds Enhanced Input Actions directly to MuJoCo Actuators.
 * Attach this to a Pawn or Character that possesses an MjArticulation.
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class URLAB_API UMjInputMapping : public UActorComponent
{
	GENERATED_BODY()

public:	
	UMjInputMapping();

protected:
	virtual void BeginPlay() override;

public:	
    /** @brief The Mapping Context to add to the Player Controller (optional). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MuJoCo|Input")
    UInputMappingContext* DefaultMappingContext = nullptr;

    /** @brief Priority for the mapping context. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MuJoCo|Input")
    int32 Priority = 0;

    /** @brief List of bindings to setup. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MuJoCo|Input")
    TArray<FMjInputBinding> Bindings;

    /**
     * @brief The articulation this input mapping drives.
     * If set, actuator lookup is scoped to this articulation only.
     * If null, all articulations registered with AMuJoCoManager are searched.
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MuJoCo|Input")
    AMjArticulation* TargetArticulation = nullptr;

private:
    void SetupBindings();
    
    // Handler that accepts Instance to identify source action
    void GenericInputHandler(const FInputActionInstance& Instance);

    TMap<const UInputAction*, FCachedMjBinding> ActionCache;
};
