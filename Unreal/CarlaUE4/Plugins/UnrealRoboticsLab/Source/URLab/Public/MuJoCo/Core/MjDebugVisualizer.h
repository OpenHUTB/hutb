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
#include "MuJoCo/Core/MjDebugTypes.h"
#include "MuJoCo/Components/Sensors/MjCameraTypes.h"
#include "MjDebugVisualizer.generated.h"

// Forward declarations
class AAMjManager;
class UMaterialInterface;
class UMjCamera;
struct FMuJoCoDebugData;

/** Per-body visual overlay mode applied during PIE. */
UENUM(BlueprintType)
enum class EMjDebugShaderMode : uint8
{
    Off                     UMETA(DisplayName="Off"),
    Island                  UMETA(DisplayName="Constraint Islands"),
    InstanceSegmentation    UMETA(DisplayName="Instance Segmentation (per-body)"),
    SemanticSegmentation    UMETA(DisplayName="Semantic Segmentation (per-actor)")
};


/**
 * @class UMjDebugVisualizer
 * @brief Handles debug visualization for the MuJoCo simulation (contact forces, collision wireframes, etc.).
 */
UCLASS(ClassGroup=(MuJoCo), meta=(BlueprintSpawnableComponent))
class URLAB_API UMjDebugVisualizer : public UActorComponent
{
    GENERATED_BODY()

public:
    UMjDebugVisualizer();

    virtual void BeginPlay() override;
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

    // --- Debug rendering params ---

    /** @brief If true, draws contact force visualization. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category = "MuJoCo|Debug")
    bool bShowDebug = false;

    /** @brief Scaling factor for contact force visualization. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category = "MuJoCo|Debug")
    float DebugForceScale = 0.1f;

    /** @brief Maximum force value for clamping visual size. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category = "MuJoCo|Debug")
    float DebugMaxForce = 1000.0f;

    /** @brief Size of the drawn contact point. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category = "MuJoCo|Debug")
    float DebugContactPointSize = 5.0f;

    /** @brief Base thickness of the contact force arrow. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category = "MuJoCo|Debug")
    float DebugContactArrowThickness = 1.0f;

    // --- Global toggle flags ---

    /** @brief Toggles debug collision drawing globally for all articulations. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category = "MuJoCo|Debug")
    bool bGlobalDrawDebugCollision = false;

    /** @brief Toggles debug joint axis/range drawing globally for all articulations. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category = "MuJoCo|Debug")
    bool bGlobalDrawDebugJoints = false;

    /** @brief Toggles debug Group 3 drawing globally for all articulations. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category = "MuJoCo|Debug")
    bool bGlobalShowGroup3 = false;

    /** @brief Toggles debug collision drawing globally for all QuickConvert components. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category = "MuJoCo|Debug")
    bool bGlobalQuickConvertCollision = false;

    /** @brief Per-body overlay shader mode (Island / Segmentation / Off). Cycled via key 6. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MuJoCo|Debug")
    EMjDebugShaderMode DebugShaderMode = EMjDebugShaderMode::Off;

    /** @brief When true, sleeping bodies are dimmed + desaturated on top of the active shader mode. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MuJoCo|Debug")
    bool bModulateBySleep = true;

    /** @brief Value (brightness) multiplier applied to sleeping bodies. MuJoCo upstream uses 0.6; default 0.35 dims clearly while keeping hue visible. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MuJoCo|Debug", meta=(ClampMin="0.05", ClampMax="1.0"))
    float SleepValueScale = 0.35f;

    /** @brief Saturation multiplier applied to sleeping bodies. Keep high to preserve hue identity; lower toward 0 to desaturate sleeping bodies to grey. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MuJoCo|Debug", meta=(ClampMin="0.0", ClampMax="1.0"))
    float SleepSaturationScale = 0.9f;

    /** @brief Toggles smooth-tube tendon/muscle rendering. Toggled via key 7. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MuJoCo|Debug")
    bool bGlobalDrawTendons = false;

    /** @brief Tube radius (cm). Muscle activation scales this up to ~2x per-tendon. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MuJoCo|Debug", meta=(ClampMin="0.05", ClampMax="5.0"))
    float TendonTubeRadius = 0.25f;

    /** @brief When a tendon wraps around a cylinder/sphere geom, insert this many intermediate points along the wrap arc. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MuJoCo|Debug", meta=(ClampMin="0", ClampMax="16"))
    int32 TendonArcSubdivisions = 6;

    // --- Thread-safe debug data ---
    FMuJoCoDebugData DebugData;
    FCriticalSection DebugMutex;

    // --- Methods ---

    /** @brief Captures debug info from m_data to DebugData. Called on Physics Thread. */
    void CaptureDebugData();

    /** @brief Applies global debug visibility settings to all active articulations. */
    void UpdateAllGlobalVisibility();

    /** @brief Toggle contact force visualization (key: 1). */
    void ToggleDebugContacts();

    /** @brief Toggle collision wireframes on articulations (key: 3). */
    void ToggleArticulationCollisions();

    /** @brief Toggle collision wireframes on Quick Convert objects (key: 5). */
    void ToggleQuickConvertCollisions();

    /** @brief Toggle joint axes on all articulations (key: 4). */
    void ToggleDebugJoints();

    /** @brief Toggle visual mesh visibility on all articulations (key: 2). */
    void ToggleVisuals();

    /** @brief Cycle per-body shader overlay: Off -> Island -> Segmentation -> Off (key: 6). */
    void CycleDebugShaderMode();

    /** @brief Toggle tendon/muscle spline rendering (key: 7). */
    void ToggleTendons();

    /** @brief Parent material for body/tendon overlay MIDs, probed from engine content at BeginPlay. */
    UPROPERTY(Transient)
    UMaterialInterface* OverlayParentMaterial = nullptr;

    /** @brief Vector parameter name on OverlayParentMaterial that accepts the overlay colour. */
    FName OverlayColorParam = NAME_None;

    /** @brief Loads /Engine/BasicShapes/BasicShapeMaterial and records the first vector param name. */
    void InitializeOverlayMaterial();

    /** @brief Apply per-body overlay MIDs based on DebugShaderMode, or restore originals if Off. */
    void UpdateBodyOverlays();

    /** @brief Restore original materials recorded at overlay apply time, clear caches. */
    void ClearBodyOverlays();

    /** @brief Rebuild / reuse the spline-mesh tendon segment pool based on the latest snapshot. */
    void UpdateTendonTubes();

    /** @brief Hide the tendon tube pool without destroying it. */
    void HideTendonTubes();

    /** @brief Destroy the tube pool entirely. Used on EndPlay. */
    void ClearTendonTubes();

    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

    // --- Per-camera segmentation pool ---
    //
    // Seg-mode UMjCameras share sibling mesh components rather than each maintaining
    // their own. Pools are lazy: built on first Acquire, destroyed on last Release.
    // Only two modes are poolable — InstanceSegmentation and SemanticSegmentation.

    /**
     * @brief Subscribe a camera to the sibling-mesh pool for Mode.
     *        Builds the pool on first subscription, returns its sibling primitives.
     * @param Mode         Must be Semantic- or InstanceSegmentation; other values are a no-op.
     * @param Camera       Subscriber; tracked so Release can refcount correctly.
     * @param OutSiblings  Populated with the shared sibling primitives for the camera's ShowOnly list.
     */
    void AcquireSegPool(EMjCameraMode Mode, UMjCamera* Camera,
                        TArray<UPrimitiveComponent*>& OutSiblings);

    /**
     * @brief Unsubscribe a camera. When the last subscriber leaves, the pool is destroyed.
     */
    void ReleaseSegPool(EMjCameraMode Mode, UMjCamera* Camera);

    /**
     * @brief Snapshot of a currently-live sibling pool. Used for tests and for
     *        non-seg URLab cameras that need to hide siblings via HiddenComponents.
     */
    void GetSegPoolSiblings(EMjCameraMode Mode, TArray<UPrimitiveComponent*>& OutSiblings) const;

private:
    bool bVisualsHidden = false;

    /** Sibling-mesh pool for InstanceSegmentation-mode cameras. Empty when no subscribers. */
    UPROPERTY(Transient)
    TArray<UObject*> InstanceSegSiblings;

    /** Sibling-mesh pool for SemanticSegmentation-mode cameras. */
    UPROPERTY(Transient)
    TArray<UObject*> SemanticSegSiblings;

    TSet<TWeakObjectPtr<UMjCamera>> InstanceSegSubscribers;
    TSet<TWeakObjectPtr<UMjCamera>> SemanticSegSubscribers;

    /** Returns a mutable ref to the pool matching Mode, or null for non-seg modes. */
    TArray<UObject*>* GetSegPoolArray(EMjCameraMode Mode);
    TSet<TWeakObjectPtr<UMjCamera>>* GetSegSubscribers(EMjCameraMode Mode);

    /** Build the seg pool for Mode by walking the world's articulations + quick-convert. */
    void BuildSegPool(EMjCameraMode Mode);

    /** Destroy all siblings in Mode's pool and clear it. */
    void DestroySegPool(EMjCameraMode Mode);

    /** Spawn one sibling mesh for a given original. Returns the new component (already registered). */
    UStaticMeshComponent* SpawnSegSibling(UStaticMeshComponent* Original,
                                          int32 BodyId, uint32 GroupHash,
                                          EMjCameraMode Mode);

    /** Original slot-0 material on meshes we've overridden, so we can restore. Keyed by mesh component. */
    TMap<TWeakObjectPtr<class UStaticMeshComponent>, class UMaterialInterface*> OriginalMaterials;

    /** Original slot-1..N materials for multi-material meshes. Parallel to OriginalMaterials. */
    TMap<TWeakObjectPtr<class UStaticMeshComponent>, TMap<int32, class UMaterialInterface*>> OriginalSlotMaterials;

    /** Dynamic material instances we created per mesh, reused across ticks. */
    TMap<TWeakObjectPtr<class UStaticMeshComponent>, class UMaterialInstanceDynamic*> ActiveMIDs;

    /** Pool of spline-mesh components used by the tube tendon render style. Grown on demand. */
    UPROPERTY(Transient)
    TArray<class USplineMeshComponent*> TendonSegmentPool;

    /** One MID per pool entry so each tube can colour independently. Grown in lockstep with TendonSegmentPool. */
    UPROPERTY(Transient)
    TArray<class UMaterialInstanceDynamic*> TendonSegmentMIDs;

    /** Base cylinder mesh used for every tendon segment. Loaded from /Engine/BasicShapes at BeginPlay. */
    UPROPERTY(Transient)
    class UStaticMesh* TendonTubeMesh = nullptr;
};
