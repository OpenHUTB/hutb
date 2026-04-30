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
#include "MuJoCo/Components/Actuators/MjActuator.h"
#include "MjPositionActuator.generated.h"

/**
 * @class UMjPositionActuator
 * @brief Specific Position Actuator component.
 */
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class URLAB_API UMjPositionActuator : public UMjActuator
{
    GENERATED_BODY()
public:
    UMjPositionActuator();

    /** @brief Override toggle for Kp. */
    UPROPERTY(EditAnywhere, Category = "MuJoCo|Actuator", meta=(InlineEditConditionToggle))
    bool bOverride_Kp = false;

    /** @brief Proportional gain (Kp) for position servos. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MuJoCo|Actuator|Parameters", meta=(EditCondition="bOverride_Kp"))
    float Kp = 1.0f;

    /** @brief Override toggle for Kv. */
    UPROPERTY(EditAnywhere, Category = "MuJoCo|Actuator", meta=(InlineEditConditionToggle))
    bool bOverride_Kv = false;

    /** @brief Derivative gain (Kv) for velocity damping. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MuJoCo|Actuator|Parameters", meta=(EditCondition="bOverride_Kv"))
    double Kv = 0.0f;

    /** @brief Override toggle for DampRatio. */
    UPROPERTY(EditAnywhere, Category = "MuJoCo|Actuator", meta=(InlineEditConditionToggle))
    bool bOverride_DampRatio = false;

    /** @brief Damping ratio. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MuJoCo|Actuator|Parameters", meta=(EditCondition="bOverride_DampRatio"))
    float DampRatio = 0.0f;

    /** @brief Override toggle for TimeConst. */
    UPROPERTY(EditAnywhere, Category = "MuJoCo|Actuator", meta=(InlineEditConditionToggle))
    bool bOverride_TimeConst = false;

    /** @brief Time constant for activation dynamics. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MuJoCo|Actuator|Parameters", meta=(EditCondition="bOverride_TimeConst"))
    float TimeConst = 0.0f;

    /** @brief Override toggle for InheritRange. */
    UPROPERTY(EditAnywhere, Category = "MuJoCo|Actuator", meta=(InlineEditConditionToggle))
    bool bOverride_InheritRange = false;

    /** @brief Inherited range scale factor. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MuJoCo|Actuator|Parameters", meta=(EditCondition="bOverride_InheritRange"))
    float InheritRange = 0.0f;

    virtual void ParseSpecifics(const class FXmlNode* Node) override;
    virtual void ExtractSpecifics(const mjsActuator* act) override;
    virtual void ExportTo(mjsActuator* act, mjsDefault* def = nullptr) override;
};
