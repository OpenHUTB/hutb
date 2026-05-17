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
#include "Components/ActorComponent.h"
#include "mujoco/mujoco.h"
#include "MuJoCo/Core/MjSimOptions.h"
#include <functional>
#include <atomic>
#include "MjPhysicsEngine.generated.h"

// Forward declarations
class AMjArticulation;
class AMjHeightfieldActor;
class UMjQuickConvertComponent;
class UMjSimulationState;

// Shared enum: control source for articulations and physics engine
UENUM(BlueprintType)
enum class EControlSource : uint8
{
    ZMQ UMETA(DisplayName = "External (ZMQ)"),
    UI  UMETA(DisplayName = "Internal (UI)")
};

/**
 * @class UMjPhysicsEngine
 * @brief Core physics engine component that owns the MuJoCo simulation lifecycle.
 *
 * Manages mjSpec compilation, mjModel/mjData ownership, the async physics loop,
 * and callback registration for pre/post step hooks.
 */
UCLASS(ClassGroup=(MuJoCo), meta=(BlueprintSpawnableComponent))
class URLAB_API UMjPhysicsEngine : public UActorComponent
{
    GENERATED_BODY()

public:
    UMjPhysicsEngine();

    // --- MuJoCo Core Pointers ---

    /** @brief Pointer to the MuJoCo specification structure. */
    mjSpec* m_spec = nullptr;

    /** @brief MuJoCo Virtual File System instance. */
    mjVFS m_vfs;

    /** @brief Pointer to the compiled MuJoCo model. */
    mjModel* m_model = nullptr;

    /** @brief Pointer to the active MuJoCo simulation data. */
    mjData* m_data = nullptr;

    // --- Public Getters ---

    mjModel* GetModel() const { return m_model; }
    mjData* GetData() const { return m_data; }
    mjSpec* GetSpec() const { return m_spec; }

    // --- Thread Synchronization ---

    FCriticalSection CallbackMutex;

    /** @brief Flag to signal the async task to stop. Atomic for thread safety. */
    std::atomic<bool> bShouldStopTask{false};

    /** @brief Future for the async physics thread, used to wait for exit in EndPlay. */
    TFuture<void> AsyncPhysicsFuture;

    // --- Step Callbacks ---

    /**
     * @brief Delegate for custom physics stepping.
     * If bound, this replaces the default mj_step(). Used by Replay System.
     */
    using FMujocoStepCallback = std::function<void(mjModel*, mjData*)>;
    FMujocoStepCallback CustomStepHandler;

    /**
     * @brief Delegate called immediately after mj_step (or custom step).
     * Used for Recording State.
     */
    using FMujocoPostStepCallback = std::function<void(mjModel*, mjData*)>;
    FMujocoPostStepCallback OnPostStep;

    void SetCustomStepHandler(FMujocoStepCallback Handler);
    void ClearCustomStepHandler();

    // --- Pending State (Thread-Safe Atomics) ---

    /** @brief Thread-safe flag to trigger a reset on the next physics step. */
    std::atomic<bool> bPendingReset{false};

    /** @brief Thread-safe flag to trigger a restore on the next physics step. */
    std::atomic<bool> bPendingRestore{false};

    /** @brief Plain-data copy of the state to restore. */
    TArray<double> PendingStateVector;
    int32 PendingStateMask = 0;

    // --- Simulation Options ---

    /** @brief Simulation Options (Timestep, Gravity, etc.). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MuJoCo|Options")
    FMuJoCoOptions Options;

    /** @brief Simulation speed percentage (0-100). 100 = realtime, 50 = half speed. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MuJoCo|Options", meta=(ClampMin="5", ClampMax="100"))
    float SimSpeedPercent = 100.0f;

    /** @brief If true, the simulation loop skips stepping the physics. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MuJoCo|Status")
    bool bIsPaused = true;

    /** @brief If true, saves compiled scene XML and MJB to Saved/URLab/ on each compile. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MuJoCo|Debug")
    bool bSaveDebugXml = false;

    /** @brief Global default control source (ZMQ or UI). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MuJoCo|Runtime")
    EControlSource ControlSource;

    // --- Registered Scene Objects ---

    /** @brief List of custom physics components registered with this manager. */
    UPROPERTY()
    TArray<UMjQuickConvertComponent*> m_MujocoComponents;

    /** @brief List of articulations (multi-body structures) registered with this manager. */
    UPROPERTY()
    TArray<AMjArticulation*> m_articulations;

    /** @brief List of heightfield actors registered with this manager. */
    UPROPERTY()
    TArray<AMjHeightfieldActor*> m_heightfieldActors;

    /** @brief O(1) articulation lookup. Key = actor name. */
    TMap<FString, AMjArticulation*> m_ArticulationMap;

    // --- Error Reporting ---

    /** @brief Error string from the most recent Compile() call. Empty on success. */
    FString m_LastCompileError;

    // --- Compilation ---

    /** @brief Compiles the aggregated mjSpec into an mjModel and initializes mjData. */
    void Compile();

    /** @brief Scans the scene for MuJoCo components and articulations to populate m_spec. */
    void PreCompile();

    /** @brief Finalizes setup after compilation (actuator maps, PostSetup calls). */
    void PostCompile();

    /** @brief Applies the settings from Options to m_model->opt. */
    void ApplyOptions();

    // --- Runtime ---

    /** @brief Starts the asynchronous thread for stepping the MuJoCo simulation. */
    void RunMujocoAsync();

    /** @brief Pauses or Resumes the physics simulation. */
    void SetPaused(bool bPaused);

    /** @brief Checks if the simulation is currently running (initialized and not paused). */
    bool IsRunning() const;

    /** @brief Checks if the MuJoCo model is compiled and data is allocated. */
    bool IsInitialized() const;

    /** @brief Gets the current MuJoCo simulation time in seconds. */
    float GetSimTime() const;

    /** @brief Gets the simulation timestep from Options (dt). */
    float GetTimestep() const;

    /** @brief Resets the MuJoCo simulation data to initial state. */
    void ResetSimulation();

    /** @brief Steps the simulation synchronously on the calling thread. */
    void StepSync(int32 NumSteps);

    /** @brief Re-runs Compile() and restarts the async simulation loop. */
    bool CompileModel();

    /** @brief Captures a full state snapshot of the current simulation. */
    UMjSimulationState* CaptureSnapshot();

    /** @brief Schedules a simulation state restore for the next physics step. */
    void RestoreSnapshot(UMjSimulationState* Snapshot);

    /** @brief Sets the global default control source. */
    void SetControlSource(EControlSource NewSource);

    /** @brief Gets the current control source. */
    EControlSource GetControlSource() const;

    /** @brief Gets a registered articulation by its Actor name. */
    AMjArticulation* GetArticulation(const FString& ActorName) const;

    /** @brief Gets all registered articulations. */
    TArray<AMjArticulation*> GetAllArticulations() const;

    /** @brief Gets all registered QuickConvert components. */
    TArray<UMjQuickConvertComponent*> GetAllQuickComponents() const;

    /** @brief Gets all registered Heightfield actors. */
    TArray<AMjHeightfieldActor*> GetAllHeightfields() const;

    /** @brief Returns the error string from the most recent failed compile. */
    FString GetLastCompileError() const;

    // --- Callback Registration ---

    using FPhysicsCallback = std::function<void(mjModel*, mjData*)>;

    /** @brief Register a callback to be invoked before each physics step. */
    void RegisterPreStepCallback(FPhysicsCallback Callback);

    /** @brief Register a callback to be invoked after each physics step. */
    void RegisterPostStepCallback(FPhysicsCallback Callback);

    /** @brief Clear all registered pre/post step callbacks. */
    void ClearCallbacks();

private:
    TArray<FPhysicsCallback> PreStepCallbacks;
    TArray<FPhysicsCallback> PostStepCallbacks;
};
