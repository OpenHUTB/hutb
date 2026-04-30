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
#include "MjKeyframe.generated.h"

/**
 * @class UMjKeyframe
 * @brief Component representing a MuJoCo keyframe.
 *
 * Keyframes store simulation state (time, qpos, qvel, act, ctrl, mocap) at a specific moment.
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class URLAB_API UMjKeyframe : public UMjComponent
{
    GENERATED_BODY()

public:
    UMjKeyframe();

    /**
     * @brief Exports properties to a pre-created MuJoCo spec keyframe structure.
     * @param Key Pointer to the target mjsKey structure.
     */
    void ExportTo(mjsKey* Key);

    /**
     * @brief Registers this keyframe to the MuJoCo spec.
     * @param Wrapper The spec wrapper instance.
     * @param ParentBody Unused (keyframes are global).
     */
    virtual void RegisterToSpec(class FMujocoSpecWrapper& Wrapper, mjsBody* ParentBody = nullptr) override;

    /**
     * @brief Imports properties from a raw XML node.
     * @param Node The <key> XML node inside <keyframe>.
     */
    void ImportFromXml(const class FXmlNode* Node);

    /** @brief Simulation time for this keyframe. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MuJoCo|Keyframe")
    float Time = 0.0f;

    // --- State Vectors ---

    /** @brief Override toggle for Qpos. */
    UPROPERTY(EditAnywhere, Category = "MuJoCo|Keyframe", meta=(InlineEditConditionToggle))
    bool bOverride_Qpos = false;

    /** @brief Joint positions. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MuJoCo|Keyframe", meta=(EditCondition="bOverride_Qpos"))
    TArray<float> Qpos;

    /** @brief Override toggle for Qvel. */
    UPROPERTY(EditAnywhere, Category = "MuJoCo|Keyframe", meta=(InlineEditConditionToggle))
    bool bOverride_Qvel = false;

    /** @brief Joint velocities. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MuJoCo|Keyframe", meta=(EditCondition="bOverride_Qvel"))
    TArray<float> Qvel;

    /** @brief Override toggle for Act. */
    UPROPERTY(EditAnywhere, Category = "MuJoCo|Keyframe", meta=(InlineEditConditionToggle))
    bool bOverride_Act = false;

    /** @brief Actuator activations. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MuJoCo|Keyframe", meta=(EditCondition="bOverride_Act"))
    TArray<float> Act;

    /** @brief Override toggle for Ctrl. */
    UPROPERTY(EditAnywhere, Category = "MuJoCo|Keyframe", meta=(InlineEditConditionToggle))
    bool bOverride_Ctrl = false;

    /** @brief Actuator controls. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MuJoCo|Keyframe", meta=(EditCondition="bOverride_Ctrl"))
    TArray<float> Ctrl;

    /** @brief Override toggle for Mpos. */
    UPROPERTY(EditAnywhere, Category = "MuJoCo|Keyframe", meta=(InlineEditConditionToggle))
    bool bOverride_Mpos = false;

    /** @brief Mocap body positions. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MuJoCo|Keyframe", meta=(EditCondition="bOverride_Mpos"))
    TArray<float> Mpos;

    /** @brief Override toggle for Mquat. */
    UPROPERTY(EditAnywhere, Category = "MuJoCo|Keyframe", meta=(InlineEditConditionToggle))
    bool bOverride_Mquat = false;

    /** @brief Mocap body quaternions. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MuJoCo|Keyframe", meta=(EditCondition="bOverride_Mquat"))
    TArray<float> Mquat;
};
