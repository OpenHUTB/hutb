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
#include <mujoco/mujoco.h>
#include "MuJoCo/Utils/MjBind.h"
#include "MuJoCo/Components/Geometry/MjGeom.h"
#include <string>
#include <vector>

// Forward Declarations
class UMjDefault;

/**
 * @class FMujocoSpecWrapper
 * @brief Helper class to simplify building a MuJoCo mjSpec structure from Unreal Engine components.
 * 
 * Handles coordinate system conversion, unique naming, and resource management (VFS for meshes).
 * Acts as a bridge between high-level UE components and low-level MuJoCo C API.
 */
class FMujocoSpecWrapper {
public:
    /**
     * @brief Constructor.
     * @param InSpec Pointer to the MjSpec to populate.
     * @param InVFS Pointer to the Virtual File System for loading assets.
     */
    FMujocoSpecWrapper(mjSpec* InSpec, mjVFS* InVFS);

    // --- Body Creation ---
    /**
     * @brief Creates a body in the spec.
     * @param Name Desired name for the body.
     * @param ParentName Name of the parent body.
     * @param WorldTransform World transform of the body in UE (converted to MuJoCo coords relative to parent automatically).
     * @return Pointer to the created mjsBody.
     */
    mjsBody* CreateBody(const FString& Name, const FString& ParentName, const FTransform& WorldTransform);

    /**
     * @brief Creates a body in the spec attached to a specific parent body.
     * @param Name Desired name for the body.
     * @param ParentBody Pointer to the parent mjsBody.
     * @param WorldTransform World transform of the body in UE.
     * @return Pointer to the created mjsBody.
     */
    mjsBody* CreateBody(const FString& Name, mjsBody* ParentBody, const FTransform& WorldTransform);

    /**
     * @brief Creates a frame in the spec attached to a specific parent body.
     * @param Name Desired name for the frame.
     * @param ParentBody Pointer to the parent mjsBody.
     * @param WorldTransform World transform of the frame in UE (converted to MuJoCo coords relative to parent automatically).
     * @return Pointer to the created mjsFrame.
     */
    mjsFrame* CreateFrame(const FString& Name, mjsBody* ParentBody, const FTransform& WorldTransform);

    // --- Mesh Management ---
    /**
     * @brief Registers a mesh asset in the spec and VFS.
     * @param MeshName Unique name for the mesh.
     * @param FilePath Absolute path to the .obj file.
     * @param Scale Scaling factor for the mesh.
     * @return The registered mesh name.
     */
    FString AddMeshAsset(const FString& MeshName, const FString& FilePath, const FVector& Scale);

    // --- Geom Creation ---
    /**
     * @brief Adds a primitive geom to a body (Direct creation).
     * @param Body Target body.
     * @param Type Geometric primitive type.
     * @param Size Dimensions.
     * @param RGBA Color.
     * @param Density Density for mass calculation.
     * @return Pointer to the created mjsGeom.
     */
    mjsGeom* AddPrimitiveGeom(mjsBody* Body, mjtGeom Type, const FVector& Size, const FVector4& RGBA = FVector4(1,1,1,1), double Density = 1000.0);
    
    /**
     * @brief Adds a mesh geom to a body (Direct creation).
     * @param Body Target body.
     * @param RelTrans Relative transform.
     * @param MeshAssetName Name of the registered mesh asset.
     * @param GeomNameSuffix Suffix for the geom name.
     * @param RGBA Color.
     * @param Density Density.
     * @return Pointer to the created mjsGeom.
     */
    mjsGeom* AddMeshGeom(mjsBody* Body, FTransform RelTrans, const FString& MeshAssetName, const FString& GeomNameSuffix, const FVector4& RGBA = FVector4(1,1,1,1), double Density = 1000.0);
    
    // --- Joint Creation ---
    /**
     * @brief Adds a legacy free joint.
     * @param Body Target body.
     * @param JointName Optional name.
     */
    void AddFreeJoint(mjsBody* Body, const FString& JointName = "root_free");
    
    // --- Default Creation ---
    /**
     * @brief Adds a default class from a component.
     * @param DefaultComp Source MjDefault component.
     */
     void AddDefault(class UMjDefault* DefaultComp);

    /**
     * @brief Reconstructs implementation views for binding after compilation.
     * @param m Compiled mjModel.
     * @param d Active mjData.
     * @param Prefix Optional prefix applied to body names (from mjs_attach).
     * @return Array of BodyView structures.
     */
    TArray<BodyView> ReconstructViews(const mjModel* m, mjData* d, const FString& Prefix = TEXT(""));

    /**
     * @brief Prepares a static mesh for MuJoCo (exports OBJ, logs it).
     * @param SMC Static Mesh Component to export.
     * @param bComplexMeshRequired Whether to use complex collision mesh.
     * @return An array of unique names of the mesh assets in MuJoCo.
     */
    TArray<FString> PrepareMeshForMuJoCo(UStaticMeshComponent* SMC, bool bComplexMeshRequired, float CoACDThreshold = 0.05f);

    /**
     * @brief Generates a unique name for a MuJoCo object to avoid collisions.
     * @param BaseName Desired base name.
     * @param Type Object type (mjOBJ_BODY, mjOBJ_GEOM, etc.).
     * @param ContextActor Optional actor context to prefix the name.
     * @return Unique name string.
     */
    FString GetUniqueName(const FString& BaseName, mjtObj Type, const AActor* ContextActor = nullptr);

    mjSpec* Spec;
    mjVFS* VFS;

    /** @brief Sub-directory name for mesh cache (set to articulation name). */
    FString MeshCacheSubDir;

    /** @brief storage of created body names for view reconstruction. */
    TArray<FString> CreatedBodyNames;
};