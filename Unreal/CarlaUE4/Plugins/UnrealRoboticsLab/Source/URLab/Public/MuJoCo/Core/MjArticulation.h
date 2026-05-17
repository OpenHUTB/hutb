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
#include <mujoco/mjspec.h>
#include "MuJoCo/Core/Spec/MjSpecWrapper.h"
#include "MuJoCo/Core/MjSimOptions.h"
#include "GameFramework/Pawn.h"
#include "MuJoCo/Components/Bodies/MjBody.h"
#include "MuJoCo/Components/Joints/MjJoint.h"
#include "MuJoCo/Components/Sensors/MjSensor.h"
#include "MuJoCo/Components/Actuators/MjActuator.h"
#include "MuJoCo/Components/Tendons/MjTendon.h"
#include "MuJoCo/Components/Constraints/MjEquality.h"
#include "MuJoCo/Components/Keyframes/MjKeyframe.h"
#include "MuJoCo/Components/Bodies/MjFrame.h"
#include "MjArticulation.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnMjSimulationReset);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnMjCollision, UMjGeom*, SelfGeom, UMjGeom*, OtherGeom, FVector, ContactPos);

/**
 * @class AMjArticulation
 * @brief Represents a MuJoCo articulation (robot or multibody system).
 * 
 * This actor parses its hierarchy of MuJoCo-related components (MjBody, MjJoint, etc.)
 * and compiles them into the MuJoCo mjSpec. It also handles runtime synchronization
 * between MuJoCo physics and Unreal SceneComponents.
 */
UCLASS(config=Game)
class URLAB_API AMjArticulation : public APawn
{
    GENERATED_BODY()
    
public:    
    AMjArticulation();

    /** Allow ticking in editor viewports when debug drawing is enabled. */
    virtual bool ShouldTickIfViewportsOnly() const override;

public:
#if WITH_EDITORONLY_DATA
    /** @brief If true, automatically validates spec when the Blueprint is compiled. */
    UPROPERTY(EditAnywhere, Category = "MuJoCo|Validation")
    bool bValidateOnBlueprintCompile = true;
#endif

#if WITH_EDITOR
    virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
    virtual void OnConstruction(const FTransform& Transform) override;

    /** @brief Validates the articulation's MuJoCo spec by attempting a temporary compile. */
    UFUNCTION(BlueprintCallable, CallInEditor, Category = "MuJoCo|Validation")
    void ValidateSpec();

private:
    FDelegateHandle BlueprintCompiledHandle;
    void OnBlueprintCompiled(UBlueprint* Blueprint);
public:
#endif

    /** @brief Toggle to show/hide Group 3 visualization (often used for collision meshes). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MuJoCo|Visuals")
    bool bShowGroup3 = false;

    /** @brief Toggles debug drawing of MuJoCo collision convex hulls for this articulation. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category = "MuJoCo|Debug")
    bool bDrawDebugCollision = false;

    /** @brief Toggles debug drawing of joint axes and range arcs for this articulation. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category = "MuJoCo|Debug")
    bool bDrawDebugJoints = false;

    /** @brief Toggles debug drawing of site markers for this articulation. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category = "MuJoCo|Debug")
    bool bDrawDebugSites = false;

    /** @brief Editor button to toggle Group 3 visibility. */
    UFUNCTION(CallInEditor, Category = "MuJoCo|Visuals")
    void ToggleGroup3Visibility();

    /** @brief Updates the visibility of all Group 3 geoms based on bShowGroup3. */
    void UpdateGroup3Visibility();


    // --- Possess Camera Settings ---

    /** @brief Spring arm length when possessed (cm). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MuJoCo|Camera", meta = (ClampMin = "50.0", ClampMax = "1000.0"))
    float PossessCameraDistance = 300.0f;

    /** @brief Camera pitch angle when possessed (degrees, negative = look down). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MuJoCo|Camera", meta = (ClampMin = "-80.0", ClampMax = "0.0"))
    float PossessCameraPitch = -20.0f;

    /** @brief Camera position lag speed. Lower = smoother, higher = snappier. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MuJoCo|Camera", meta = (ClampMin = "0.5", ClampMax = "20.0"))
    float PossessCameraLagSpeed = 3.0f;

    /** @brief Camera rotation lag speed. Lower = smoother. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MuJoCo|Camera", meta = (ClampMin = "0.5", ClampMax = "20.0"))
    float PossessCameraRotationLagSpeed = 3.0f;

    /** @brief Vertical offset above body center (cm). Reduces vertical bounce from walking/trotting. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MuJoCo|Camera")
    FVector PossessCameraOffset = FVector(0.0f, 0.0f, 30.0f);

    /** @brief Determines whether this specific articulation is controlled by ZMQ or UI/Blueprints. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MuJoCo|Runtime")
    uint8 ControlSource = 0; // 0 = ZMQ, 1 = UI. Using uint8 to avoid circular dependency with AMuJoCoManager header in some contexts, but can cast to EControlSource.

    /** @brief Fires when the simulation is reset (qpos/qvel cleared). Use this to reset robot logic. */
    UPROPERTY(BlueprintAssignable, Category = "MuJoCo|Events")
    FOnMjSimulationReset OnSimulationReset;

    /** @brief Fires when a collision occurs involving geoms of this articulation. */
    UPROPERTY(BlueprintAssignable, Category = "MuJoCo|Events")
    FOnMjCollision OnCollision;
    
    // Internal cache for name->id mapping
    TMap<FString, int> ActuatorIndices;
    
    /** @brief Applies values from ActuatorControls map to the running m_data->ctrl buffer. */
    UFUNCTION(BlueprintCallable, Category = "MuJoCo|Runtime")
    void ApplyControls();

    // =========================================================================
    // Blueprint Runtime API — Discovery
    // =========================================================================

    /** @brief Returns an array of all UMjActuator components on this articulation. */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "MuJoCo|Runtime")
    TArray<UMjActuator*> GetActuators() const;

    /** @brief Returns an array of all UMjJoint components on this articulation. */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "MuJoCo|Runtime")
    TArray<UMjJoint*> GetJoints() const;

    /** @brief Returns an array of all UMjSensor components on this articulation. */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "MuJoCo|Runtime")
    TArray<UMjSensor*> GetSensors() const;

    /** @brief Returns an array of all UMjBody components on this articulation. */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "MuJoCo|Runtime")
    TArray<UMjBody*> GetBodies() const;

    /**
     * @brief Wakes all bodies in this articulation (MuJoCo 3.4+).
     *        No-op if sleep is disabled globally or if the articulation has not been compiled.
     */
    UFUNCTION(BlueprintCallable, Category = "MuJoCo|Sleep")
    void WakeAll();

    /**
     * @brief Forces all bodies in this articulation to sleep (MuJoCo 3.4+).
     *        No-op if sleep is disabled globally or if the articulation has not been compiled.
     */
    UFUNCTION(BlueprintCallable, Category = "MuJoCo|Sleep")
    void SleepAll();

    /** @brief Returns an array of all UMjFrame components on this articulation. */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "MuJoCo|Runtime")
    TArray<UMjFrame*> GetFrames() const;

    /** @brief Returns an array of all UMjGeom components on this articulation. */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "MuJoCo|Runtime")
    TArray<UMjGeom*> GetGeoms() const;

    /** @brief Returns the UE component names of all actuators on this articulation. */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "MuJoCo|Runtime")
    TArray<FString> GetActuatorNames() const;

    /** @brief Returns the UE component names of all joints on this articulation. */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "MuJoCo|Runtime")
    TArray<FString> GetJointNames() const;

    /** @brief Returns the UE component names of all sensors on this articulation. */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "MuJoCo|Runtime")
    TArray<FString> GetSensorNames() const;

    /** @brief Returns the UE component names of all MjBody components on this articulation. */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "MuJoCo|Runtime")
    TArray<FString> GetBodyNames() const;

    /** @brief Returns all components of a specific class that are not marked as 'bIsDefault'. */
    UFUNCTION(BlueprintCallable, Category = "MuJoCo|Discovery")
    TArray<USceneComponent*> GetRuntimeComponentsOfClass(TSubclassOf<USceneComponent> ComponentClass) const;

    /** 
     * @brief Templated helper to find all components of type T belonging to this articulation 
     * that are NOT marked as 'bIsDefault'. This is the standard way to find simulation-active components.
     */
    template<typename T>
    void GetRuntimeComponents(TArray<T*>& OutComponents) const
    {
        OutComponents.Empty();
        TArray<T*> AllComponents;
        GetComponents<T>(AllComponents);
        UWorld* MyWorld = GetWorld();
        for (T* Comp : AllComponents)
        {
            if (UMjComponent* MjComp = Cast<UMjComponent>(Comp))
            {
                // Skip default class children and SCS template components
                // that leak into PIE via GetComponents.
                if (MjComp->bIsDefault) continue;
                if (MyWorld && MjComp->GetWorld() != MyWorld) continue;
                OutComponents.Add(Comp);
            }
        }
    }

    // =========================================================================
    // Blueprint Runtime API — Component Accessors (delegate to the component)
    // =========================================================================

    /** @brief Gets the UMjActuator component by its UE component name. Returns nullptr if not found. */
    UFUNCTION(BlueprintCallable, Category = "MuJoCo|Runtime")
    UMjActuator* GetActuator(const FString& Name) const;

    /** @brief Gets the UMjJoint component by its UE component name. Returns nullptr if not found. */
    UFUNCTION(BlueprintCallable, Category = "MuJoCo|Runtime")
    UMjJoint* GetJoint(const FString& Name) const;

    /** @brief Gets the UMjSensor component by its UE component name. Returns nullptr if not found. */
    UFUNCTION(BlueprintCallable, Category = "MuJoCo|Runtime")
    UMjSensor* GetSensor(const FString& Name) const;

    /** @brief Gets the UMjBody component by its UE component name. Returns nullptr if not found. */
    UFUNCTION(BlueprintCallable, Category = "MuJoCo|Runtime")
    UMjBody* GetBody(const FString& Name) const;

    /** @brief Returns an array of all UMjTendon components on this articulation. */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "MuJoCo|Runtime")
    TArray<UMjTendon*> GetTendons() const;

    /** @brief Returns the UE component names of all tendons on this articulation. */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "MuJoCo|Runtime")
    TArray<FString> GetTendonNames() const;

    /** @brief Gets the UMjTendon component by its UE component name. Returns nullptr if not found. */
    UFUNCTION(BlueprintCallable, Category = "MuJoCo|Runtime")
    UMjTendon* GetTendon(const FString& Name) const;

    /** @brief Returns an array of all UMjEquality components on this articulation. */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "MuJoCo|Runtime")
    TArray<UMjEquality*> GetEqualities() const;

    /** @brief Returns an array of all UMjKeyframe components on this articulation. */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "MuJoCo|Runtime")
    TArray<UMjKeyframe*> GetKeyframes() const;

    /** @brief Returns the names of all keyframes on this articulation. */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "MuJoCo|Keyframes")
    TArray<FString> GetKeyframeNames() const;

    /**
     * @brief Resets simulation state to a named keyframe (one-shot teleport).
     * Sets qpos/qvel/ctrl to the keyframe's values via mj_resetDataKeyframe.
     * The robot jumps instantly to the pose. Thread-safe: schedules on the physics thread.
     * @param KeyframeName The name of the keyframe. Empty string uses first keyframe (index 0).
     * @return true if the keyframe was found and the reset was scheduled.
     */
    UFUNCTION(BlueprintCallable, Category = "MuJoCo|Keyframes")
    bool ResetToKeyframe(const FString& KeyframeName = TEXT(""));

    /**
     * @brief Starts continuously driving actuators to hold a keyframe's joint positions.
     * Uses the keyframe's ctrl values if available, otherwise computes PD targets from qpos.
     * Call StopHoldKeyframe() to release.
     * @param KeyframeName The name of the keyframe. Empty string uses first keyframe (index 0).
     * @return true if the keyframe was found and hold mode was activated.
     */
    UFUNCTION(BlueprintCallable, Category = "MuJoCo|Keyframes")
    bool HoldKeyframe(const FString& KeyframeName = TEXT(""));

    /** @brief Stops holding a keyframe. Actuators return to normal control source. */
    UFUNCTION(BlueprintCallable, Category = "MuJoCo|Keyframes")
    void StopHoldKeyframe();

    /** @brief Returns true if currently holding a keyframe pose. */
    UFUNCTION(BlueprintPure, Category = "MuJoCo|Keyframes")
    bool IsHoldingKeyframe() const { return bHoldingKeyframe; }

private:
    bool bHoldingKeyframe = false;
    bool bHoldViaQpos = false;        // true = inject qpos directly, false = use ctrl
    TArray<float> HeldKeyframeCtrl;   // ctrl values (when holding via actuators)
    TArray<float> HeldKeyframeQpos;   // qpos values (when holding via direct injection)
    int32 HeldQposOffset = 0;         // offset into d->qpos for this articulation's joints

public:
    /** @brief Gets any UMjComponent by its MuJoCo objective type and ID. */
    UMjComponent* GetComponentByMjId(mjtObj type, int32 id) const;

    /** @brief Gets a UMjBody component by its MuJoCo Body ID. */
    UFUNCTION(BlueprintCallable, Category = "MuJoCo|Runtime")
    UMjBody* GetBodyByMjId(int32 id) const;

    /** @brief Gets a UMjGeom component by its MuJoCo Geom ID. */
    UFUNCTION(BlueprintCallable, Category = "MuJoCo|Runtime")
    UMjGeom* GetGeomByMjId(int32 id) const;

    // =========================================================================
    // Blueprint Runtime API — Convenience One-Liners
    // =========================================================================

    /** @brief Sets a single actuator's control value by component name. Returns false if not found. */
    UFUNCTION(BlueprintCallable, Category = "MuJoCo|Runtime|Actuators")
    bool SetActuatorControl(const FString& ActuatorName, float Value);

    /** @brief Gets an actuator's control range [min, max] by component name. */
    UFUNCTION(BlueprintCallable, Category = "MuJoCo|Runtime|Actuators")
    FVector2D GetActuatorRange(const FString& ActuatorName) const;

    /** @brief Gets a joint's position (qpos) by component name. Returns 0 if not found. */
    UFUNCTION(BlueprintCallable, Category = "MuJoCo|Runtime|Joints")
    float GetJointAngle(const FString& JointName) const;

    /** @brief Gets a sensor's scalar reading by component name. Returns 0 if not found. */
    UFUNCTION(BlueprintCallable, Category = "MuJoCo|Runtime|Sensors")
    float GetSensorScalar(const FString& SensorName) const;

    /** @brief Gets a sensor's full reading array by component name. Returns empty if not found. */
    UFUNCTION(BlueprintCallable, Category = "MuJoCo|Runtime|Sensors")
    TArray<float> GetSensorReading(const FString& SensorName) const;


protected:
    virtual void BeginPlay() override;

    /** @brief Called when the game ends or actor is destroyed. */
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

    /** Binds twist controller input when possessed. */
    virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

public:
    /** Adds twist mapping context on possession. */
    virtual void PossessedBy(AController* NewController) override;

    /** Removes twist mapping context and zeros twist state on release. */
    virtual void UnPossessed() override;

protected:

public:
    /** @brief True if mjs_attach failed during Setup. This articulation has no compiled representation. */
    UPROPERTY(BlueprintReadOnly, Category = "MuJoCo|Status")
    bool bAttachFailed = false;

protected:
    mjVFS* m_vfs = nullptr;
    mjSpec* m_spec = nullptr;
    mjSpec* m_ChildSpec = nullptr;
    FString m_prefix;


    FMujocoSpecWrapper* m_wrapper = nullptr;

    // Component-name maps (populated in PostSetup, used by Blueprint API)
    TMap<FString, UMjActuator*> ActuatorComponentMap;
    TMap<FString, UMjJoint*>    JointComponentMap;
    TMap<FString, UMjSensor*>   SensorComponentMap;
    TMap<FString, UMjBody*>     BodyComponentMap;
    TMap<FString, UMjTendon*>   TendonComponentMap;
    TMap<FString, UMjEquality*> EqualityComponentMap;
    TMap<FString, UMjKeyframe*> KeyframeComponentMap;

    // MuJoCo ID maps (populated in PostSetup)
    TMap<int32, UMjBody*>     BodyIdMap;
    TMap<int32, UMjGeom*>     GeomIdMap;
    TMap<int32, UMjJoint*>    JointIdMap;
    TMap<int32, UMjSensor*>   SensorIdMap;
    TMap<int32, UMjTendon*>   TendonIdMap;
    TMap<int32, UMjActuator*> ActuatorIdMap;

    /** Controller cached at PostSetup (game thread) so ApplyControls can
     *  read it from the physics thread without iterating OwnedComponents —
     *  that iteration races against game-thread component mutations (e.g.
     *  the auto-created UMjTwistController) and corrupts nearby heap state. */
    UPROPERTY(Transient)
    class UMjArticulationController* CachedController = nullptr;

public:    
    virtual void Tick(float DeltaTime) override;

    /** @brief Optional: Path to an existing MuJoCo XML to import. */
    UPROPERTY(EditAnywhere, Category = "MuJoCo Import")
    FFilePath MuJoCoXMLFile;

    /**
     * @brief Simulation options for this articulation's child spec.
     * Applied to the child mjSpec's option struct during Setup().
     * mjs_attach merges these into the root spec at compile time.
     * Parsed from the MJCF <option> element during import.
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MuJoCo|Options")
    FMuJoCoOptions SimOptions;
    
    /**
     * @brief Initializes the articulation, adding its components to the provided mjSpec.
     * @param spec Pointer to the shared mjSpec.
     * @param vfs Pointer to the shared Virtual File System.
     */
    void Setup(mjSpec* Spec, mjVFS* VFS);

    /**
     * @brief Finalizes setup after compilation, resolving indices and bindings.
     * @param model Pointer to the compiled mjModel.
     * @param data Pointer to the active mjData.
     */
    void PostSetup(mjModel* Model, mjData* Data);
    
    /** @brief draws debug lines for collision geoms. */
    void DrawDebugCollision();

    /** @brief draws joint axis lines and range arcs. */
    void DrawDebugJoints();

    /** @brief draws site location markers. */
    void DrawDebugSites();
protected:
    mjModel* m_model = nullptr;
    mjData* m_data = nullptr;

public:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "MjArticulation")
    class USceneComponent* DefaultSceneRoot;

};