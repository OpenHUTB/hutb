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

#include "MuJoCo/Components/QuickConvert/MjQuickConvertComponent.h"
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MuJoCo/Core/MjArticulation.h"
#include "AMjManager.generated.h"

// Forward declarations
class AMjHeightfieldActor;
class UMjPhysicsEngine;
class UMjDebugVisualizer;
class UMjNetworkManager;
class UMjInputHandler;
class UMjPerturbation;
class UMjSimulationState;

/**
 * @class AAMjManager
 * @brief Thin coordinator actor for the MuJoCo simulation within Unreal Engine.
 *
 * Owns subsystem components and delegates to them:
 * - UMjPhysicsEngine: simulation lifecycle, model/data, options, async loop
 * - UMjDebugVisualizer: debug drawing, collision wireframes
 * - UMjNetworkManager: ZMQ components, camera streaming
 * - UMjInputHandler: keyboard hotkeys
 *
 * External code accesses subsystem state via Manager->PhysicsEngine->X, etc.
 */
UCLASS()
class URLAB_API AAMjManager : public AActor
{
	GENERATED_BODY()

public:
	/**
     * @brief Default constructor.
     */
	AAMjManager();

    // --- Physics Engine Component (Phase 1 extraction) ---
    /** @brief Core physics engine component that owns the MuJoCo simulation lifecycle. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "MuJoCo")
    UMjPhysicsEngine* PhysicsEngine;

    // --- Debug Visualizer Component (Phase 2 extraction) ---
    /** @brief Debug visualization component for contact forces, collision wireframes, etc. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "MuJoCo")
    UMjDebugVisualizer* DebugVisualizer;

    // --- Network Manager Component (Phase 3 extraction) ---
    /** @brief Network component managing ZMQ discovery and camera streaming. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "MuJoCo")
    UMjNetworkManager* NetworkManager;

    // --- Input Handler Component (Phase 4 extraction) ---
    /** @brief Input handler component for keyboard hotkeys. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "MuJoCo")
    UMjInputHandler* InputHandler;

    /** @brief Mouse-driven body perturbation (simulate-style Ctrl+LMB/RMB drag). */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "MuJoCo")
    UMjPerturbation* Perturbation;
    
    // --- Global Access ---
    /** 
     * @brief Singleton instance pointer. Set in BeginPlay, cleared in EndPlay. 
     * Use GetManager() to access safely from Blueprints.
     */
    static AAMjManager* Instance;

    /** 
     * @brief Gets the active MuJoCo Manager instance.
     * @return Pointer to the manager, or nullptr if not existing.
     */
    UFUNCTION(BlueprintPure, Category = "MuJoCo|Global", meta=(DisplayName="Get MuJoCo Manager"))
    static AAMjManager* GetManager();

    // --- State Control (delegates to PhysicsEngine) ---

    /** @brief Pauses or Resumes the physics simulation. */
    UFUNCTION(BlueprintCallable, Category = "MuJoCo|Control")
    void SetPaused(bool bPaused);

    /** @brief Checks if the simulation is currently running (initialized and not paused). */
    UFUNCTION(BlueprintPure, Category = "MuJoCo|Status")
    bool IsRunning() const;

    /** @brief Checks if the MuJoCo model is compiled and data is allocated. */
    UFUNCTION(BlueprintPure, Category = "MuJoCo|Status")
    bool IsInitialized() const;

    // --- Articulation Access ---

    /** @brief Gets a registered articulation by its Actor name. Returns nullptr if not found. */
    UFUNCTION(BlueprintCallable, Category = "MuJoCo|Global")
    AMjArticulation* GetArticulation(const FString& ActorName) const;

    /** @brief Gets all registered articulations. */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "MuJoCo|Global")
    TArray<AMjArticulation*> GetAllArticulations() const;

    /** @brief Gets all registered QuickConvert components. */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "MuJoCo|Global")
    TArray<UMjQuickConvertComponent*> GetAllQuickComponents() const;

    /** @brief Gets all registered Heightfield actors. */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "MuJoCo|Global")
    TArray<AMjHeightfieldActor*> GetAllHeightfields() const;

    /** @brief Gets the current MuJoCo simulation time (m_data->time) in seconds. */
    UFUNCTION(BlueprintPure, Category = "MuJoCo|Status")
    float GetSimTime() const;

    /** @brief Gets the simulation timestep from Options (dt). */
    UFUNCTION(BlueprintPure, Category = "MuJoCo|Status")
    float GetTimestep() const;

    /** @brief If true, automatically creates and adds the MjSimulate dashboard widget to the viewport on BeginPlay. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MuJoCo|UI")
    bool bAutoCreateSimulateWidget = true;

    /** @brief Toggles visibility of the MjSimulate dashboard widget. Bound to Tab key. */
    UFUNCTION(BlueprintCallable, Category = "MuJoCo|UI")
    void ToggleSimulateWidget();

    /** @brief Reference to the auto-created simulate widget (if any). */
    UPROPERTY()
    UUserWidget* SimulateWidget = nullptr;

protected:

    /** @brief O(1) articulation lookup built in PostCompile(). Key = actor name. */
    TMap<FString, AMjArticulation*> m_ArticulationMap;

	/**
     * @brief Called when the game starts or when spawned.
     * Initiates the compilation and starts the async simulation loop.
     */
	virtual void BeginPlay() override;

    /**
     * @brief Called when the game ends or the actor is destroyed.
     * Handles cleanup of MuJoCo resources and stops the async task.
     * @param EndPlayReason The reason for end of play.
     */
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

public:	
	/**
     * @brief Called every frame.
     * Manages synchronization of control inputs on the Game Thread.
     * @param DeltaTime Time since the last frame.
     */
	virtual void Tick(float DeltaTime) override;
    
    /** @brief List of custom physics components registered with this manager. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mujoco Physics|Objects")
    TArray<UMjQuickConvertComponent*> m_MujocoComponents;

    /** @brief List of articulations (multi-body structures) registered with this manager. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mujoco Physics|Objects")
	TArray<AMjArticulation*> m_articulations;

    /** @brief List of heightfield actors registered with this manager. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mujoco Physics|Objects")
    TArray<AMjHeightfieldActor*> m_heightfieldActors;

    /**
     * @brief Compiles the aggregated mjSpec into an mjModel and initializes mjData.
     * Calls PreCompile and PostCompile during the process.
     */
    void Compile();

    /**
     * @brief Scans the scene for Mujoco components and Articulations to populate m_spec.
     * Finds attached ZmqComponents.
     */
    void PreCompile();

    /**
     * @brief Finalizes setup after compilation, such as initializing actuator maps.
     * Calls PostSetup on all registered components.
     */
    void PostCompile();
    
    // --- Replay ---

    UFUNCTION(CallInEditor, Category = "MuJoCo|Replay")
    void StartRecording();

    UFUNCTION(CallInEditor, Category = "MuJoCo|Replay")
    void StopRecording();

    UFUNCTION(CallInEditor, Category = "MuJoCo|Replay")
    void StartReplay();

    UFUNCTION(CallInEditor, Category = "MuJoCo|Replay")
    void StopReplay();


    // --- Simulation Management ---

    /** 
     * @brief Resets the MuJoCo simulation data (qpos, qvel, time) to initial keyframe (or zero).
     */
    UFUNCTION(BlueprintCallable, CallInEditor, Category = "MuJoCo|Control")
    void ResetSimulation();

    // --- Delegating Methods (thin wrappers around PhysicsEngine) ---

    /** @brief Returns the error string from the most recent failed compile, or empty if last compile succeeded. */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "MuJoCo|Status")
    FString GetLastCompileError() const;

    /** @brief Pauses the async loop, steps N times synchronously, then restores pause state. */
    UFUNCTION(BlueprintCallable, Category = "MuJoCo|Control")
    void StepSync(int32 NumSteps);

    /** @brief Re-runs Compile() and restarts the async simulation loop. */
    UFUNCTION(BlueprintCallable, Category = "MuJoCo|Control")
    bool CompileModel();

    /** @brief Captures a full state snapshot of the current simulation. */
    UFUNCTION(BlueprintCallable, Category = "MuJoCo|Snapshot")
    UMjSimulationState* CaptureSnapshot();

    /** @brief Schedules a simulation state restore for the next physics step. */
    UFUNCTION(BlueprintCallable, Category = "MuJoCo|Snapshot")
    void RestoreSnapshot(UMjSimulationState* Snapshot);
};
