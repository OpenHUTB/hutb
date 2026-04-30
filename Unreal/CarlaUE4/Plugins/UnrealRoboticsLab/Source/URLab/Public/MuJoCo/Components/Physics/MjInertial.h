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
#include "Components/SceneComponent.h"
#include "mujoco/mujoco.h"
#include "MuJoCo/Components/MjComponent.h"
#include "MjInertial.generated.h"



UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class URLAB_API UMjInertial : public UMjComponent
{
	GENERATED_BODY()

public:
    /** @brief Default constructor. */
    UMjInertial();

    /** @brief Override toggle for Mass. */
    UPROPERTY(EditAnywhere, Category = "MuJoCo|Inertial", meta=(InlineEditConditionToggle))
    bool bOverride_Mass = false;

    /** @brief Mass of the body. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MuJoCo|Inertial", meta=(EditCondition="bOverride_Mass"))
    float Mass = 0.0f;

    /** @brief Override toggle for DiagInertia. */
    UPROPERTY(EditAnywhere, Category = "MuJoCo|Inertial", meta=(InlineEditConditionToggle))
    bool bOverride_DiagInertia = false;

    /** @brief Diagonal inertia moments (ixx, iyy, izz). Used if FullInertia is empty. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MuJoCo|Inertial", meta=(EditCondition="bOverride_DiagInertia"))
    FVector DiagInertia = FVector(0.0f);

    /** @brief Override toggle for FullInertia. */
    UPROPERTY(EditAnywhere, Category = "MuJoCo|Inertial", meta=(InlineEditConditionToggle))
    bool bOverride_FullInertia = false;

    /** @brief Full inertia tensor (3 or 6 elements). If 6: xx, yy, zz, xy, xz, yz. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MuJoCo|Inertial", meta=(EditCondition="bOverride_FullInertia"))
    TArray<float> FullInertia;

    /**
     * @brief Imports properties from a MuJoCo XML node.
     * @param Node Pointer to the XML node.
     */
    void ImportFromXml(const class FXmlNode* Node);
    void ImportFromXml(const class FXmlNode* Node, const struct FMjCompilerSettings& CompilerSettings);

    // Note: Inertial usually doesn't have a direct 'mjsInertial' struct in mjspec.h same as others,
    // it's often part of body or explicit inertial element. 
    // mjsBody has 'mass', 'inertia', 'fullinertia'.
    
    /**
     * @brief Binds this component to the live MuJoCo simulation.
     */
    virtual void Bind(mjModel* model, mjData* data, const FString& Prefix = TEXT("")) override;

    /**
     * @brief Registers/Applies this inertial to the parent body in the spec.
     * @param Wrapper The spec wrapper instance.
     * @param ParentBody The parent body to apply inertial properties to.
     */
    virtual void RegisterToSpec(class FMujocoSpecWrapper& Wrapper, mjsBody* ParentBody) override;

};
