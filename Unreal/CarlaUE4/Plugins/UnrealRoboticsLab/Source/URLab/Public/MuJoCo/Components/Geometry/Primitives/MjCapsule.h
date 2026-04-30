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
#include "MuJoCo/Components/Geometry/MjGeom.h"
#include "MjCapsule.generated.h"

/**
 * @class UMjCapsule
 * @brief Capsule primitive geometry.
 *
 * Unreal doesn't ship `/Engine/BasicShapes/Capsule`, so the visual is
 * assembled from three engine basic shapes: a shaft cylinder and two sphere
 * end caps. Sub-mesh transforms are kept in sync with the component's own
 * relative scale so the gizmo still drives `radius` (X/Y) and `half-length`
 * (Z) coherently. Caps use a Z-counter-scale so they stay spherical when
 * the half-length differs from the radius.
 */
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class URLAB_API UMjCapsule : public UMjGeom
{
    GENERATED_BODY()

public:
    UMjCapsule();
    virtual void OnRegister() override;

    /** @brief Capsule radius. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MuJoCo|Primitive")
    float Radius = 0.0f;

    /** @brief Capsule half-length measured along the component's local Z
     *         axis. Total axial length from tip-to-tip is `2 * (Radius + HalfLength)`;
     *         the shaft length (between the cap centres) is `2 * HalfLength`. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MuJoCo|Primitive")
    float HalfLength = 0.0f;

    // UPROPERTY-decorated so Unreal's GC tracks these sub-component pointers.
    // Editing RelativeScale3D from the Details panel triggers a component
    // reconstruction; without UPROPERTY, the raw pointers dangle after the
    // old sub-components are collected, and UpdateCapTransforms crashes with
    // EXCEPTION_ACCESS_VIOLATION on the stale pointer.

    /** @brief Internal shaft cylinder. */
    UPROPERTY(Transient)
    class UStaticMeshComponent* VisualizerShaft = nullptr;

    /** @brief Internal top cap (positive Z). */
    UPROPERTY(Transient)
    class UStaticMeshComponent* VisualizerCapTop = nullptr;

    /** @brief Internal bottom cap (negative Z). */
    UPROPERTY(Transient)
    class UStaticMeshComponent* VisualizerCapBottom = nullptr;

    virtual void ImportFromXml(const class FXmlNode* Node) override;
    virtual void ImportFromXml(const class FXmlNode* Node, const struct FMjCompilerSettings& CompilerSettings) override;
    virtual void ExportTo(mjsGeom* geom, mjsDefault* def = nullptr) override;

    virtual void SyncUnrealTransformFromMj() override;
    virtual void SetGeomVisibility(bool bNewVisibility) override;

#if WITH_EDITOR
    virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

    /** Returns the shaft cylinder so the overlay/debug-viz paths that grep
     *  for a `UStaticMeshComponent` find *something*. The caps are covered
     *  by the polymorphic attached-children walk in debug-viz. */
    virtual class UStaticMeshComponent* GetVisualizerMesh() const override
    {
        const_cast<UMjCapsule*>(this)->EnsureVisualizerMesh();
        return VisualizerShaft;
    }

    void EnsureVisualizerMesh();

private:
    /** Position + counter-scale the caps so they sit at the shaft endpoints
     *  and stay spherical regardless of parent non-uniform scale. */
    void UpdateCapTransforms();
};
