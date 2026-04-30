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
#include "mujoco/mjspec.h"
#include "MuJoCo/Components/MjComponent.h"
#include "MjEquality.generated.h"

/**
 * @enum EMjEqualityType
 * @brief Defines the type of equality constraint.
 */
UENUM(BlueprintType)
enum class EMjEqualityType : uint8
{
    Connect     = 0,
    Weld        = 1,
    Joint       = 2,
    Tendon      = 3,
    Flex        = 4,
    FlexVert    = 5,
    FlexStrain  = 6
};

/**
 * @class UMjEquality
 * @brief Component representing a MuJoCo equality constraint.
 *
 * Equality constraints (connect, weld, joint, tendon) enforce kinematic relationships
 * between bodies, joints, or tendons.
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class URLAB_API UMjEquality : public UMjComponent
{
    GENERATED_BODY()

public:
    UMjEquality();

    /**
     * @brief Exports properties to a pre-created MuJoCo spec equality structure.
     * @param Eq Pointer to the target mjsEquality structure.
     */
    void ExportTo(mjsEquality* Eq);

    /**
     * @brief Registers this equality constraint to the MuJoCo spec.
     * @param Wrapper The spec wrapper instance.
     * @param ParentBody Unused (equalities are global in the spec).
     */
    virtual void RegisterToSpec(class FMujocoSpecWrapper& Wrapper, mjsBody* ParentBody = nullptr) override;

    /**
     * @brief Imports properties from a raw XML node.
     * @param Node The <connect>, <weld>, <joint>, or <tendon> XML child node inside <equality>.
     */
    void ImportFromXml(const class FXmlNode* Node);

    /** @brief The type of equality constraint. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MuJoCo|Equality")
    EMjEqualityType EqualityType = EMjEqualityType::Weld;

    /** @brief Name of the first object (body, joint, or tendon). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MuJoCo|Equality", meta=(GetOptions="GetObjOptions"))
    FString Obj1;

    /** @brief Name of the second object (body, joint, or tendon). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MuJoCo|Equality", meta=(GetOptions="GetObjOptions"))
    FString Obj2;

#if WITH_EDITOR
    UFUNCTION()
    TArray<FString> GetObjOptions() const;
#endif

    /** @brief Whether the equality constraint is initially active. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MuJoCo|Equality")
    bool bActive = true;

    // --- Solver Parameters ---

    /** @brief Override toggle for SolRef. */
    UPROPERTY(EditAnywhere, Category = "MuJoCo|Equality|Physics", meta=(InlineEditConditionToggle))
    bool bOverride_SolRef = false;

    /** @brief Constraint solver reference (timeconst, dampratio). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MuJoCo|Equality|Physics", meta=(EditCondition="bOverride_SolRef"))
    TArray<float> SolRef;

    /** @brief Override toggle for SolImp. */
    UPROPERTY(EditAnywhere, Category = "MuJoCo|Equality|Physics", meta=(InlineEditConditionToggle))
    bool bOverride_SolImp = false;

    /** @brief Constraint solver impedance (dmin, dmax, width). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MuJoCo|Equality|Physics", meta=(EditCondition="bOverride_SolImp"))
    TArray<float> SolImp;

    // --- Type Specific Parameters ---

    /** @brief Override toggle for Anchor. Used for Connect and Weld. */
    UPROPERTY(EditAnywhere, Category = "MuJoCo|Equality|Parameters", meta=(InlineEditConditionToggle))
    bool bOverride_Anchor = false;

    /** @brief Position of the connection point, in body2 frame. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MuJoCo|Equality|Parameters", meta=(EditCondition="bOverride_Anchor"))
    FVector Anchor = FVector::ZeroVector;

    /** @brief Override toggle for RelPose. Used for Weld. */
    UPROPERTY(EditAnywhere, Category = "MuJoCo|Equality|Parameters", meta=(InlineEditConditionToggle))
    bool bOverride_RelPose = false;

    /** @brief Relative position and orientation of body1 in body2 frame. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MuJoCo|Equality|Parameters", meta=(EditCondition="bOverride_RelPose"))
    FTransform RelPose = FTransform::Identity;

    /** @brief Override toggle for PolyCoef. Used for Joint and Tendon. */
    UPROPERTY(EditAnywhere, Category = "MuJoCo|Equality|Parameters", meta=(InlineEditConditionToggle))
    bool bOverride_PolyCoef = false;

    /** @brief Coefficients of the cubic polynomial: y = a + b*x + c*x^2 + d*x^3. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MuJoCo|Equality|Parameters", meta=(EditCondition="bOverride_PolyCoef"))
    TArray<float> PolyCoef;

    /** @brief Override toggle for TorqueScale. Used for Weld only. */
    UPROPERTY(EditAnywhere, Category = "MuJoCo|Equality|Parameters", meta=(InlineEditConditionToggle))
    bool bOverride_TorqueScale = false;

    /** @brief Torque-to-force scaling ratio for weld constraint (mjSpec weld data[7]). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MuJoCo|Equality|Parameters", meta=(EditCondition="bOverride_TorqueScale", EditConditionHides))
    float TorqueScale = 1.0f;
};
