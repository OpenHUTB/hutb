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
#include "MjTypes.generated.h"

/**
 * @struct FMuJoCoSpatialVelocity
 * @brief Represents 6D spatial velocity (Linear + Angular).
 */
USTRUCT(BlueprintType)
struct FMuJoCoSpatialVelocity
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MuJoCo")
    FVector Linear = FVector::ZeroVector;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MuJoCo")
    FVector Angular = FVector::ZeroVector;
};

/**
 * @struct FMuJoCoJointState
 * @brief Complete runtime state of a 1-DOF joint.
 */
USTRUCT(BlueprintType)
struct FMuJoCoJointState
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MuJoCo")
    float Position = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MuJoCo")
    float Velocity = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MuJoCo")
    float Acceleration = 0.0f;
};
