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
#include "mujoco/mujoco.h"
#include "MjSimOptions.generated.h"

UENUM(BlueprintType)
enum class EMjIntegrator : uint8
{
    Euler        = 0,
    RK4          = 1,
    Implicit     = 2,
    ImplicitFast = 3
};

UENUM(BlueprintType)
enum class EMjCone : uint8
{
    Pyramidal = 0,
    Elliptic  = 1
};

UENUM(BlueprintType)
enum class EMjSolver : uint8
{
    PGS    = 0,
    CG     = 1,
    Newton = 2
};

/**
 * @struct FMuJoCoOptions
 * @brief MuJoCo simulation options (mjOption).
 *
 * Used in two contexts:
 * - On AMjArticulation: ALL fields are written to the child spec's option.
 *   The bOverride_* toggles are ignored (all values apply).
 * - On AAMjManager: acts as post-compile overrides on m_model->opt.
 *   Only fields with bOverride_* = true are applied; others keep the
 *   compiled model's values (from the articulation specs).
 */
USTRUCT(BlueprintType)
struct URLAB_API FMuJoCoOptions
{
    GENERATED_BODY()

    // --- Timing ---

    UPROPERTY(EditAnywhere, Category = "MuJoCo Options", meta=(InlineEditConditionToggle))
    bool bOverride_Timestep = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MuJoCo Options", meta=(EditCondition="bOverride_Timestep"))
    float Timestep = 0.002f;

    UPROPERTY(EditAnywhere, Category = "MuJoCo Options", meta=(InlineEditConditionToggle))
    bool bOverride_MemoryMB = false;

    /** @brief MuJoCo spec-level arena size in megabytes. Maps to
     *  applied pre-compile (the unified arena is sized at mjData creation). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MuJoCo Options",
        meta=(EditCondition="bOverride_MemoryMB", ClampMin="1"))
    int32 MemoryMB = 100;

    // --- Physics Environment ---

    UPROPERTY(EditAnywhere, Category = "MuJoCo Options", meta=(InlineEditConditionToggle))
    bool bOverride_Gravity = false;

    /** @brief Gravity in UE coordinates (cm/s²). Converted to MuJoCo (m/s², Y-negated) at apply time. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MuJoCo Options", meta=(EditCondition="bOverride_Gravity"))
    FVector Gravity = FVector(0.0f, 0.0f, -981.0f);

    UPROPERTY(EditAnywhere, Category = "MuJoCo Options", meta=(InlineEditConditionToggle))
    bool bOverride_Wind = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MuJoCo Options", meta=(EditCondition="bOverride_Wind"))
    FVector Wind = FVector(0.0f, 0.0f, 0.0f);

    UPROPERTY(EditAnywhere, Category = "MuJoCo Options", meta=(InlineEditConditionToggle))
    bool bOverride_Magnetic = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MuJoCo Options", meta=(EditCondition="bOverride_Magnetic"))
    FVector Magnetic = FVector(0.0f, -0.5f, 0.0f);

    UPROPERTY(EditAnywhere, Category = "MuJoCo Options", meta=(InlineEditConditionToggle))
    bool bOverride_Density = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MuJoCo Options", meta=(EditCondition="bOverride_Density"))
    float Density = 0.0f;

    UPROPERTY(EditAnywhere, Category = "MuJoCo Options", meta=(InlineEditConditionToggle))
    bool bOverride_Viscosity = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MuJoCo Options", meta=(EditCondition="bOverride_Viscosity"))
    float Viscosity = 0.0f;

    // --- Solver ---

    UPROPERTY(EditAnywhere, Category = "MuJoCo Options", meta=(InlineEditConditionToggle))
    bool bOverride_Impratio = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MuJoCo Options", meta=(EditCondition="bOverride_Impratio"))
    float Impratio = 1.0f;

    UPROPERTY(EditAnywhere, Category = "MuJoCo Options", meta=(InlineEditConditionToggle))
    bool bOverride_Tolerance = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MuJoCo Options", meta=(EditCondition="bOverride_Tolerance"))
    float Tolerance = 1e-8f;

    UPROPERTY(EditAnywhere, Category = "MuJoCo Options", meta=(InlineEditConditionToggle))
    bool bOverride_Iterations = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MuJoCo Options", meta=(EditCondition="bOverride_Iterations"))
    int Iterations = 100;

    UPROPERTY(EditAnywhere, Category = "MuJoCo Options", meta=(InlineEditConditionToggle))
    bool bOverride_LsIterations = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MuJoCo Options", meta=(EditCondition="bOverride_LsIterations"))
    int LsIterations = 50;

    UPROPERTY(EditAnywhere, Category = "MuJoCo Options", meta=(InlineEditConditionToggle))
    bool bOverride_Integrator = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MuJoCo Options", meta=(EditCondition="bOverride_Integrator"))
    EMjIntegrator Integrator = EMjIntegrator::Euler;

    UPROPERTY(EditAnywhere, Category = "MuJoCo Options", meta=(InlineEditConditionToggle))
    bool bOverride_Cone = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MuJoCo Options", meta=(EditCondition="bOverride_Cone"))
    EMjCone Cone = EMjCone::Pyramidal;

    UPROPERTY(EditAnywhere, Category = "MuJoCo Options", meta=(InlineEditConditionToggle))
    bool bOverride_Solver = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MuJoCo Options", meta=(EditCondition="bOverride_Solver"))
    EMjSolver Solver = EMjSolver::Newton;

    UPROPERTY(EditAnywhere, Category = "MuJoCo Options", meta=(InlineEditConditionToggle))
    bool bOverride_NoslipIterations = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MuJoCo Options", meta=(EditCondition="bOverride_NoslipIterations"))
    int NoslipIterations = 0;

    UPROPERTY(EditAnywhere, Category = "MuJoCo Options", meta=(InlineEditConditionToggle))
    bool bOverride_NoslipTolerance = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MuJoCo Options", meta=(EditCondition="bOverride_NoslipTolerance"))
    float NoslipTolerance = 1e-6f;

    UPROPERTY(EditAnywhere, Category = "MuJoCo Options", meta=(InlineEditConditionToggle))
    bool bOverride_CCD_Iterations = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MuJoCo Options", meta=(EditCondition="bOverride_CCD_Iterations"))
    int CCD_Iterations = 0;

    UPROPERTY(EditAnywhere, Category = "MuJoCo Options", meta=(InlineEditConditionToggle))
    bool bOverride_CCD_Tolerance = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MuJoCo Options", meta=(EditCondition="bOverride_CCD_Tolerance"))
    float CCD_Tolerance = 1e-6f;

    // --- Multi-CCD (MuJoCo 3.0+) ---

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MuJoCo Options|Collision",
        meta=(ToolTip="Enable multi-contact convex collision detection. Useful for flat surfaces (e.g. mesh-mesh) where single-point contact causes sliding or wobbling."))
    bool bEnableMultiCCD = false;

    // --- Sleep (MuJoCo 3.4+) ---

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MuJoCo Options|Sleep")
    bool bEnableSleep = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MuJoCo Options|Sleep",
        meta=(EditCondition="bEnableSleep"))
    float SleepTolerance = 1e-4f;

    /**
     * @brief Writes ALL option values to an mjSpec's option struct.
     * Used by articulations to set their child spec options.
     * Applies UE→MJ coordinate conversion for vectors.
     */
    void ApplyToSpec(mjSpec* Spec) const;

    /**
     * @brief Applies only overridden values to a compiled mjModel.
     * Used by the manager for post-compile runtime overrides.
     */
    void ApplyOverridesToModel(mjModel* Model) const;
};
