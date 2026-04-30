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
#include "GameFramework/Actor.h"
#include "Replay/MjReplayTypes.h"
#include "mujoco/mujoco.h"
#include <atomic>
#include "MjReplayManager.generated.h"

// Forward Declaration
class AAMjManager;

UCLASS()
class URLAB_API AMjReplayManager : public AActor
{
	GENERATED_BODY()

public:
	AMjReplayManager();

protected:
	virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

public:
	virtual void Tick(float DeltaTime) override;

    // --- Control API ---

    UFUNCTION(BlueprintCallable, CallInEditor, Category = "URLab|Replay")
    void StartRecording();

    UFUNCTION(BlueprintCallable, CallInEditor, Category = "URLab|Replay")
    void StopRecording();

    UFUNCTION(BlueprintCallable, CallInEditor, Category = "URLab|Replay")
    void StartReplay();

    UFUNCTION(BlueprintCallable, CallInEditor, Category = "URLab|Replay")
    void StopReplay();

    UFUNCTION(BlueprintCallable, CallInEditor, Category = "URLab|Replay")
    void ClearRecording();

    UFUNCTION(BlueprintCallable, CallInEditor, Category = "URLab|Replay")
    bool SaveRecordingToFile(FString FileName);

    UFUNCTION(BlueprintCallable, CallInEditor, Category = "URLab|Replay")
    bool LoadRecordingFromFile(FString FileName);

    // --- CSV Import ---

    /** @brief Load joint trajectory from a CSV file. Header row must contain joint names matching MuJoCo model. */
    UFUNCTION(BlueprintCallable, CallInEditor, Category = "URLab|Replay")
    bool LoadFromCSV(FString FilePath, float Timestep = 0.002f);

    /** @brief Opens a file dialog to browse for a CSV file, then loads it. */
    UFUNCTION(BlueprintCallable, CallInEditor, Category = "URLab|Replay")
    bool BrowseAndLoadCSV(float Timestep = 0.002f);

    /** @brief Opens a save dialog and saves the active session to a JSON file. */
    UFUNCTION(BlueprintCallable, CallInEditor, Category = "URLab|Replay")
    bool BrowseAndSaveRecording();

    // --- Session Management ---

    /** @brief Get names of all loaded replay sessions. */
    UFUNCTION(BlueprintCallable, Category = "URLab|Replay")
    TArray<FString> GetSessionNames() const;

    /** @brief Set the active session for playback. */
    UFUNCTION(BlueprintCallable, Category = "URLab|Replay")
    void SetActiveSession(const FString& Name);

    /** @brief Get the currently active session name. */
    UFUNCTION(BlueprintCallable, Category = "URLab|Replay")
    FString GetActiveSessionName() const { return ActiveSessionName; }

    /** @brief Get the number of loaded sessions. */
    UFUNCTION(BlueprintCallable, Category = "URLab|Replay")
    int32 GetSessionCount() const { return Sessions.Num(); }

    /** @brief Rebuild articulation bindings by matching scene articulations against active session. */
    void RebuildArticulationBindings();

    /** @brief Get the articulation bindings for UI display. */
    TArray<FReplayArticulationBinding>& GetArticulationBindings() { return ArticulationBindings; }

    // --- Settings ---

    /** @brief Maximum duration of recording in seconds. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "URLab|Replay")
    float MaxRecordDuration = 60.0f;

    // --- State ---

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "URLab|Replay")
    bool bIsRecording = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "URLab|Replay")
    bool bIsReplaying = false;

    /** @brief If true, starts recording automatically on BeginPlay. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "URLab|Replay")
    bool bAutoRecord = false;

    /** @brief Current playback time (0 to Duration). Updated on physics thread, read on game thread. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "URLab|Replay")
    float PlaybackTime = 0.0f;

    /** @brief Enable linear interpolation between replay frames for smoother playback. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "URLab|Replay")
    bool bInterpolateFrames = true;

    /** @brief Physics-thread playback time. Copied to PlaybackTime on game thread for UI. */
    std::atomic<double> PhysicsPlaybackTime{0.0};

    // --- Session Storage ---

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "URLab|Replay")
    FString ActiveSessionName;

    static const FString LiveSessionName;

    TMap<FString, FReplaySession> Sessions;

    /** @brief Per-articulation replay bindings. */
    TArray<FReplayArticulationBinding> ArticulationBindings;

    // Ptr to Global Manager
    AAMjManager* Manager = nullptr;

private:
    // --- Accessors ---
    TArray<FMjReplayFrame>& GetActiveFrames();
    TArray<FMjReplayFrame>& GetLiveFrames();

    // --- Physics Thread Callbacks ---
    void OnPostStep(mjModel* m, mjData* d);
    void OnReplayStep(mjModel* m, mjData* d);

    // Cached Layout for optimization (Joint IDs to Names)
    TArray<FString> CachedJointNames;
    bool bCacheValid = false;
    void UpdateCache(mjModel* m);

    /** @brief Tracks whether the current replay has emitted its first frame. Reset on StartReplay(). */
    bool bFirstReplayFrame = true;

    /** @brief Throttles periodic replay logging to once per second. */
    double LastReplayLogTime = 0.0;
};
