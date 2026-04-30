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

// MujocoGenerationAction.cpp — Orchestration: entry points and top-level generation flow.
// Mesh/material import lives in MujocoMeshImporter.cpp.
// XML parsing lives in MujocoXmlParser.cpp.

#include "MujocoGenerationAction.h"
#include "URLabEditorLogging.h"
#include "MuJoCo/Components/Bodies/MjWorldBody.h"
#include "MuJoCo/Core/MjArticulation.h"
#include "MuJoCo/Utils/MjUtils.h"
#include "MuJoCo/Utils/MjOrientationUtils.h"

#include "Engine/Blueprint.h"
#include "Engine/SCS_Node.h"
#include "Engine/SimpleConstructionScript.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "PackageTools.h"
#include "EditorUtilityLibrary.h"
#include "XmlFile.h"
#include "XmlNode.h"
#include "mujoco/mujoco.h"

// ProcessDefault removed — default class creation handled by UMjDefault + mjs_addDefault API.


UMujocoGenerationAction::UMujocoGenerationAction()
{
    SupportedClasses.Add(UBlueprint::StaticClass());
}


void UMujocoGenerationAction::GenerateMuJoCoComponents()
{
    // Clear defaults cache
    CreatedDefaultNodes.Empty();

    UE_LOG(LogURLabEditor, Log, TEXT("Generating MuJoCo model components"));
    TArray<UObject*> SelectedAssets = UEditorUtilityLibrary::GetSelectedAssets();

    for (UObject* Asset : SelectedAssets)
    {
        UBlueprint* BP = Cast<UBlueprint>(Asset);
        if (!BP || !BP->GeneratedClass->IsChildOf(AMjArticulation::StaticClass()))
        {
            continue;
        }

        AMjArticulation* CDO = Cast<AMjArticulation>(BP->GeneratedClass->GetDefaultObject());
        if (!CDO || CDO->MuJoCoXMLFile.FilePath.IsEmpty())
        {
            UE_LOG(LogURLabEditor, Error, TEXT("No XML File Path set in Blueprint Defaults for %s"), *BP->GetName());
            continue;
        }

        GenerateForBlueprint(BP, CDO->MuJoCoXMLFile.FilePath);
    }
}

void UMujocoGenerationAction::GenerateForBlueprint(UBlueprint* BP, const FString& XMLPath)
{
    if (!BP || XMLPath.IsEmpty()) return;

    // Optional: Basic MuJoCo XML Verification (can be removed if total purity desired, but safe to keep)
    mjModel* M = nullptr;
    char Error[1000];
    M = mj_loadXML(TCHAR_TO_UTF8(*XMLPath), nullptr, Error, 1000);
    if (!M) {
        UE_LOG(LogURLabEditor, Error, TEXT("MuJoCo Load/Validation Error: %s"), UTF8_TO_TCHAR(Error));
        // We can choose to return or proceed. If XML is invalid, pure parsing might also fail or yield garbage.
        return;
    }
    mj_deleteModel(M);

    // Pure XML Strategy
    GenerateForBlueprintXml(BP, XMLPath);

    FKismetEditorUtilities::CompileBlueprint(BP);
}

void UMujocoGenerationAction::GenerateForBlueprintXml(UBlueprint* BP, const FString& XMLPath)
{
    if (!BP || XMLPath.IsEmpty()) return;

    FXmlFile XmlFile(XMLPath);
    if (!XmlFile.IsValid())
    {
         UE_LOG(LogURLabEditor, Error, TEXT("Failed to load XML file: %s"), *XMLPath);
         return;
    }

    GenerateForBlueprintXml(BP, XMLPath, &XmlFile);
}

void UMujocoGenerationAction::GenerateForBlueprintXml(UBlueprint* BP, const FString& XMLPath, const FXmlFile* InXmlFile)
{
    if (!BP || !InXmlFile || !InXmlFile->IsValid()) return;

    const FXmlNode* Root = InXmlFile->GetRootNode();
    if (!Root) return;

    FString XMLDir = FPaths::GetPath(XMLPath);
    USCS_Node* SceneRoot = BP->SimpleConstructionScript->GetDefaultSceneRootNode();

    FString BaseContentPath = TEXT("/Game/MuJoCoImports/");
    FString BaseName = FPaths::GetBaseFilename(XMLPath);
    if (BaseName.IsEmpty()) BaseName = TEXT("MemModel");

    FString AssetImportPath = BaseContentPath + BaseName + TEXT("_Assets");
    AssetImportPath = UPackageTools::SanitizePackageName(AssetImportPath);

    // 0. Pre-scan: collect default mesh scales so asset parsing can inherit them
    CollectDefaultMeshScales(Root);

    // 1. Pass: Resolved Assets
    TMap<FString, FString> MeshAssets;
    TMap<FString, FVector> MeshScales;
    TMap<FString, FString> TextureAssets;
    TMap<FString, FMuJoCoMaterialData> MaterialData;
    ParseAssetsRecursive(Root, XMLDir, MeshAssets, MeshScales, TextureAssets, MaterialData);

    // 1b. Import Textures
    TMap<FString, UTexture2D*> ImportedTextures;
    for (const auto& TexPair : TextureAssets)
    {
        const FString& TexName = TexPair.Key;
        const FString& TexPath = TexPair.Value;

        UTexture2D* ImportedTex = ImportSingleTexture(TexPath, AssetImportPath);
        if (ImportedTex)
        {
            ImportedTextures.Add(TexName, ImportedTex);
            UE_LOG(LogURLabEditor, Log, TEXT("Imported texture: %s"), *TexName);
        }
        else
        {
            UE_LOG(LogURLabEditor, Warning, TEXT("Failed to import texture: %s from %s"), *TexName, *TexPath);
        }
    }

    // 2. Create organizational nodes dynamically
    FArticulationHierarchy Hierarchy = CreateOrganizationalHierarchy(BP);

    USCS_Node* DefinitionsNode = Hierarchy.DefinitionsRoot;
    USCS_Node* DefaultsNode    = Hierarchy.DefaultsRoot;
    USCS_Node* ActuatorsNode   = Hierarchy.ActuatorsRoot;
    USCS_Node* SensorsNode     = Hierarchy.SensorsRoot;
    USCS_Node* TendonsNode     = Hierarchy.TendonsRoot;
    USCS_Node* ContactsNode    = Hierarchy.ContactsRoot;
    USCS_Node* EqualitiesNode  = Hierarchy.EqualitiesRoot;
    USCS_Node* KeyframesNode   = Hierarchy.KeyframesRoot;

    // 2. Parse compiler settings first (angle, eulerseq) so they can be
    //    propagated into <default>-block imports — joint ranges inside a
    //    default class depend on the compiler-level `angle` setting.
    FMjCompilerSettings CompilerSettings = MjOrientationUtils::ParseCompilerSettings(Root);

    // 2a. Pass: Defaults
    ParseDefaultsRecursive(Root, BP, DefaultsNode, XMLDir, CompilerSettings, TEXT(""));

    // 2b. Pass: Contact pairs and excludes
    ParseContactSection(Root, BP, ContactsNode, XMLDir);

    // 2c. Pass: Equality constraints
    ParseEqualitySection(Root, BP, EqualitiesNode, XMLDir);

    // 2d. Pass: Keyframes
    ParseKeyframeSection(Root, BP, KeyframesNode, XMLDir);

    // 3. Pass: Structure Traversal
    USCS_Node* WorldBodyNode = nullptr;
    for (const FXmlNode* Child : Root->GetChildrenNodes())
    {
         FString Tag = Child->GetTag();
         if (Tag.Equals(TEXT("worldbody")))
         {
             // Create worldbody node only once — merge subsequent <worldbody> sections into it
             if (!WorldBodyNode)
             {
                 WorldBodyNode = BP->SimpleConstructionScript->CreateNode(UMjWorldBody::StaticClass(), TEXT("worldbody"));
                 WorldBodyNode->SetVariableName(TEXT("worldbody"));
                 BP->SimpleConstructionScript->AddNode(WorldBodyNode);
             }

             // Iterate worldbody children
             for (const FXmlNode* WBChild : Child->GetChildrenNodes())
             {
                 ImportNodeRecursive(WBChild, WorldBodyNode, BP, XMLDir, AssetImportPath, MeshAssets, MeshScales, TextureAssets, MaterialData, ImportedTextures, CompilerSettings, false);
             }
         }
         else if (Tag.Equals(TEXT("actuator")))
         {
              for (const FXmlNode* Item : Child->GetChildrenNodes())
              {
                   ImportNodeRecursive(Item, ActuatorsNode, BP, XMLDir, AssetImportPath, MeshAssets, MeshScales, TextureAssets, MaterialData, ImportedTextures, CompilerSettings, false);
              }
         }
         else if (Tag.Equals(TEXT("sensor")))
         {
              for (const FXmlNode* Item : Child->GetChildrenNodes())
              {
                   ImportNodeRecursive(Item, SensorsNode, BP, XMLDir, AssetImportPath, MeshAssets, MeshScales, TextureAssets, MaterialData, ImportedTextures, CompilerSettings, false);
              }
         }
         else if (Tag.Equals(TEXT("tendon")))
         {
              for (const FXmlNode* Item : Child->GetChildrenNodes())
              {
                   ImportNodeRecursive(Item, TendonsNode, BP, XMLDir, AssetImportPath, MeshAssets, MeshScales, TextureAssets, MaterialData, ImportedTextures, CompilerSettings, false);
              }
         }
         else if (Tag.Equals(TEXT("include")))
         {
              ImportNodeRecursive(Child, SceneRoot, BP, XMLDir, AssetImportPath, MeshAssets, MeshScales, TextureAssets, MaterialData, ImportedTextures, CompilerSettings, false);
         }
    }

    // 4. Parse <option> and store on articulation CDO so AAMjManager can apply them at runtime
    for (const FXmlNode* Child : Root->GetChildrenNodes())
    {
        if (Child->GetTag().Equals(TEXT("option")))
        {
            AMjArticulation* CDO = Cast<AMjArticulation>(BP->GeneratedClass->GetDefaultObject());
            if (CDO)
            {
                FMuJoCoOptions& Opts = CDO->SimOptions;

                auto ParseVec3 = [](const FString& Raw, FVector& Out)
                {
                    TArray<FString> P; Raw.ParseIntoArray(P, TEXT(" "), true);
                    if (P.Num() >= 3) { Out.X = FCString::Atof(*P[0]); Out.Y = FCString::Atof(*P[1]); Out.Z = FCString::Atof(*P[2]); }
                };

                FString V;
                V = Child->GetAttribute(TEXT("timestep"));      if (!V.IsEmpty()) Opts.Timestep = FCString::Atof(*V);
                V = Child->GetAttribute(TEXT("gravity"));       if (!V.IsEmpty()) ParseVec3(V, Opts.Gravity);
                V = Child->GetAttribute(TEXT("wind"));          if (!V.IsEmpty()) ParseVec3(V, Opts.Wind);
                V = Child->GetAttribute(TEXT("magnetic"));      if (!V.IsEmpty()) ParseVec3(V, Opts.Magnetic);
                V = Child->GetAttribute(TEXT("density"));       if (!V.IsEmpty()) Opts.Density    = FCString::Atof(*V);
                V = Child->GetAttribute(TEXT("viscosity"));     if (!V.IsEmpty()) Opts.Viscosity  = FCString::Atof(*V);
                V = Child->GetAttribute(TEXT("impratio"));      if (!V.IsEmpty()) Opts.Impratio   = FCString::Atof(*V);
                V = Child->GetAttribute(TEXT("tolerance"));     if (!V.IsEmpty()) Opts.Tolerance  = FCString::Atof(*V);
                V = Child->GetAttribute(TEXT("iterations"));    if (!V.IsEmpty()) Opts.Iterations = FCString::Atoi(*V);
                V = Child->GetAttribute(TEXT("ls_iterations")); if (!V.IsEmpty()) Opts.LsIterations = FCString::Atoi(*V);

                V = Child->GetAttribute(TEXT("noslip_iterations"));
                if (!V.IsEmpty()) { Opts.NoslipIterations = FCString::Atoi(*V); Opts.bOverride_NoslipIterations = true; }
                V = Child->GetAttribute(TEXT("noslip_tolerance"));
                if (!V.IsEmpty()) { Opts.NoslipTolerance  = FCString::Atof(*V); Opts.bOverride_NoslipTolerance  = true; }
                V = Child->GetAttribute(TEXT("ccd_iterations"));
                if (!V.IsEmpty()) { Opts.CCD_Iterations   = FCString::Atoi(*V); Opts.bOverride_CCD_Iterations   = true; }
                V = Child->GetAttribute(TEXT("ccd_tolerance"));
                if (!V.IsEmpty()) { Opts.CCD_Tolerance    = FCString::Atof(*V); Opts.bOverride_CCD_Tolerance    = true; }

                V = Child->GetAttribute(TEXT("integrator")).ToLower();
                if (!V.IsEmpty())
                {
                    Opts.bOverride_Integrator = true;
                    if      (V == TEXT("euler"))        Opts.Integrator = EMjIntegrator::Euler;
                    else if (V == TEXT("rk4"))          Opts.Integrator = EMjIntegrator::RK4;
                    else if (V == TEXT("implicit"))     Opts.Integrator = EMjIntegrator::Implicit;
                    else if (V == TEXT("implicitfast")) Opts.Integrator = EMjIntegrator::ImplicitFast;
                }

                V = Child->GetAttribute(TEXT("cone")).ToLower();
                if (!V.IsEmpty())
                {
                    Opts.bOverride_Cone = true;
                    if      (V == TEXT("pyramidal")) Opts.Cone = EMjCone::Pyramidal;
                    else if (V == TEXT("elliptic"))  Opts.Cone = EMjCone::Elliptic;
                }

                V = Child->GetAttribute(TEXT("solver")).ToLower();
                if (!V.IsEmpty())
                {
                    Opts.bOverride_Solver = true;
                    if      (V == TEXT("pgs"))    Opts.Solver = EMjSolver::PGS;
                    else if (V == TEXT("cg"))     Opts.Solver = EMjSolver::CG;
                    else if (V == TEXT("newton")) Opts.Solver = EMjSolver::Newton;
                }

                // <option><flag sleep="enable|disable"/></option>
                V = Child->GetAttribute(TEXT("sleep")).ToLower();
                if      (V == TEXT("enable"))  Opts.bEnableSleep = true;
                else if (V == TEXT("disable")) Opts.bEnableSleep = false;
                // sleep_tolerance is a direct option attribute (not inside <flag>)
                V = Child->GetAttribute(TEXT("sleep_tolerance"));
                if (!V.IsEmpty()) Opts.SleepTolerance = FCString::Atof(*V);

                // Also check inside child <flag> nodes (MuJoCo XML spec allows both)
                for (const FXmlNode* FlagNode : Child->GetChildrenNodes())
                {
                    if (FlagNode->GetTag().Equals(TEXT("flag"), ESearchCase::IgnoreCase))
                    {
                        FString SleepFlag = FlagNode->GetAttribute(TEXT("sleep")).ToLower();
                        if      (SleepFlag == TEXT("enable"))  Opts.bEnableSleep = true;
                        else if (SleepFlag == TEXT("disable")) Opts.bEnableSleep = false;
                    }
                }

                CDO->MarkPackageDirty();

                UE_LOG(LogURLabEditor, Log, TEXT("Parsed <option>: timestep=%.4f, gravity=%s"),
                    Opts.Timestep, *Opts.Gravity.ToString());
            }
            break;
        }
    }

}

FArticulationHierarchy UMujocoGenerationAction::CreateOrganizationalHierarchy(UBlueprint* BP)
{
    FArticulationHierarchy Hierarchy;
    if (!BP || !BP->SimpleConstructionScript) return Hierarchy;

    USCS_Node* SceneRoot = BP->SimpleConstructionScript->GetDefaultSceneRootNode();

    auto CreateOrgNode = [&](const FName& VarName, const FName& InternalName, USCS_Node* Parent) -> USCS_Node*
    {
        USCS_Node* NewNode = BP->SimpleConstructionScript->CreateNode(USceneComponent::StaticClass(), InternalName);
        NewNode->SetVariableName(VarName);
        // Use AddNode (not AddChildNode on DefaultSceneRoot) for top-level SCS nodes.
        // AMjArticulation has a C++ root (ArticulationRoot), so the SCS DefaultSceneRoot is
        // redundant and gets removed on Blueprint compile, orphaning anything attached to it.
        if (Parent && Parent != SceneRoot)
            Parent->AddChildNode(NewNode);
        else
            BP->SimpleConstructionScript->AddNode(NewNode);
        return NewNode;
    };

    UE_LOG(LogURLabEditor, Log, TEXT("Creating organizational hierarchy..."));
    Hierarchy.DefinitionsRoot = CreateOrgNode(TEXT("DefinitionsRoot"), TEXT("DefinitionsRoot"), SceneRoot);
    Hierarchy.DefaultsRoot    = CreateOrgNode(TEXT("DefaultsRoot"),    TEXT("DefaultsRoot"),    Hierarchy.DefinitionsRoot);
    Hierarchy.ActuatorsRoot   = CreateOrgNode(TEXT("ActuatorsRoot"),   TEXT("ActuatorsRoot"),   Hierarchy.DefinitionsRoot);
    Hierarchy.SensorsRoot     = CreateOrgNode(TEXT("SensorsRoot"),     TEXT("SensorsRoot"),     Hierarchy.DefinitionsRoot);
    Hierarchy.TendonsRoot     = CreateOrgNode(TEXT("TendonsRoot"),     TEXT("TendonsRoot"),     Hierarchy.DefinitionsRoot);
    Hierarchy.ContactsRoot    = CreateOrgNode(TEXT("ContactsRoot"),    TEXT("ContactsRoot"),    Hierarchy.DefinitionsRoot);
    Hierarchy.EqualitiesRoot  = CreateOrgNode(TEXT("EqualitiesRoot"),  TEXT("EqualitiesRoot"),  Hierarchy.DefinitionsRoot);
    Hierarchy.KeyframesRoot   = CreateOrgNode(TEXT("KeyframesRoot"),   TEXT("KeyframesRoot"),   Hierarchy.DefinitionsRoot);

    return Hierarchy;
}

void UMujocoGenerationAction::SetupEmptyArticulation(UBlueprint* BP)
{
    if (!BP) return;

    // 1. Create the organizational roots
    CreateOrganizationalHierarchy(BP);

    // 2. Create a default "worldbody" to get the user started
    USCS_Node* SceneRoot = BP->SimpleConstructionScript->GetDefaultSceneRootNode();
    USCS_Node* MainBodyNode = BP->SimpleConstructionScript->CreateNode(UMjWorldBody::StaticClass(), TEXT("worldbody"));
    if (MainBodyNode)
    {
        MainBodyNode->SetVariableName(TEXT("worldbody"));
        BP->SimpleConstructionScript->AddNode(MainBodyNode);
        UE_LOG(LogURLabEditor, Log, TEXT("Created default 'worldbody' for new articulation."));
    }

    // 3. Compile the Blueprint to finalize
    FKismetEditorUtilities::CompileBlueprint(BP);
}
