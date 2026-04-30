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
#include "MuJoCo/Core/MjArticulation.h"
#include "MjReplayTypes.generated.h"

/**
 * @struct FMjBodyKinematics
 * @brief Stores kinematics (Position/Velocity) for a single joint or body (if free joint).
 * We store as double to prevent drift during playback, though float is often sufficient.
 */
USTRUCT(BlueprintType)
struct FMjBodyKinematics
{
    GENERATED_BODY()

    /** @brief Joint positions (qpos). Size depends on joint type (1 for hinge, 7 for free). */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    TArray<double> QPos;

    /** @brief Joint velocities (qvel). */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    TArray<double> QVel;
};

/**
 * @struct FMjReplayFrame
 * @brief A snapshot of the entire simulation physics state at a specific time.
 */
USTRUCT(BlueprintType)
struct FMjReplayFrame
{
    GENERATED_BODY()

    /** @brief Simulation time in seconds. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    double Timestamp = 0.0;

    /**
     * @brief Map of Joint Name -> Kinematics.
     * Name-based storage allows replaying motion even if the scene hierarchy changes
     * (e.g. new obstacles added), as long as the robot joints remain named the same.
     */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    TMap<FString, FMjBodyKinematics> JointStates;

    /** @brief Camera world position at this frame (zero if no camera was recording). */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    FVector CameraPosition = FVector::ZeroVector;

    /** @brief Camera world rotation at this frame. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    FRotator CameraRotation = FRotator::ZeroRotator;

    /** @brief Whether this frame has valid camera data. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    bool bHasCameraData = false;
};

/**
 * @struct FReplaySession
 * @brief A named collection of replay frames with source metadata.
 */
USTRUCT(BlueprintType)
struct FReplaySession
{
    GENERATED_BODY()

    /** @brief The replay frames for this session. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    TArray<FMjReplayFrame> Frames;

    /** @brief Source file path (empty for live recording). */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    FString SourceFile;
};

/**
 * @struct FReplayArticulationBinding
 * @brief Per-articulation replay settings (enabled, relative position).
 */
USTRUCT()
struct FReplayArticulationBinding
{
    GENERATED_BODY()

    UPROPERTY()
    TWeakObjectPtr<AMjArticulation> Articulation;

    /** @brief Whether this articulation participates in the replay. */
    UPROPERTY()
    bool bEnabled = true;

    /** @brief If true, free joint position is applied relative to the actor's placed position. */
    UPROPERTY()
    bool bRelativePosition = true;

    /** @brief The model joint name prefix for this articulation (e.g. "g1_29dof_C_UAID_..._"). */
    FString JointPrefix;

    /** @brief Cached initial position when replay starts (editor-placed). */
    FVector InitialPosition = FVector::ZeroVector;

    /** @brief Cached CSV start position (from first frame's free joint qpos, in MuJoCo meters). */
    FVector CsvStartPosition = FVector::ZeroVector;

    /** @brief Cached initial MuJoCo qpos position at replay start (in MuJoCo meters). */
    FVector InitialMjPosition = FVector::ZeroVector;

    /** @brief Whether initial positions have been captured. */
    bool bInitialsCaptured = false;
};
