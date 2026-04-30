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
#include "MuJoCo/Core/Spec/MjSpecWrapper.h"
#include "MjQuickConvertComponent.generated.h"

// Forward Declaration
class UMjBody;

/**
 * @class UMjQuickConvertComponent
 * @brief Component that enables physically simulated MuJoCo behavior for an Actor.
 * 
 * This component automatically parses the StaticMesh of the owning Actor and creates
 * a corresponding body and collision geoms in the MuJoCo mjSpec.
 * It also synchronizes the Actor's transform with the MuJoCo simulation at runtime.
 */
UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class URLAB_API UMjQuickConvertComponent : public UActorComponent
{
	GENERATED_BODY()
private:

    UMjBody* m_CreatedBody = nullptr;
    mjSpec* spec_;
    mjVFS* vfs_;

    FMujocoSpecWrapper* wrapper = nullptr;

    FString m_BodyName;

    mjModel* m_model = nullptr;
    mjData* m_data = nullptr;

    bool m_isStatic = false;

    AActor* m_actor = nullptr;

public:
    /** @brief Sets default values for this component's properties. */
	UMjQuickConvertComponent();

    /** @brief Forces complex mesh (convex decomposition) generation for collision. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MuJoCo|Mesh",
        meta=(ToolTip="When true, runs CoACD convex decomposition on the mesh to produce accurate collision geometry. Slower but required for non-convex shapes."))
    bool ComplexMeshRequired = false;

    /** @brief CoACD decomposition threshold. Lower = more accurate but more convex hulls. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MuJoCo|Mesh",
        meta=(EditCondition="ComplexMeshRequired", EditConditionHides, ClampMin="0.01", ClampMax="1.0",
        ToolTip="CoACD concavity threshold. Lower values produce more accurate (but more) convex hulls. Default 0.05."))
    float CoACDThreshold = 0.05f;

    /** @brief Draws debug lines for collision geoms in the viewport. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MuJoCo|Debug",
        meta=(ToolTip="Draw debug wireframes for all MuJoCo collision geoms created by this component."))
    bool m_debug_meshes = false;

    /** @brief If true, the body is static (no free joint added). */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MuJoCo|Physics",
        meta=(ToolTip="Static bodies have no free joint and cannot move under physics forces. Use for fixed obstacles."))
    bool Static = false;

    /** @brief If true, this body's transform is driven by the Unreal Actor/Component, enabling one-way coupling (Unreal → MuJoCo). */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MuJoCo|Physics",
        meta=(ToolTip="When true, writes the actor's world transform to MuJoCo as a mocap body every tick. The physics simulation does not feed back into Unreal."))
    bool bDrivenByUnreal = false;

    double* MocapPos = nullptr;
    double* MocapQuat = nullptr;

    /** @brief Friction parameters (sliding, torsional, rolling). Applied to all geoms created by this component. */
    UPROPERTY(EditAnywhere, Category="MuJoCo|Physics")
    FVector3d friction = {1.0, 1, 1};

    /** @brief Constraint solver reference parameters (timeconst, dampratio). Applied to all geoms. */
    UPROPERTY(EditAnywhere, Category="MuJoCo|Physics")
    FVector3d solref = {0.02, 1.0, 0.0};

    /** @brief Constraint solver impedance parameters (dmin, dmax, width). Applied to all geoms. */
    UPROPERTY(EditAnywhere, Category="MuJoCo|Physics")
    FVector3d solimp = {0.9, 0.95, 0.001};

    /** @brief Map of Geom name to MuJoCo ID. Populated after Compile(). */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="MuJoCo|Debug",
        meta=(ToolTip="Read-only map from geom name to MuJoCo geom ID. Populated after the model is compiled. Useful for debugging."))
    TMap<FString, int> m_geomName2ID;

    /** @brief Returns proper body name. */
    FString GetBodyName();

    /** @brief MuJoCo body id for this quick-converted actor, or -1 if not yet bound. */
    int32 GetMjBodyId() const;
    
    /**
     * @brief Initializes the component, parsing meshes and adding to spec.
     * @param spec Pointer to the shared mjSpec.
     * @param vfs Pointer to the shared Virtual File System.
     */
    void Setup(mjSpec* spec, mjVFS* vfs);
    
    /**
     * @brief Finalizes setup after compilation.
     * @param model Pointer to the compiled mjModel.
     * @param data Pointer to the active mjData.
     */
    void PostSetup(mjModel* model, mjData* data);

protected:
    /** @brief Called when the game starts. */
	virtual void BeginPlay() override;
    
    /** @brief Called when the game ends. */
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

    void DrawDebugCollision();

    void UpdateUETransform();

public:	
    /** @brief Called every frame. */
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
};
