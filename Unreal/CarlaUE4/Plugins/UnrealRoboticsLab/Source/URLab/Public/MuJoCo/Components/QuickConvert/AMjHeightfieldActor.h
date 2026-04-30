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
#include "Components/BoxComponent.h"
#include "Components/LineBatchComponent.h"
#include "mujoco/mujoco.h"
#include "AMjHeightfieldActor.generated.h"

/**
 * @class AMjHeightfieldActor
 * @brief An actor that samples terrain height within its bounding box and registers
 *        the result as a MuJoCo heightfield asset (mjsHField) into the shared mjSpec.
 *
 * Place this actor in the level over any landscape / static geometry. The Manager
 * will auto-detect it during PreCompile() via a Cast<AMjHeightfieldActor> and call
 * Setup() to perform raycasting and spec population.
 *
 * A grid visualizer (ULineBatchComponent) is drawn in the editor viewport to preview
 * the sample-point resolution. The grid refreshes automatically whenever any property
 * is changed via the Details panel (driven by OnConstruction).
 */
UCLASS()
class URLAB_API AMjHeightfieldActor : public AActor
{
    GENERATED_BODY()

public:
    AMjHeightfieldActor();

    // -------------------------------------------------------------------------
    // Config
    // -------------------------------------------------------------------------

    /**
     * @brief Number of sample points along each axis (Resolution x Resolution grid).
     *        Higher values give more detailed terrain but increase compile cost.
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MuJoCo|Heightfield", meta = (ClampMin = "2", ClampMax = "512"))
    int32 Resolution = 64;

    /**
     * @brief Name used to register the heightfield asset in the MuJoCo spec.
     *        Must be unique per level if multiple heightfield actors are placed.
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MuJoCo|Heightfield")
    FString HFieldName = TEXT("terrain");

    /**
     * @brief Collision channel to use for downward height raycasts.
     *        Should match the channel your landscape/terrain mesh responds to.
     *        ECC_Visibility traces the visual mesh for higher fidelity.
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MuJoCo|Heightfield")
    TEnumAsByte<ECollisionChannel> ElevationTraceChannel = ECC_Visibility;

    /** @brief Force re-sampling on next compile, ignoring any cached heightfield data. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MuJoCo|Heightfield")
    bool bForceRecache = false;

    /**
     * @brief Whitelist of actors to trace against for height sampling.
     *        If non-empty, ONLY hits on these actors count — everything else is ignored.
     *        If empty, traces hit anything (with MuJoCo actors auto-filtered as usual).
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MuJoCo|Heightfield",
        meta=(ToolTip="If set, only these actors will be sampled for height. Leave empty to trace against everything."))
    TArray<TSoftObjectPtr<AActor>> TraceWhitelist;

    /**
     * @brief Thickness of the solid base below the heightfield surface, expressed
     *        as a fraction of the total elevation range. MuJoCo 'base' parameter.
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MuJoCo|Heightfield", meta = (ClampMin = "0.01", ClampMax = "1.0"))
    float BaseThickness = 0.1f;

    // -------------------------------------------------------------------------
    // Visualizer
    // -------------------------------------------------------------------------

    /** @brief If true, a grid preview is rendered in the editor viewport. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MuJoCo|Heightfield|Visualizer")
    bool bShowGrid = true;

    /** @brief Color used to draw the resolution grid lines. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MuJoCo|Heightfield|Visualizer")
    FLinearColor GridColor = FLinearColor(0.0f, 1.0f, 1.0f, 1.0f); // Cyan

    // -------------------------------------------------------------------------
    // Components
    // -------------------------------------------------------------------------

    /** @brief Root box component — defines the sampling region. Scale/move this in the editor. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "MuJoCo|Heightfield")
    UBoxComponent* BoundsBox;

    /** @brief Line batch used to draw the resolution grid in the editor. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "MuJoCo|Heightfield|Visualizer")
    ULineBatchComponent* GridLines;

    // -------------------------------------------------------------------------
    // MuJoCo Integration
    // -------------------------------------------------------------------------

    /**
     * @brief Called by AMuJoCoManager::PreCompile(). Raycasts the terrain under the
     *        bounding box, normalises heights, and registers an mjsHField + world geom
     *        into the provided spec.
     * @param Spec  Shared MuJoCo spec to populate.
     * @param VFS   Shared virtual file system (not used; kept for signature consistency).
     */
    void Setup(mjSpec* Spec, mjVFS* VFS);

    /**
     * @brief Called by AMuJoCoManager::PostCompile(). Currently a no-op for static terrain.
     *        Reserved for future live-update or runtime query support.
     * @param Model  Compiled mjModel.
     * @param Data   Active mjData.
     */
    void PostSetup(mjModel* Model, mjData* Data);

protected:
    /** @brief Called when the actor is constructed or a property changes in the editor. */
    virtual void OnConstruction(const FTransform& Transform) override;

private:
    /** @brief Rebuilds the GridLines component to reflect the current Resolution and bounds. */
    void RebuildGridVisualizer();

    /** @brief Traces downward to find the terrain height, ignoring MuJoCo actors. */
    float SampleHeightAt(const FVector2D& WorldXY, const FBox& Bounds) const;

    /** @brief Returns the cache file path for this heightfield. */
    FString GetCacheFilePath() const;

    /** @brief Computes a cache key from actor transform, resolution, and bounds. */
    FString ComputeCacheKey() const;

    /** @brief Saves sampled heightfield data to a binary cache file. */
    bool SaveCache(const TArray<float>& NormHeights, float MinH, float ElevRange,
        const FBox& Bounds, const FString& CacheKey) const;

    /** @brief Loads cached heightfield data. Returns true if cache is valid. */
    bool LoadCache(TArray<float>& OutNormHeights, float& OutMinH, float& OutElevRange,
        FBox& OutBounds, const FString& ExpectedCacheKey) const;
};
