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

// MujocoGenerationAction.h
#pragma once

#include "CoreMinimal.h"
#include "AssetActionUtility.h"
#include "MuJoCo/Core/Spec/MjSpecWrapper.h"
#include "MuJoCo/Core/Spec/SpecIterator.h"
#include "MuJoCo/Utils/MjOrientationUtils.h"
#include "Materials/MaterialInstanceConstant.h"

// Forward declarations
namespace EFBXNormalGenerationMethod { enum Type : int; }

#include "MujocoGenerationAction.generated.h"

/**
 * @struct FMuJoCoMaterialData
 * @brief Stores parsed MuJoCo material properties from XML.
 */
USTRUCT()
struct FMuJoCoMaterialData
{
    GENERATED_BODY()

    UPROPERTY()
    FLinearColor Rgba = FLinearColor::White;

    UPROPERTY()
    FString BaseColorTextureName;

    UPROPERTY()
    FString NormalTextureName;

    UPROPERTY()
    FString ORMTextureName; // OcclusionRoughnessMetallic combined texture

    UPROPERTY()
    FString RoughnessTextureName;

    UPROPERTY()
    FString MetallicTextureName;
};

/**
 * @struct FArticulationHierarchy
 * @brief Stores the organizational root nodes for a MuJoCo articulation.
 */
struct FArticulationHierarchy
{
    class USCS_Node* DefinitionsRoot = nullptr;
    class USCS_Node* DefaultsRoot = nullptr;
    class USCS_Node* ActuatorsRoot = nullptr;
    class USCS_Node* SensorsRoot = nullptr;
    class USCS_Node* TendonsRoot = nullptr;
    class USCS_Node* ContactsRoot = nullptr;
    class USCS_Node* EqualitiesRoot = nullptr;
    class USCS_Node* KeyframesRoot = nullptr;
};

// Forward declaration if possible, but mj::Body suggests C++ API usage. 
// If <mujoco/mjmodel.h> is the C header, mj::Body shouldn't exist there.
// mj::Body usually comes from C++ wrapper or custom iterator.
// Assuming "spec_iterator.h" provides mj::Body.

/**
 * @class UMujocoGenerationAction
 * @brief Asset Action Utility to generate MuJoCo components in Blueprints.
 * 
 * Accessible via right-click context menu on Blueprints (specifically AMjArticulation descendants).
 * Parses MuJoCo specs and populates the Blueprint's SCS (Simple Construction Script) with 
 * corresponding URLab components (MjBody, MjJoint, MjGeom, etc.).
 */
UCLASS()
class URLABEDITOR_API UMujocoGenerationAction : public UAssetActionUtility
{
	GENERATED_BODY()

public:
	UMujocoGenerationAction();

    /**
     * @brief Generates MuJoCo components for the selected Blueprint.
     * Accessible via the "Scripted Asset Actions" menu in the editor.
     */
	UFUNCTION(CallInEditor, Category = "MuJoCo")
	void GenerateMuJoCoComponents();

    /**
     * @brief Generates components for a single Blueprint using the specified XML file.
     * Use this for programmatic import (e.g. from Factory).
     */
    void GenerateForBlueprint(UBlueprint* BP, const FString& XMLPath);

    /**
     * @brief Pre-populates an empty AMjArticulation Blueprint with organizational hierarchy and a main body.
     */
    void SetupEmptyArticulation(UBlueprint* BP);

    /**
     * @brief Creates the standard MuJoCo organizational hierarchy (Definitions, Defaults, etc.) in the Blueprint.
     * @return The organizational hierarchy structure.
     */
    FArticulationHierarchy CreateOrganizationalHierarchy(UBlueprint* BP);
    
    /**
     * @brief Generates components using Direct XML Traversal (bypassing mjSpec structure iteration).
     */
    void GenerateForBlueprintXml(UBlueprint* BP, const FString& XMLPath);

    /**
     * @brief Generates components using Direct XML Traversal from an already parsed XML file.
     */
    void GenerateForBlueprintXml(UBlueprint* BP, const FString& XMLPath, const class FXmlFile* InXmlFile);

    /**
     * @brief Recursively parses XML to find all <asset> -> <mesh>, <texture>, and <material> tags.
     */
    void ParseAssetsRecursive(const class FXmlNode* Node, const FString& XMLDir, 
                             TMap<FString, FString>& OutMeshAssets, 
                             TMap<FString, FVector>& OutMeshScales,
                             TMap<FString, FString>& OutTextureAssets,
                             TMap<FString, FMuJoCoMaterialData>& OutMaterialData,
                             const FString& MeshDir = TEXT(""),
                             const FString& TextureDir = TEXT(""),
                             const FString& AssetDir = TEXT(""));

    /**
     * @brief Attempts to import a mesh with specified normal generation method.
     */
    UStaticMesh* AttemptMeshImport(const FString& SourcePath, const FString& DestinationPath, EFBXNormalGenerationMethod::Type NormalMethod);

    /**
     * @brief Validates imported mesh has valid render data and geometry.
     */
    bool ValidateMesh(UStaticMesh* Mesh, const FString& MeshName);

    /**
     * @brief Recursively parses XML to find all <default> tags and create UMjDefault components.
     */
    void ParseDefaultsRecursive(const class FXmlNode* Node, UBlueprint* BP, USCS_Node* RootNode, const FString& XMLDir, const struct FMjCompilerSettings& CompilerSettings, const FString& ParentClassName = TEXT(""), bool bIsDefaultContext = true);
    
    /** @brief Parses XML <contact> section to find <pair> and <exclude> elements and create corresponding components. */
    void ParseContactSection(const class FXmlNode* Node, UBlueprint* BP, USCS_Node* RootNode, const FString& XMLDir);

    /** @brief Parses XML <equality> section to create corresponding components. */
    void ParseEqualitySection(const class FXmlNode* Node, UBlueprint* BP, USCS_Node* RootNode, const FString& XMLDir);

    /** @brief Parses XML <keyframe> section to create corresponding components. */
    void ParseKeyframeSection(const class FXmlNode* Node, UBlueprint* BP, USCS_Node* RootNode, const FString& XMLDir);
    
    /**
     * @brief Recursively imports XML nodes into SCS.
     */
    void ImportNodeRecursive(const class FXmlNode* Node, USCS_Node* ParentNode, UBlueprint* BP, 
                            const FString& XMLDir, const FString& AssetImportPath, 
                            const TMap<FString, FString>& MeshAssets, 
                            const TMap<FString, FVector>& MeshScales,
                            const TMap<FString, FString>& TextureAssets,
                            const TMap<FString, FMuJoCoMaterialData>& MaterialData,
                            const TMap<FString, UTexture2D*>& ImportedTextures,
                            const FMjCompilerSettings& CompilerSettings,
                            bool bIsDefaultContext = false,
                            USCS_Node* ReuseNode = nullptr);
    
    /**
     * @brief scans the hierarchy after import and fixes issues (like Free Joints on child bodies).
     */
private:
    TMap<FString, USCS_Node*> CreatedDefaultNodes;
    TMap<FString, FVector> DefaultMeshScales;

    /** Pre-scans the XML for <default><mesh scale="..."/> to populate DefaultMeshScales. */
    void CollectDefaultMeshScales(const class FXmlNode* Node, const FString& CurrentClass = TEXT("main"));
	UStaticMesh* ImportSingleMesh(const FString& SourcePath, const FString& DestinationPath);

    UTexture2D* ImportSingleTexture(const FString& SourcePath, const FString& DestinationPath);

    UMaterialInstanceConstant* CreateMaterialInstance(
        const FString& MeshName,
        const FMuJoCoMaterialData& MaterialData,
        const TMap<FString, UTexture2D*>& TextureAssets,
        const FString& DestinationPath);
};