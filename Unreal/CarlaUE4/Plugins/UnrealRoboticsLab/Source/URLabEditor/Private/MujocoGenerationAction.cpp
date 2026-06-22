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

// ProcessDefault 已移除 — 默认类的创建由 UMjDefault + mjs_addDefault API处理。


UMujocoGenerationAction::UMujocoGenerationAction()
{
    // SupportedClasses.Add(UBlueprint::StaticClass()); // 将 UBlueprint 类型的 UClass* 类型注册到当前对象维护的“支持的类”集合里
}


// 从编辑器选中的资产（机器人蓝图）生成 MuJoCo 相关的组件
void UMujocoGenerationAction::GenerateMuJoCoComponents()
{
    // 清除内部默认的节点缓存
    CreatedDefaultNodes.Empty();

    UE_LOG(LogURLabEditor, Log, TEXT("Generating MuJoCo model components"));
    TArray<UObject*> SelectedAssets = UEditorUtilityLibrary::GetSelectedAssets();  // 获取当前在编辑器中选中的资产

    for (UObject* Asset : SelectedAssets)
    {
        UBlueprint* BP = Cast<UBlueprint>(Asset);
        if (!BP || !BP->GeneratedClass->IsChildOf(AMjArticulation::StaticClass()))  // 找出是 UBlueprint 的资产，并且其生成类是 AMjArticulation 的子类（即期望的 MuJoCo 铰链蓝图）
        {
            continue;
        }

        AMjArticulation* CDO = Cast<AMjArticulation>(BP->GeneratedClass->GetDefaultObject());  // 取得每个符合条件蓝图的 类默认对象（Class Default Object, CDO）
        if (!CDO || CDO->MuJoCoXMLFile.FilePath.IsEmpty())  // 每个蓝图类都应该有对应的 xml 文件路径
        {
            UE_LOG(LogURLabEditor, Error, TEXT("No XML File Path set in Blueprint Defaults for %s"), *BP->GetName());
            continue;
        }

        GenerateForBlueprint(BP, CDO->MuJoCoXMLFile.FilePath);  // 编译蓝图，使其生效
    }
}


// 对指定蓝图执行基于 MuJoCo XML 的生成工作
void UMujocoGenerationAction::GenerateForBlueprint(UBlueprint* BP, const FString& XMLPath)
{
    if (!BP || XMLPath.IsEmpty()) return;

    // 可选：MuJoCo XML 基本校验（如果追求完全纯净，可以移除，但保留也无妨）
    mjModel* M = nullptr;
    char Error[1000];
    M = mj_loadXML(TCHAR_TO_UTF8(*XMLPath), nullptr, Error, 1000);  // 用 mj_loadXML 尝试加载 XML
    if (!M) {
        UE_LOG(LogURLabEditor, Error, TEXT("MuJoCo Load/Validation Error: %s"), UTF8_TO_TCHAR(Error));
        // 我们可以选择返回或继续。如果XML无效，纯解析也可能失败或产生无用结果。
        return;
    }
    mj_deleteModel(M);  // 校验成功立即用 mj_deleteModel 释放临时模型

    // 执行纯 XML 解析、资产导入与 SCS 节点/组件创建等具体生成逻辑
    GenerateForBlueprintXml(BP, XMLPath);

    FKismetEditorUtilities::CompileBlueprint(BP);  // 对蓝图所做的修改写入并生效
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

    const FXmlNode* Root = InXmlFile->GetRootNode();  // 获取 xml 文件的根节点：mujoco
    if (!Root) return;

    FString XMLDir = FPaths::GetPath(XMLPath);
    USCS_Node* SceneRoot = BP->SimpleConstructionScript->GetDefaultSceneRootNode();  // 获取场景的根节点

    FString BaseContentPath = TEXT("/Game/MuJoCoImports/");
    FString BaseName = FPaths::GetBaseFilename(XMLPath);
    if (BaseName.IsEmpty()) BaseName = TEXT("MemModel");

    FString AssetImportPath = BaseContentPath + BaseName + TEXT("_Assets");  // 资产导入后的路径：hutb/Unreal/CarlaUE4/Content/MuJoCoImports/g1_29dof_rev_1_0_ue_Assets/
    AssetImportPath = UPackageTools::SanitizePackageName(AssetImportPath);

    // 0. 预扫描：收集默认网格缩放，以便资产解析可以继承它们
    CollectDefaultMeshScales(Root);

    // 1. 为后面的步骤传递已解析的资产：网格资产MeshAssets、网格缩放MeshScales、纹理资产TextureAssets、材质数据MaterialData
    TMap<FString, FString> MeshAssets;  // 35个
    TMap<FString, FVector> MeshScales;  // 35个
    TMap<FString, FString> TextureAssets;  // 纹理 1 个：g1\groundplane
    TMap<FString, FMuJoCoMaterialData> MaterialData;  // 材质 1 个：groundplane
    ParseAssetsRecursive(Root, XMLDir, MeshAssets, MeshScales, TextureAssets, MaterialData);  // 递归遍历 MuJoCo XML 中和资源相关的节点，把 mesh（网格资产35个） / texture（可以引用材质） / material（反光等） 等资产信息解析出来，并整理成映射表（键、值），供后续导入 Unreal 使用

    // 1b. 导入纹理
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

    // 2. 动态创建有组织的节点
    FArticulationHierarchy Hierarchy = CreateOrganizationalHierarchy(BP);

    USCS_Node* DefinitionsNode = Hierarchy.DefinitionsRoot;
    USCS_Node* DefaultsNode    = Hierarchy.DefaultsRoot;
    USCS_Node* ActuatorsNode   = Hierarchy.ActuatorsRoot;
    USCS_Node* SensorsNode     = Hierarchy.SensorsRoot;
    USCS_Node* TendonsNode     = Hierarchy.TendonsRoot;
    USCS_Node* ContactsNode    = Hierarchy.ContactsRoot;
    USCS_Node* EqualitiesNode  = Hierarchy.EqualitiesRoot;
    USCS_Node* KeyframesNode   = Hierarchy.KeyframesRoot;

    // 2. 首先解析编译器设置（角度 angle、解析欧拉角时轴的顺序 eulerseq），
    //    以便将其传播到<default>-block导入中
    //    ——默认类中的关节范围依赖于编译器级别的`angle`设置。
    FMjCompilerSettings CompilerSettings = MjOrientationUtils::ParseCompilerSettings(Root);

    // 2a. 解析：默认
    ParseDefaultsRecursive(Root, BP, DefaultsNode, XMLDir, CompilerSettings, TEXT(""));

    // 2b. 解析：接触对、排除项
    ParseContactSection(Root, BP, ContactsNode, XMLDir);

    // 2c. 解析（传递）：等式约束
    ParseEqualitySection(Root, BP, EqualitiesNode, XMLDir);

    // 2d. 解析（传递）：关键帧
    ParseKeyframeSection(Root, BP, KeyframesNode, XMLDir);

    // 3. 传递：结构遍历
    USCS_Node* WorldBodyNode = nullptr;
    for (const FXmlNode* Child : Root->GetChildrenNodes())
    {
         FString Tag = Child->GetTag();
         if (Tag.Equals(TEXT("worldbody")))
         {
             // 仅创建一次 worldbody 节点 — 将后续的 <worldbody> 部分合并到其中
             if (!WorldBodyNode)
             {
                 WorldBodyNode = BP->SimpleConstructionScript->CreateNode(UMjWorldBody::StaticClass(), TEXT("worldbody"));
                 WorldBodyNode->SetVariableName(TEXT("worldbody"));
                 BP->SimpleConstructionScript->AddNode(WorldBodyNode);
             }

             // 递归导入世界体（worldbody）子节点
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

    // 4. 解析 <option> 并存储在铰链的类默认对象（Class Default Object，CDO）中，以便 Mujoco 管理器（AAMjManager）能够在运行时应用它们
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
                // sleep_tolerance是一个直接选项属性（不在<flag>中）
                V = Child->GetAttribute(TEXT("sleep_tolerance"));
                if (!V.IsEmpty()) Opts.SleepTolerance = FCString::Atof(*V);

                // 也检查子 <flag> 节点内部（MuJoCo XML 规范两者都允许）
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
