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
#include "MjMuscleActuator.generated.h"

/**
 * @class UMjMuscleActuator
 * @brief Specific Muscle Actuator component.
 */
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class URLAB_API UMjMuscleActuator : public UMjActuator
{
    GENERATED_BODY()
public:
    UMjMuscleActuator();

    /** @brief Override toggle for TimeConst. */
    UPROPERTY(EditAnywhere, Category = "MuJoCo|Actuator", meta=(InlineEditConditionToggle))
    bool bOverride_TimeConst = false;

    /** @brief Time constant for activation dynamics. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MuJoCo|Actuator|Parameters", meta=(EditCondition="bOverride_TimeConst"))
    float TimeConst = 0.01f;

    /** @brief Override toggle for TimeConst2. */
    UPROPERTY(EditAnywhere, Category = "MuJoCo|Actuator", meta=(InlineEditConditionToggle))
    bool bOverride_TimeConst2 = false;

    /** @brief Second time constant (e.g. deactivation time). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MuJoCo|Actuator|Parameters", meta=(EditCondition="bOverride_TimeConst2"))
    float TimeConst2 = 0.04f;

    /** @brief Override toggle for TauSmooth. */
    UPROPERTY(EditAnywhere, Category = "MuJoCo|Actuator", meta=(InlineEditConditionToggle))
    bool bOverride_TauSmooth = false;

    /** @brief Smoothing time constant. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MuJoCo|Actuator|Parameters", meta=(EditCondition="bOverride_TauSmooth"))
    float TauSmooth = 0.0f;

    /** @brief Override toggle for Force. */
    UPROPERTY(EditAnywhere, Category = "MuJoCo|Actuator", meta=(InlineEditConditionToggle))
    bool bOverride_Force = false;

    /** @brief Peak active force. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MuJoCo|Actuator|Parameters", meta=(EditCondition="bOverride_Force"))
    float Force = -1.0f; // Muscle force

    /** @brief Override toggle for Scale. */
    UPROPERTY(EditAnywhere, Category = "MuJoCo|Actuator", meta=(InlineEditConditionToggle))
    bool bOverride_Scale = false;

    /** @brief Force scaling factor. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MuJoCo|Actuator|Parameters", meta=(EditCondition="bOverride_Scale"))
    float Scale = 200.0f; // Muscle scale

    /** @brief Override toggle for LMin. */
    UPROPERTY(EditAnywhere, Category = "MuJoCo|Actuator", meta=(InlineEditConditionToggle))
    bool bOverride_LMin = false;

    /** @brief Minimum fiber length. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MuJoCo|Actuator|Parameters", meta=(EditCondition="bOverride_LMin"))
    float LMin = 0.5f;

    /** @brief Override toggle for LMax. */
    UPROPERTY(EditAnywhere, Category = "MuJoCo|Actuator", meta=(InlineEditConditionToggle))
    bool bOverride_LMax = false;

    /** @brief Maximum fiber length. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MuJoCo|Actuator|Parameters", meta=(EditCondition="bOverride_LMax"))
    float LMax = 1.6f;

    /** @brief Override toggle for VMax. */
    UPROPERTY(EditAnywhere, Category = "MuJoCo|Actuator", meta=(InlineEditConditionToggle))
    bool bOverride_VMax = false;

    /** @brief Maximum shortening velocity. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MuJoCo|Actuator|Parameters", meta=(EditCondition="bOverride_VMax"))
    float VMax = 1.5f;

    /** @brief Override toggle for FPMax. */
    UPROPERTY(EditAnywhere, Category = "MuJoCo|Actuator", meta=(InlineEditConditionToggle))
    bool bOverride_FPMax = false;

    /** @brief Peak passive force. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MuJoCo|Actuator|Parameters", meta=(EditCondition="bOverride_FPMax"))
    float FPMax = 1.3f;

    /** @brief Override toggle for FVMax. */
    UPROPERTY(EditAnywhere, Category = "MuJoCo|Actuator", meta=(InlineEditConditionToggle))
    bool bOverride_FVMax = false;

    /** @brief Peak viscous force. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MuJoCo|Actuator|Parameters", meta=(EditCondition="bOverride_FVMax"))
    float FVMax = 1.2f;

    virtual void ParseSpecifics(const class FXmlNode* Node) override;
    virtual void ExtractSpecifics(const mjsActuator* act) override;
    virtual void ExportTo(mjsActuator* act, mjsDefault* def = nullptr) override;
};
