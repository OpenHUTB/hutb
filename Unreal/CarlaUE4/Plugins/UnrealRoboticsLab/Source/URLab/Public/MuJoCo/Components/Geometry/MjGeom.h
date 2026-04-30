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
#include "MuJoCo/Components/Defaults/MjDefault.h"
#include "mujoco/mujoco.h"
#include "MuJoCo/Utils/MjBind.h"
#include "MuJoCo/Components/MjComponent.h"
#include "MjGeom.generated.h"




/**
 * @enum EMjGeomType
 * @brief Defines the geometric primitive type for the MuJoCo geom.
 */
UENUM(BlueprintType)
enum class EMjGeomType : uint8
{
	Plane		UMETA(DisplayName = "Plane"),
	Hfield		UMETA(DisplayName = "Hfield"),
	Sphere		UMETA(DisplayName = "Sphere"),
	Capsule		UMETA(DisplayName = "Capsule"),
	Ellipsoid	UMETA(DisplayName = "Ellipsoid"),
	Cylinder	UMETA(DisplayName = "Cylinder"),
	Box			UMETA(DisplayName = "Box"),
	Mesh		UMETA(DisplayName = "Mesh"),
	SDF			UMETA(DisplayName = "SDF")
};

/**
 * @class UMjGeom
 * @brief Component representing a collision geometry (geom) in MuJoCo.
 * 
 * Geoms define the shape and collision properties of a body.
 * This class corresponds to the `geom` element in MuJoCo XML.
 */
// ...

UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class URLAB_API UMjGeom : public UMjComponent
{
	GENERATED_BODY()

public:
     UMjGeom();

    virtual void ExportTo(mjsGeom* Geom, mjsDefault* Default = nullptr);

    virtual void Bind(mjModel* Model, mjData* Data, const FString& Prefix = TEXT("")) override;
    
    virtual void ImportFromXml(const class FXmlNode* Node);
    virtual void ImportFromXml(const class FXmlNode* Node, const struct FMjCompilerSettings& CompilerSettings);

    /** 
     * @brief Synchronizes the Unreal Component transform (Scale/Location/Rotation) 
     * from the live MuJoCo view. Called after successful Bind().
     */
    virtual void SyncUnrealTransformFromMj();
    
    /** @brief Sets visibility for this geom and its child visual components. */
    virtual void SetGeomVisibility(bool bNewVisibility);

#if WITH_EDITOR
    virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

    /** @brief Returns the built-in visualizer mesh if it exists. */
    virtual class UStaticMeshComponent* GetVisualizerMesh() const { return nullptr; }

    /** @brief Apply a material to the visual mesh. Override in subclasses with direct member access. */
    virtual void ApplyOverrideMaterial(class UMaterialInterface* Material);

    /** @brief The runtime view of the MuJoCo geom. Valid only after Bind() is called. */
    GeomView m_GeomView;

    /** @brief Semantic accessor for raw MuJoCo data and helper methods. */
    GeomView& GetMj() { return m_GeomView; }
    const GeomView& GetMj() const { return m_GeomView; }

    // --- Runtime Accessors ---

    /** 
     * @brief Updates the Unreal SceneComponent transform to match the MuJoCo simulation state.
     * Call this in Tick if you want the visual to follow the physics.
     */
    UFUNCTION(BlueprintCallable, Category = "MuJoCo|Runtime")
    void UpdateGlobalTransform();

    /** @brief Gets the current world location from MuJoCo. */
    UFUNCTION(BlueprintCallable, Category = "MuJoCo|Runtime")
    FVector GetWorldLocation() const;

    /** 
     * @brief Sets the sliding friction at runtime.
     * @param NewFriction The new friction value (only the first dimension is usually used for sliding).
     */
    UFUNCTION(BlueprintCallable, Category = "MuJoCo|Runtime")
    void SetFriction(float NewFriction);

public:
	// --- Geometric Properties ---
    /** @brief The geometric shape type of the geom. */
    UPROPERTY(EditAnywhere, Category = "MuJoCo|Geom", meta=(InlineEditConditionToggle))
    bool bOverride_Type = false;
	
	UPROPERTY(EditAnywhere, Category = "MuJoCo|Geom", meta=(InlineEditConditionToggle))
	bool bOverride_Pos = false;
	
	UPROPERTY(EditAnywhere, Category = "MuJoCo|Geom", meta=(InlineEditConditionToggle))
	bool bOverride_Quat = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MuJoCo|Geom", meta=(EditCondition="bOverride_Type"))
	EMjGeomType Type = EMjGeomType::Sphere; 

	/** @brief Dimensions for primitives (Sphere: radius; Capsule: radius, halflength; Box: x,y,z half-sizes). */
    UPROPERTY(EditAnywhere, Category = "MuJoCo|Geom", meta=(InlineEditConditionToggle))
    bool bOverride_Size = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MuJoCo|Geom", meta=(EditCondition="bOverride_Size"))
	FVector Size = FVector(0.0f); 

    /**
     * @brief True when this component was created via ImportFromXml (i.e. loaded from a .xml file).
     * False for components that are manually authored by the user in the editor.
     * Used by ShouldOverrideSize() to distinguish "inherit-from-default" intent
     * (imported, bOverride_Size=false) from "use my UE scale" intent (user-authored).
     */
    UPROPERTY()
    bool bWasImported = false;

    /**
     * @brief Returns true if the geom's size should be written to MuJoCo on export
     * and the UE scale should NOT be overwritten from the MuJoCo default on Bind.
     *
     * Semantics:
     *  - Imported geom, explicit size in XML  (bWasImported=true,  bOverride_Size=true)  -> true
     *  - Imported geom, no size in XML        (bWasImported=true,  bOverride_Size=false) -> false (inherit default)
     *  - User-authored geom                   (bWasImported=false, bOverride_Size=any)   -> true (use UE scale)
     */
    bool ShouldOverrideSize() const { return bOverride_Size || !bWasImported; }

    /** @brief Name of the mesh asset if Type is Mesh. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "MuJoCo|Geom")
	FString MeshName;

    /** @brief Legacy: use complex collision mesh. Hidden — use "Decompose Mesh" button instead.
     *  Still used internally by QuickConvert and as import/compile fallback. */
    UPROPERTY(BlueprintReadWrite, Category = "MuJoCo|Geom")
    bool bComplexMeshRequired = false;

    /** @brief CoACD decomposition threshold. Lower = more accurate but more hulls. Range [0.01, 1.0]. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MuJoCo|Geom",
        meta=(EditCondition="Type == EMjGeomType::Mesh", EditConditionHides, ClampMin="0.01", ClampMax="1.0",
        ToolTip="CoACD concavity threshold. Lower values produce more accurate (but more) convex hulls. Default 0.05."))
    float CoACDThreshold = 0.05f;

    /** @brief True if this geom was created by CoACD decomposition of another geom. */
    UPROPERTY()
    bool bIsDecomposedHull = false;

    /** @brief True if this geom's mesh was decomposed into hull sub-geoms.
     *  When set, this geom is skipped during RegisterToSpec. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MuJoCo|Geom")
    bool bDisabledByDecomposition = false;

    /** @brief Runs CoACD decomposition on this geom's mesh and creates persistent hull sub-geom components. */
    UFUNCTION(BlueprintCallable, Category = "MuJoCo|Geom")
    void DecomposeMesh();

    /** @brief Removes all hull sub-geoms created by decomposition and re-enables this geom. */
    UFUNCTION(BlueprintCallable, Category = "MuJoCo|Geom")
    void RemoveDecomposition();

    /** @brief Override toggle for FromTo. */
    UPROPERTY(EditAnywhere, Category = "MuJoCo|Geom", meta=(InlineEditConditionToggle))
    bool bOverride_FromTo = false;

    /** 
     * @brief Start point for fromto definition. 
     * Used for Capsule/Cylinder/Box orientation and length.
     * Overrides Pos, Quat, and Size[1].
     */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MuJoCo|Geom", meta=(EditCondition="bOverride_FromTo"))
	FVector FromToStart = FVector(0.0f); 

    /** @brief End point for fromto definition. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MuJoCo|Geom", meta=(EditCondition="bOverride_FromTo"))
	FVector FromToEnd = FVector(0.0f, 0.0f, 1.0f);

    /** @brief True when fromto was resolved at import — only the half-length slot is valid, radius comes from defaults. */
    UPROPERTY()
    bool bFromToResolvedHalfLength = false;

    /** @brief Optional MuJoCo class name to inherit defaults from (string fallback). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MuJoCo|Geom", meta=(GetOptions="GetDefaultClassOptions"))
    FString MjClassName;
    virtual FString GetMjClassName() const override { return MjClassName; }

#if WITH_EDITOR
    UFUNCTION()
    TArray<FString> GetDefaultClassOptions() const;
#endif

    /** @brief Optional MuJoCo material name for visual properties. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "MuJoCo|Geom")
    FString MaterialName;

    /** @brief Resolves MaterialName through the default class chain. Returns empty if none found. */
    FString GetResolvedMaterialName() const;

	// --- Physics Properties (with override toggles) ---
    
    /** @brief Override toggle for Friction. */
    UPROPERTY(EditAnywhere, Category = "MuJoCo|Geom|Physics", meta=(InlineEditConditionToggle))
    bool bOverride_Friction = false;

    /** @brief Friction coefficients (slide, spin, roll). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MuJoCo|Geom|Physics", meta=(EditCondition="bOverride_Friction"))
	TArray<float> Friction = {1.0f, 0.005f, 0.0001f}; 

    /** @brief Override toggle for SolRef. */
    UPROPERTY(EditAnywhere, Category = "MuJoCo|Geom|Physics", meta=(InlineEditConditionToggle))
    bool bOverride_SolRef = false;

    /** @brief Solver reference parameters (timeconst, dampradio). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MuJoCo|Geom|Physics", meta=(EditCondition="bOverride_SolRef"))
	TArray<float> SolRef = {0.02f, 1.0f}; 

    /** @brief Override toggle for SolImp. */
    UPROPERTY(EditAnywhere, Category = "MuJoCo|Geom|Physics", meta=(InlineEditConditionToggle))
    bool bOverride_SolImp = false;

    /** @brief Solver impedance parameters (dmin, dmax, width). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MuJoCo|Geom|Physics", meta=(EditCondition="bOverride_SolImp"))
	TArray<float> SolImp = {0.9f, 0.95f, 0.001f}; 

    /** @brief Override toggle for Density. */
    UPROPERTY(EditAnywhere, Category = "MuJoCo|Geom|Physics", meta=(InlineEditConditionToggle))
    bool bOverride_Density = false;

    /** @brief Density of the material (used for mass calculation if mass not implicit). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MuJoCo|Geom|Physics", meta=(EditCondition="bOverride_Density"))
	float Density = 1000.0f;

    /** @brief Override toggle for Mass. */
    UPROPERTY(EditAnywhere, Category = "MuJoCo|Geom|Physics", meta=(InlineEditConditionToggle))
    bool bOverride_Mass = false;

    /** @brief Explicit mass of the geom (if provided, density is ignored). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MuJoCo|Geom|Physics", meta=(EditCondition="bOverride_Mass"))
	float Mass = 0.0f;

    /** @brief Override toggle for Margin. */
    UPROPERTY(EditAnywhere, Category = "MuJoCo|Geom|Physics", meta=(InlineEditConditionToggle))
    bool bOverride_Margin = false;

    /** @brief Collision margin (excludes contacts within this distance). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MuJoCo|Geom|Physics", meta=(EditCondition="bOverride_Margin"))
	float Margin = 0.0f;

    /** @brief Override toggle for Gap. */
    UPROPERTY(EditAnywhere, Category = "MuJoCo|Geom|Physics", meta=(InlineEditConditionToggle))
    bool bOverride_Gap = false;

    /** @brief Contact generation gap (includes contacts within this distance). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MuJoCo|Geom|Physics", meta=(EditCondition="bOverride_Gap"))
	float Gap = 0.0f;

    // --- Contact Dimension ---
    /** @brief Override toggle for Condim. */
    UPROPERTY(EditAnywhere, Category = "MuJoCo|Geom|Physics", meta=(InlineEditConditionToggle))
    bool bOverride_Condim = false;

    /** @brief Contact dimensionality (1, 3, 4, 6). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MuJoCo|Geom|Physics", meta=(EditCondition="bOverride_Condim"))
    int32 Condim = 3;

	// --- Collision Filtering (with override toggles) ---
    
    /** @brief Override toggle for Contype. */
    UPROPERTY(EditAnywhere, Category = "MuJoCo|Geom|Collision", meta=(InlineEditConditionToggle))
    bool bOverride_Contype = false;

    /** @brief Content type bitmask for collision filtering. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MuJoCo|Geom|Collision", meta=(EditCondition="bOverride_Contype"))
	int32 Contype = 1;

    /** @brief Override toggle for Conaffinity. */
    UPROPERTY(EditAnywhere, Category = "MuJoCo|Geom|Collision", meta=(InlineEditConditionToggle))
    bool bOverride_Conaffinity = false;

    /** @brief Content affinity bitmask for collision filtering (matches against other's contype). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MuJoCo|Geom|Collision", meta=(EditCondition="bOverride_Conaffinity"))
	int32 Conaffinity = 1;

    /** @brief Override toggle for Priority. */
    UPROPERTY(EditAnywhere, Category = "MuJoCo|Geom|Collision", meta=(InlineEditConditionToggle))
    bool bOverride_Priority = false;

    /** @brief Collision priority (higher priority interactions override lower). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MuJoCo|Geom|Collision", meta=(EditCondition="bOverride_Priority"))
	int32 Priority = 0;

    /** @brief Override toggle for Group. */
    UPROPERTY(EditAnywhere, Category = "MuJoCo|Geom|Collision", meta=(InlineEditConditionToggle))
    bool bOverride_Group = false;

    /** @brief Visual/Collision group ID. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MuJoCo|Geom|Collision", meta=(EditCondition="bOverride_Group"))
	int32 Group = 0;

    /** 
     * @brief Number of size parameters explicitly provided in XML. 
     * Used to support partial inheritance (e.g. overriding radius but keeping length).
     */
    UPROPERTY()
    int32 SizeParamsCount = 0;

	// --- Visuals (with override toggle) ---
    
    /** @brief Override toggle for Rgba. */
    UPROPERTY(EditAnywhere, Category = "MuJoCo|Geom|Visual", meta=(InlineEditConditionToggle))
    bool bOverride_Rgba = false;

    /** @brief RGBA Color of the geom. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MuJoCo|Geom|Visual", meta=(EditCondition="bOverride_Rgba"))
	FLinearColor Rgba = FLinearColor(0.5f, 0.5f, 0.5f, 1.0f);
	
    /** @brief Optional Unreal material override for primitive visualizer meshes (Box/Sphere/Cylinder). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MuJoCo|Geom|Visual",
        meta=(EditCondition="Type != EMjGeomType::Mesh", EditConditionHides))
    TObjectPtr<UMaterialInterface> OverrideMaterial;

    /** @brief Reference to a UMjDefault component for default class inheritance. Set via detail customization dropdown. */
	UPROPERTY(BlueprintReadWrite, Category = "MuJoCo|Geom")
	UMjDefault* DefaultClass;

    /**
     * @brief Registers this geom to the MuJoCo spec.
     * @param Wrapper The spec wrapper instance.
     * @param ParentBody The parent body to attach to.
     */
    virtual void RegisterToSpec(class FMujocoSpecWrapper& Wrapper, mjsBody* ParentBody) override;

};
