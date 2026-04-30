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
#include "MjCylinderActuator.generated.h"

/**
 * @class UMjCylinderActuator
 * @brief Specific Cylinder Actuator component.
 */
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class URLAB_API UMjCylinderActuator : public UMjActuator
{
    GENERATED_BODY()
public:
    UMjCylinderActuator();

    /** @brief Override toggle for TimeConst. */
    UPROPERTY(EditAnywhere, Category = "MuJoCo|Actuator", meta=(InlineEditConditionToggle))
    bool bOverride_TimeConst = false;

    /** @brief Time constant for activation dynamics. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MuJoCo|Actuator|Parameters", meta=(EditCondition="bOverride_TimeConst"))
    float TimeConst = 0.01f;

    /** @brief Override toggle for Bias. */
    UPROPERTY(EditAnywhere, Category = "MuJoCo|Actuator", meta=(InlineEditConditionToggle))
    bool bOverride_Bias = false;

    /** @brief Bias force offset. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MuJoCo|Actuator|Parameters", meta=(EditCondition="bOverride_Bias"))
    float Bias = 0.0f;

    /** @brief Override toggle for Area. */
    UPROPERTY(EditAnywhere, Category = "MuJoCo|Actuator", meta=(InlineEditConditionToggle))
    bool bOverride_Area = false;

    /** @brief Cross-sectional area. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MuJoCo|Actuator|Parameters", meta=(EditCondition="bOverride_Area"))
    float Area = 1.0f;

    /** @brief Override toggle for Diameter. */
    UPROPERTY(EditAnywhere, Category = "MuJoCo|Actuator", meta=(InlineEditConditionToggle))
    bool bOverride_Diameter = false;

    /** @brief Cylinder diameter. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MuJoCo|Actuator|Parameters", meta=(EditCondition="bOverride_Diameter"))
    float Diameter = 1.0f;

    virtual void ParseSpecifics(const class FXmlNode* Node) override;
    virtual void ExtractSpecifics(const mjsActuator* act) override;
    virtual void ExportTo(mjsActuator* act, mjsDefault* def = nullptr) override;
};
