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
#include "MuJoCo/Core/MjTypes.h"
#include "MuJoCo/Core/Spec/MjSpecWrapper.h"
#include "MuJoCo/Components/Actuators/MjActuator.h"
#include "MuJoCo/Components/Sensors/MjSensor.h"

#include "MuJoCo/Components/MjComponent.h"
#include "MjBody.generated.h"

/**
 * @enum EMjBodySleepPolicy
 * @brief Per-body sleep policy, matching MuJoCo's mjtSleepPolicy (user-settable subset).
 */
UENUM(BlueprintType)
enum class EMjBodySleepPolicy : uint8
{
    /** Let the compiler/global policy decide (mjSLEEP_AUTO). */
    Default     = 0  UMETA(DisplayName="Default (Auto)"),
    /** This body's tree is never allowed to sleep (mjSLEEP_NEVER). */
    Never       = 3  UMETA(DisplayName="Never"),
    /** This body's tree is allowed to sleep (mjSLEEP_ALLOWED). */
    Allowed     = 4  UMETA(DisplayName="Allowed"),
    /** This body's tree starts the simulation asleep (mjSLEEP_INIT). */
    InitAsleep  = 5  UMETA(DisplayName="Init Asleep"),
};

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class URLAB_API UMjBody : public UMjComponent
{
	GENERATED_BODY()

public:
	UMjBody();
    /** @brief If true, this body was created via Quick Convert, enabling specific logic like pivot correction. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MuJoCo|Body")
    bool bIsQuickConverted = false;

    /** @brief If true, this body's transform is driven by the Unreal Actor/Component, enabling One-Way coupling (Unreal -> MuJoCo). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MuJoCo|Body")
    bool bDrivenByUnreal = false;

    /** @brief Override toggle for Gravcomp. */
    UPROPERTY(EditAnywhere, Category = "MuJoCo|Body", meta=(InlineEditConditionToggle))
    bool bOverride_Gravcomp = false;

    /** @brief Anti-gravity force applied to the body, in units of body weight. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MuJoCo|Body", meta=(EditCondition="bOverride_Gravcomp"))
    float Gravcomp = 0.0f;
	
    
    /** @brief If true, overrides the MuJoCo default class name inherited from the parent body's childclass attribute. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MuJoCo|Body")
    bool bOverride_ChildClassName = false;

    /** @brief Child class name for this body. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MuJoCo|Body", meta=(EditCondition="bOverride_ChildClassName", GetOptions="GetChildClassOptions"))
    FString ChildClassName;

#if WITH_EDITOR
    UFUNCTION()
    TArray<FString> GetChildClassOptions() const;
#endif

    /** @brief Per-body sleep policy (MuJoCo 3.4+). Default lets the global option decide. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MuJoCo|Body|Sleep",
        meta=(ToolTip="Sleep policy for this body's kinematic tree. Only has effect when sleep is enabled in AMjManager Options."))
    EMjBodySleepPolicy SleepPolicy = EMjBodySleepPolicy::Default;

    /**
     * @brief Registers this body to the MuJoCo spec.
     * @param Wrapper The spec wrapper instance.
     * @param ParentBody The parent MuJoCo body.
     */
    virtual void RegisterToSpec(FMujocoSpecWrapper& Wrapper, mjsBody* ParentBody = nullptr) override;



public:
    /** @brief Called every frame. */
	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
	                           FActorComponentTickFunction* ThisTickFunction) override;
    
	void Setup(USceneComponent* Parent, mjsBody* ParentBody, FMujocoSpecWrapper* Wrapper);
    
    /**
     * @brief Imports properties (Transform) directly from the raw XML node.
     * @param Node Pointer to the corresponding FXmlNode.
     */
    void ImportFromXml(const class FXmlNode* Node);

    /**
     * @brief Imports properties with orientation handling respecting compiler settings.
     * @param Node Pointer to the corresponding FXmlNode.
     * @param CompilerSettings Compiler settings (angle units, euler sequence).
     */
    void ImportFromXml(const class FXmlNode* Node, const struct FMjCompilerSettings& CompilerSettings);

	void Bind(mjModel* Model, mjData* Data, const FString& Prefix = TEXT(""));
	
	BodyView GetBodyView() const;
    
    /** @brief Semantic accessor for raw MuJoCo data and helper methods. */
    BodyView& GetMj() { return m_BodyView; }
    const BodyView& GetMj() const { return m_BodyView; }

    // --- Runtime World-State Accessors (BlueprintCallable) ---

    /** @brief Gets this body's world position in Unreal Engine coordinates (cm). */
    UFUNCTION(BlueprintCallable, Category = "MuJoCo|Runtime")
    FVector GetWorldPosition() const;

    /** @brief Gets this body's world rotation as a quaternion (UE convention). */
    UFUNCTION(BlueprintCallable, Category = "MuJoCo|Runtime")
    FQuat GetWorldRotation() const;

    /**
     * @brief Gets the body's spatial velocity in Unreal coordinates (cm/s and deg/s).
     */
    UFUNCTION(BlueprintCallable, Category = "MuJoCo|Runtime")
    FMuJoCoSpatialVelocity GetSpatialVelocity() const;

    /**
     * @brief Applies an external wrench to this body for the next simulation step.
     * Force is in Newtons (Unreal coordinates), Torque in Newton-metres.
     * Note: The force is applied for ONE step then should be reapplied each tick to sustain it.
     */
    UFUNCTION(BlueprintCallable, Category = "MuJoCo|Runtime")
    void ApplyForce(FVector Force, FVector Torque);

    /** @brief Clears any external force/torque applied via ApplyForce on this body. */
    UFUNCTION(BlueprintCallable, Category = "MuJoCo|Runtime")
    void ClearForce();

    // --- Sleep (MuJoCo 3.4+) ---

    /**
     * @brief Returns true if this body is currently awake (not sleeping).
     *        Returns true if sleep is disabled or if the body has not been bound yet.
     */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "MuJoCo|Body|Sleep")
    bool IsAwake() const;

    /**
     * @brief Wakes this body and its kinematic tree, forcing it out of sleep.
     *        No-op if sleep is disabled or if this body has not been bound.
     */
    UFUNCTION(BlueprintCallable, Category = "MuJoCo|Body|Sleep")
    void Wake();

    /**
     * @brief Forces this body and its kinematic tree to sleep immediately.
     *        The physics step will skip this tree until it is woken externally or
     *        receives an impulse that exceeds the sleep tolerance.
     *        No-op if sleep is disabled or if this body has not been bound.
     */
    UFUNCTION(BlueprintCallable, Category = "MuJoCo|Body|Sleep")
    void Sleep();

protected:
	virtual void BeginPlay() override;

private:
	BodyView m_BodyView;
    
    mjtNum* m_MocapPos = nullptr;
    mjtNum* m_MocapQuat = nullptr;

	bool m_IsSetup = false;
    
    FVector m_MeshPivotOffset = FVector::ZeroVector;

	UPROPERTY()
	USceneComponent* m_Root;

	UPROPERTY()
	TArray<UMjBody*> m_Children;

    /** @brief Cached list of child Geoms for runtime binding. */
    UPROPERTY(Transient)
    TArray<UMjGeom*> m_Geoms;

    /** @brief Cached list of child Joints for runtime binding. */
    UPROPERTY(Transient)
    TArray<UMjJoint*> m_Joints;

    UPROPERTY()
    TArray<UMjActuator*> m_Actuators;

    UPROPERTY()
    TArray<UMjSensor*> m_Sensors;

    // Generic Spec Elements for binding
    UPROPERTY()
    TArray<TScriptInterface<IMjSpecElement>> m_SpecElements;
};
