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
#include "MjDcMotorActuator.generated.h"

/**
 * @enum EMjDcMotorInput
 * @brief Control input mode for the DC motor actuator. Matches the MuJoCo
 *        upstream `dcmotorinput_map` in `src/xml/xml_native_reader.cc`.
 */
UENUM(BlueprintType)
enum class EMjDcMotorInput : uint8
{
    Voltage  = 0,
    Position = 1,
    Velocity = 2
};

/**
 * @class UMjDcMotorActuator
 * @brief Models a DC motor with optional electrical dynamics (inductance),
 *        cogging torque, thermal effects, and LuGre friction. Introduced in
 *        MuJoCo 3.7.0.
 *
 * Each array field is padded to the size that `mjs_setToDCMotor` expects:
 *   motorconst[2] / nominal[3] / saturation[3] / inductance[2]
 *   cogging[3] / controller[6] / thermal[6] / lugre[5]
 * Missing tail entries are zero by default (per upstream).
 */
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class URLAB_API UMjDcMotorActuator : public UMjActuator
{
    GENERATED_BODY()

public:
    UMjDcMotorActuator();

    // --- DC motor parameters ---

    /** @brief Motor constant K (and optional winding factor). `motorconst` attribute. */
    UPROPERTY(EditAnywhere, Category = "Mj Actuator", meta = (InlineEditConditionToggle))
    bool bOverride_MotorConst = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mj Actuator|DC Motor", meta = (EditCondition = "bOverride_MotorConst"))
    TArray<float> MotorConst = { 0.0f, 0.0f };

    /** @brief Armature resistance. Scalar, not nullable in mjs_setToDCMotor. */
    UPROPERTY(EditAnywhere, Category = "Mj Actuator", meta = (InlineEditConditionToggle))
    bool bOverride_Resistance = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mj Actuator|DC Motor", meta = (EditCondition = "bOverride_Resistance"))
    float Resistance = 0.0f;

    /** @brief Nominal operating voltage/torque/speed [3]. */
    UPROPERTY(EditAnywhere, Category = "Mj Actuator", meta = (InlineEditConditionToggle))
    bool bOverride_Nominal = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mj Actuator|DC Motor", meta = (EditCondition = "bOverride_Nominal"))
    TArray<float> Nominal = { 0.0f, 0.0f, 0.0f };

    /** @brief Saturation current / torque / stall data [3]. */
    UPROPERTY(EditAnywhere, Category = "Mj Actuator", meta = (InlineEditConditionToggle))
    bool bOverride_Saturation = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mj Actuator|DC Motor", meta = (EditCondition = "bOverride_Saturation"))
    TArray<float> Saturation = { 0.0f, 0.0f, 0.0f };

    /** @brief Winding inductance [2]. Set non-zero to enable electrical dynamics. */
    UPROPERTY(EditAnywhere, Category = "Mj Actuator", meta = (InlineEditConditionToggle))
    bool bOverride_Inductance = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mj Actuator|DC Motor", meta = (EditCondition = "bOverride_Inductance"))
    TArray<float> Inductance = { 0.0f, 0.0f };

    /** @brief Cogging-torque params [3] (amplitude, periods, phase). */
    UPROPERTY(EditAnywhere, Category = "Mj Actuator", meta = (InlineEditConditionToggle))
    bool bOverride_Cogging = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mj Actuator|DC Motor", meta = (EditCondition = "bOverride_Cogging"))
    TArray<float> Cogging = { 0.0f, 0.0f, 0.0f };

    /** @brief Controller gains / limits [6]. */
    UPROPERTY(EditAnywhere, Category = "Mj Actuator", meta = (InlineEditConditionToggle))
    bool bOverride_Controller = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mj Actuator|DC Motor", meta = (EditCondition = "bOverride_Controller"))
    TArray<float> Controller = { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f };

    /** @brief Thermal-resistance-variation params [6]. */
    UPROPERTY(EditAnywhere, Category = "Mj Actuator", meta = (InlineEditConditionToggle))
    bool bOverride_Thermal = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mj Actuator|DC Motor", meta = (EditCondition = "bOverride_Thermal"))
    TArray<float> Thermal = { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f };

    /** @brief LuGre friction params [5]. */
    UPROPERTY(EditAnywhere, Category = "Mj Actuator", meta = (InlineEditConditionToggle))
    bool bOverride_LuGre = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mj Actuator|DC Motor", meta = (EditCondition = "bOverride_LuGre"))
    TArray<float> LuGre = { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f };

    /** @brief Control-input interpretation. */
    UPROPERTY(EditAnywhere, Category = "Mj Actuator", meta = (InlineEditConditionToggle))
    bool bOverride_Input = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mj Actuator|DC Motor", meta = (EditCondition = "bOverride_Input"))
    EMjDcMotorInput Input = EMjDcMotorInput::Voltage;

    virtual void ParseSpecifics(const class FXmlNode* Node) override;
    virtual void ExtractSpecifics(const mjsActuator* Actuator) override;
    virtual void ExportTo(mjsActuator* Actuator, mjsDefault* Default = nullptr) override;
};
