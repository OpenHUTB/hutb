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

#include <mujoco/mjspec.h>

#include "CoreMinimal.h"
#include "mujoco/mujoco.h"
#include "MuJoCo/Core/Spec/MjSpecElement.h"
#include "MuJoCo/Components/MjComponent.h"
#include "MuJoCo/Components/Defaults/MjDefault.h"
#include <atomic>
#include "MjActuator.generated.h"

/**
 * @enum EMjActuatorType
 * @brief Defines the type of actuator dynamics for MuJoCo.
 */
UENUM(BlueprintType)
enum class EMjActuatorType : uint8
{
    Motor,
    Position,
    Velocity,
    IntVelocity,
    Damper,
    Cylinder,
    Muscle,
    Adhesion,
    DcMotor
};

/**
 * @enum EMjActuatorTrnType
 * @brief Defines the type of transmission for the actuator.
 */
UENUM(BlueprintType)
enum class EMjActuatorTrnType : uint8
{
    Joint,
    JointInParent,
    SliderCrank,
    Tendon,
    Site,
    Body,
    Undefined
};

/**
 * @class UMjActuator
 * @brief Component representing a MuJoCo actuator.
 * 
 * An actuator generates force/torque and applies it to the simulation.
 * It corresponds to the `actuator` elements in MuJoCo (motor, position, velocity, etc.).
 */
UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class URLAB_API UMjActuator : public UMjComponent
{
	GENERATED_BODY()

public:	
    UMjActuator();

    /** @brief The type of actuator dynamics (e.g. Motor, Position). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MuJoCo|Actuator")
    EMjActuatorType Type;

    /** @brief The transmission type connecting the actuator to the system. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MuJoCo|Actuator")
    EMjActuatorTrnType TransmissionType;

    /** @brief Optional MuJoCo class name to inherit defaults from (string fallback). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MuJoCo|Actuator", meta=(GetOptions="GetDefaultClassOptions"))
    FString MjClassName;

    /** @brief Reference to a UMjDefault component for default class inheritance. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MuJoCo|Actuator")
    UMjDefault* DefaultClass = nullptr;

    virtual FString GetMjClassName() const override
    {
        return MjClassName;
    }

    virtual void ExportTo(mjsActuator* Actuator, mjsDefault* Default = nullptr);
    void ApplyRawOverrides(mjsActuator* Actuator, mjsDefault* Default = nullptr);

    
    /**
     * @brief Imports properties and override flags directly from the raw XML node.
     * @param Node Pointer to the corresponding FXmlNode.
     */
    void ImportFromXml(const class FXmlNode* Node);

    /**
     * @brief Parses subclass-specific properties from the XML node.
     */
    virtual void ParseSpecifics(const class FXmlNode* Node) {}

    /**
     * @brief Extracts subclass-specific properties from the MuJoCo spec mechanism.
     */
    virtual void ExtractSpecifics(const mjsActuator* Actuator) {}

    // --- Runtime Binding ---
    /**
     * @brief Registers this actuator to the MuJoCo spec.
     * @param Wrapper The spec wrapper instance.
     */
    virtual void RegisterToSpec(class FMujocoSpecWrapper& Wrapper, mjsBody* ParentBody = nullptr) override;

    /**
     * @brief Binds this component to the live MuJoCo simulation.
     */
    virtual void Bind(mjModel* Model, mjData* Data, const FString& Prefix = TEXT("")) override;

    /**
     * @brief Sets the internal control input (ctrl) for this actuator.
     * Updates the lock-free InternalValue atomic register.
     */
    UFUNCTION(BlueprintCallable, Category = "MuJoCo|Runtime")
    void SetControl(float Value);

    /**
     * @brief Resets the control input (ctrl) for this actuator to zero.
     * Synchronizes both the physics buffer and the UI control map.
     */
    UFUNCTION(BlueprintCallable, Category = "MuJoCo|Runtime")
    void ResetControl();

    /** @brief Gets the current resolved control value being applied. */
    UFUNCTION(BlueprintCallable, Category = "MuJoCo|Runtime")
    float GetControl() const;

    float GetMjControl() const;
    
    /** @brief Gets the current force generated by this actuator. */
    UFUNCTION(BlueprintCallable, Category = "MuJoCo|Runtime")
    float GetForce() const;

    /** @brief Gets the current length of the actuator. */
    UFUNCTION(BlueprintCallable, Category = "MuJoCo|Runtime")
    float GetLength() const;

    /** @brief Gets the current velocity of the actuator. */
    UFUNCTION(BlueprintCallable, Category = "MuJoCo|Runtime")
    float GetVelocity() const;

    /** @brief Gets the control range [min, max] from the compiled model. Returns ZeroVector if ctrl is not limited. */
    UFUNCTION(BlueprintCallable, Category = "MuJoCo|Runtime")
    FVector2D GetControlRange() const;

    /** @brief Gets the current activation state (for stateful actuators like muscle/intvelocity). Returns 0 if stateless. */
    UFUNCTION(BlueprintCallable, Category = "MuJoCo|Runtime")
    float GetActivation() const;

    /** @brief Gets the full prefixed name of this actuator as it appears in the compiled MuJoCo model. */
    virtual FString GetMjName() const override;

    /** @brief Sets the gear ratio (expert use). */
    UFUNCTION(BlueprintCallable, Category = "MuJoCo|Runtime")
    void SetGear(const TArray<float>& NewGear);

    /** @brief Gets the gear ratio. */
    UFUNCTION(BlueprintCallable, Category = "MuJoCo|Runtime")
    TArray<float> GetGear() const;

    /** 
     * @brief Resolves the final control value to apply based on the specified source.
     * This is called by the Articulation during the async physics step.
     */
    float ResolveDesiredControl(uint8 Source) const;

    /** @brief Sets the control value from the ZMQ networked stream. */
    void SetNetworkControl(float Value);

    ActuatorView m_ActuatorView;

protected:
    /** @brief Internal control value (from UI or Blueprints). */
    std::atomic<float> InternalValue{0.0f};

    /** @brief External control value (from ZMQ). */
    std::atomic<float> NetworkValue{0.0f};

public:

    /** @brief Name of the target element (Joint, Tendon, Site, etc.) this actuator acts upon. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MuJoCo|Actuator", meta=(GetOptions="GetTargetNameOptions"))
    FString TargetName;

    /** @brief Override toggle for Gear. */
    UPROPERTY(EditAnywhere, Category = "MuJoCo|Actuator", meta=(InlineEditConditionToggle))
    bool bOverride_Gear = false;

    /** @brief Gear ratio scaling for the transmission. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MuJoCo|Actuator", meta=(EditCondition="bOverride_Gear"))
    TArray<float> Gear;

    /** @brief Override toggle for GainPrm. */
    UPROPERTY(EditAnywhere, Category = "MuJoCo|Actuator", meta=(InlineEditConditionToggle))
    bool bOverride_GainPrm = false;

    /** @brief Custom gain parameters (gainprm). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MuJoCo|Actuator", meta=(EditCondition="bOverride_GainPrm"))
    TArray<float> GainPrm;

    /** @brief Override toggle for BiasPrm. */
    UPROPERTY(EditAnywhere, Category = "MuJoCo|Actuator", meta=(InlineEditConditionToggle))
    bool bOverride_BiasPrm = false;

    /** @brief Custom bias parameters (biasprm). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MuJoCo|Actuator", meta=(EditCondition="bOverride_BiasPrm"))
    TArray<float> BiasPrm;

    /** @brief Override toggle for CtrlRange. */
    UPROPERTY(EditAnywhere, Category = "MuJoCo|Actuator", meta=(InlineEditConditionToggle))
    bool bOverride_CtrlRange = false;

    /** @brief Control range [min, max]. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MuJoCo|Actuator", meta=(EditCondition="bOverride_CtrlRange"))
    TArray<float> CtrlRange = {0.0f, 0.0f};

    /** @brief Override toggle for CtrlLimited. */
    UPROPERTY(EditAnywhere, Category = "MuJoCo|Actuator", meta=(InlineEditConditionToggle))
    bool bOverride_CtrlLimited = false;

    /** @brief Whether control limiting is enabled. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MuJoCo|Actuator", meta=(EditCondition="bOverride_CtrlLimited"))
    bool bCtrlLimited = false;

    /** @brief Override toggle for ForceRange. */
    UPROPERTY(EditAnywhere, Category = "MuJoCo|Actuator", meta=(InlineEditConditionToggle))
    bool bOverride_ForceRange = false;

    /** @brief Force output range [min, max]. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MuJoCo|Actuator", meta=(EditCondition="bOverride_ForceRange"))
    TArray<float> ForceRange = {0.0f, 0.0f};

    /** @brief Override toggle for ForceLimited. */
    UPROPERTY(EditAnywhere, Category = "MuJoCo|Actuator", meta=(InlineEditConditionToggle))
    bool bOverride_ForceLimited = false;

    /** @brief Whether force limiting is enabled. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MuJoCo|Actuator", meta=(EditCondition="bOverride_ForceLimited"))
    bool bForceLimited = false;

    /** @brief Override toggle for ActLimited. */
    UPROPERTY(EditAnywhere, Category = "MuJoCo|Actuator", meta=(InlineEditConditionToggle))
    bool bOverride_ActLimited = false;

    /** @brief Whether internal activation state is limited. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MuJoCo|Actuator", meta=(EditCondition="bOverride_ActLimited"))
    bool bActLimited = false;

    /** @brief Override toggle for ActRange. */
    UPROPERTY(EditAnywhere, Category = "MuJoCo|Actuator", meta=(InlineEditConditionToggle))
    bool bOverride_ActRange = false;

    /** @brief Activation state range [min, max]. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MuJoCo|Actuator", meta=(EditCondition="bOverride_ActRange"))
    TArray<float> ActRange = {0.0f, 0.0f};

    /** @brief Override toggle for LengthRange. */
    UPROPERTY(EditAnywhere, Category = "MuJoCo|Actuator", meta=(InlineEditConditionToggle))
    bool bOverride_LengthRange = false;

    /** @brief Length range for muscle/tendon actuators. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MuJoCo|Actuator", meta=(EditCondition="bOverride_LengthRange"))
    TArray<float> LengthRange = {0.0f, 0.0f};

    // Transmission specific
    /** @brief Length of the crank for slider-crank transmission. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MuJoCo|Actuator")
    float CrankLength;

    /** @brief Name of the slider site for slider-crank transmission. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MuJoCo|Actuator", meta=(GetOptions="GetSliderSiteOptions"))
    FString SliderSite;

    /** @brief Reference site name. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MuJoCo|Actuator", meta=(GetOptions="GetRefSiteOptions"))
    FString RefSite;

    /** @brief Override toggle for Group. */
    UPROPERTY(EditAnywhere, Category = "MuJoCo|Actuator", meta=(InlineEditConditionToggle))
    bool bOverride_Group = false;

    /** @brief Actuator group ID. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MuJoCo|Actuator", meta=(EditCondition="bOverride_Group"))
    int Group = 0;

    /** @brief Override toggle for ActEarly. */
    UPROPERTY(EditAnywhere, Category = "MuJoCo|Actuator", meta=(InlineEditConditionToggle))
    bool bOverride_ActEarly = false;

    /** @brief Whether to apply actuation early (before other forces). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MuJoCo|Actuator", meta=(EditCondition="bOverride_ActEarly"))
    bool bActEarly = false;

    /** @brief Override toggle for Dynamics. */
    UPROPERTY(EditAnywhere, Category = "MuJoCo|Actuator", meta=(InlineEditConditionToggle))
    bool bOverride_Dynamics = false;

    /** @brief Override toggle for DynPrm. */
    UPROPERTY(EditAnywhere, Category = "MuJoCo|Actuator", meta=(InlineEditConditionToggle))
    bool bOverride_DynPrm = false;

    /** @brief Custom dynamic parameters (dynprm). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MuJoCo|Actuator", meta=(EditCondition="bOverride_DynPrm"))
    TArray<float> DynPrm;

#if WITH_EDITOR
    UFUNCTION()
    TArray<FString> GetTargetNameOptions() const;
    UFUNCTION()
    TArray<FString> GetSliderSiteOptions() const;
    UFUNCTION()
    TArray<FString> GetRefSiteOptions() const;
    UFUNCTION()
    TArray<FString> GetDefaultClassOptions() const;
#endif

	virtual void BeginPlay() override;
};
