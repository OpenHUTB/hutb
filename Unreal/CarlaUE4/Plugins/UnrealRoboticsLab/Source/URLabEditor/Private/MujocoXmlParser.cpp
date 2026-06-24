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

// MujocoXmlParser.cpp — XML parsing methods for UMujocoGenerationAction.

#include "MujocoGenerationAction.h"
#include "URLabEditorLogging.h"
#include "Engine/StaticMesh.h"
#include "Components/StaticMeshComponent.h"
#include "MuJoCo/Components/MjComponent.h"
#include "MuJoCo/Components/Bodies/MjBody.h"
#include "MuJoCo/Components/Bodies/MjWorldBody.h"
#include "MuJoCo/Components/Bodies/MjFrame.h"
#include "MuJoCo/Components/Joints/MjJoint.h"
#include "MuJoCo/Components/Joints/MjHingeJoint.h"
#include "MuJoCo/Components/Joints/MjSlideJoint.h"
#include "MuJoCo/Components/Joints/MjBallJoint.h"
#include "MuJoCo/Components/Joints/MjFreeJoint.h"

#include "MuJoCo/Components/Sensors/MjSensor.h"
#include "MuJoCo/Components/Sensors/MjTouchSensor.h"
#include "MuJoCo/Components/Sensors/MjAccelerometer.h"
#include "MuJoCo/Components/Sensors/MjVelocimeter.h"
#include "MuJoCo/Components/Sensors/MjGyro.h"
#include "MuJoCo/Components/Sensors/MjForceSensor.h"
#include "MuJoCo/Components/Sensors/MjTorqueSensor.h"
#include "MuJoCo/Components/Sensors/MjMagnetometer.h"
#include "MuJoCo/Components/Sensors/MjCamProjectionSensor.h"
#include "MuJoCo/Components/Sensors/MjRangeFinderSensor.h"
#include "MuJoCo/Components/Sensors/MjJointPosSensor.h"
#include "MuJoCo/Components/Sensors/MjJointVelSensor.h"
#include "MuJoCo/Components/Sensors/MjTendonPosSensor.h"
#include "MuJoCo/Components/Sensors/MjTendonVelSensor.h"
#include "MuJoCo/Components/Sensors/MjActuatorPosSensor.h"
#include "MuJoCo/Components/Sensors/MjActuatorVelSensor.h"
#include "MuJoCo/Components/Sensors/MjActuatorFrcSensor.h"
#include "MuJoCo/Components/Sensors/MjJointActFrcSensor.h"
#include "MuJoCo/Components/Sensors/MjTendonActFrcSensor.h"
#include "MuJoCo/Components/Sensors/MjBallQuatSensor.h"
#include "MuJoCo/Components/Sensors/MjBallAngVelSensor.h"
#include "MuJoCo/Components/Sensors/MjJointLimitPosSensor.h"
#include "MuJoCo/Components/Sensors/MjJointLimitVelSensor.h"
#include "MuJoCo/Components/Sensors/MjJointLimitFrcSensor.h"
#include "MuJoCo/Components/Sensors/MjTendonLimitPosSensor.h"
#include "MuJoCo/Components/Sensors/MjTendonLimitVelSensor.h"
#include "MuJoCo/Components/Sensors/MjTendonLimitFrcSensor.h"
#include "MuJoCo/Components/Sensors/MjFramePosSensor.h"
#include "MuJoCo/Components/Sensors/MjFrameQuatSensor.h"
#include "MuJoCo/Components/Sensors/MjFrameXAxisSensor.h"
#include "MuJoCo/Components/Sensors/MjFrameYAxisSensor.h"
#include "MuJoCo/Components/Sensors/MjFrameZAxisSensor.h"
#include "MuJoCo/Components/Sensors/MjFrameLinVelSensor.h"
#include "MuJoCo/Components/Sensors/MjFrameAngVelSensor.h"
#include "MuJoCo/Components/Sensors/MjFrameLinAccSensor.h"
#include "MuJoCo/Components/Sensors/MjFrameAngAccSensor.h"
#include "MuJoCo/Components/Sensors/MjSubtreeComSensor.h"
#include "MuJoCo/Components/Sensors/MjSubtreeLinVelSensor.h"
#include "MuJoCo/Components/Sensors/MjSubtreeAngMomSensor.h"
#include "MuJoCo/Components/Sensors/MjInsideSiteSensor.h"
#include "MuJoCo/Components/Sensors/MjGeomDistSensor.h"
#include "MuJoCo/Components/Sensors/MjGeomNormalSensor.h"
#include "MuJoCo/Components/Sensors/MjGeomFromToSensor.h"
#include "MuJoCo/Components/Sensors/MjContactSensor.h"
#include "MuJoCo/Components/Sensors/MjEPotentialSensor.h"
#include "MuJoCo/Components/Sensors/MjEKineticSensor.h"
#include "MuJoCo/Components/Sensors/MjClockSensor.h"
#include "MuJoCo/Components/Sensors/MjTactileSensor.h"

#include "MuJoCo/Components/Actuators/MjActuator.h"
#include "MuJoCo/Components/Actuators/MjMotorActuator.h"
#include "MuJoCo/Components/Actuators/MjPositionActuator.h"
#include "MuJoCo/Components/Actuators/MjVelocityActuator.h"
#include "MuJoCo/Components/Actuators/MjMuscleActuator.h"
#include "MuJoCo/Components/Actuators/MjDamperActuator.h"
#include "MuJoCo/Components/Actuators/MjCylinderActuator.h"
#include "MuJoCo/Components/Actuators/MjIntVelocityActuator.h"
#include "MuJoCo/Components/Actuators/MjAdhesionActuator.h"
#include "MuJoCo/Components/Actuators/MjDcMotorActuator.h"
#include "MuJoCo/Components/Actuators/MjGeneralActuator.h"
#include "MuJoCo/Components/Tendons/MjTendon.h"

#include "MuJoCo/Components/Defaults/MjDefault.h"
#include "MuJoCo/Components/Physics/MjContactPair.h"
#include "MuJoCo/Components/Physics/MjContactExclude.h"
#include "MuJoCo/Components/Geometry/MjSite.h"
#include "MuJoCo/Components/Sensors/MjCamera.h"
#include "MuJoCo/Components/Geometry/MjGeom.h"
#include "MuJoCo/Components/Geometry/Primitives/MjBox.h"
#include "MuJoCo/Components/Geometry/Primitives/MjSphere.h"
#include "MuJoCo/Components/Geometry/Primitives/MjCylinder.h"
#include "MuJoCo/Components/Geometry/Primitives/MjCapsule.h"
#include "MuJoCo/Components/Geometry/MjMeshGeom.h"
#include "MuJoCo/Components/Physics/MjInertial.h"
#include "MuJoCo/Components/Constraints/MjEquality.h"
#include "MuJoCo/Components/Deformable/MjFlexcomp.h"
#include "MuJoCo/Components/Keyframes/MjKeyframe.h"

#include "Engine/SCS_Node.h"
#include "Engine/SimpleConstructionScript.h"
#include "XmlFile.h"
#include "XmlNode.h"


/**
 * Resolves a geom's material name through the default class chain by walking SCS nodes.
 * Used during import when component templates have no owner actor.
 */
static FString ResolveMaterialFromDefaults(const UMjGeom* GeomComp, UBlueprint* BP)
{
    if (!GeomComp || !BP || !BP->SimpleConstructionScript) return FString();

    // If the geom has an explicit material, use it
    if (!GeomComp->MaterialName.IsEmpty()) return GeomComp->MaterialName;

    // Find the default class name (explicit on the geom, or from parent body's childclass)
    FString ClassName = GeomComp->MjClassName;

    // If no explicit class, try parent body's childclass by walking SCS parent nodes
    if (ClassName.IsEmpty())
    {
        TArray<USCS_Node*> AllNodes = BP->SimpleConstructionScript->GetAllNodes();
        // Find this geom's SCS node, then walk up to find a body with ChildClassName
        for (USCS_Node* Node : AllNodes)
        {
            if (Node->ComponentTemplate == GeomComp)
            {
                // Walk up parent nodes
                for (USCS_Node* Parent : AllNodes)
                {
                    if (Parent->ChildNodes.Contains(Node))
                    {
                        if (UMjBody* Body = Cast<UMjBody>(Parent->ComponentTemplate))
                        {
                            if (Body->bOverride_ChildClassName && !Body->ChildClassName.IsEmpty())
                            {
                                ClassName = Body->ChildClassName;
                            }
                        }
                        break;
                    }
                }
                break;
            }
        }
    }

    if (ClassName.IsEmpty()) return FString();

    // Walk the default class chain looking for a geom with MaterialName set
    TArray<USCS_Node*> AllNodes = BP->SimpleConstructionScript->GetAllNodes();
    TSet<FString> Visited;

    while (!ClassName.IsEmpty() && !Visited.Contains(ClassName))
    {
        Visited.Add(ClassName);

        // Find the UMjDefault with this class name
        for (USCS_Node* Node : AllNodes)
        {
            UMjDefault* Def = Cast<UMjDefault>(Node->ComponentTemplate);
            if (!Def || Def->ClassName != ClassName) continue;

            // Check its child geom for a material
            for (USCS_Node* ChildNode : Node->ChildNodes)
            {
                if (UMjGeom* ChildGeom = Cast<UMjGeom>(ChildNode->ComponentTemplate))
                {
                    if (!ChildGeom->MaterialName.IsEmpty())
                        return ChildGeom->MaterialName;
                }
            }

            // Walk to parent default
            ClassName = Def->ParentClassName;
            break;
        }
    }

    return FString();
}

void UMujocoGenerationAction::ImportNodeRecursive(const FXmlNode* Node, USCS_Node* ParentNode, UBlueprint* BP,
                                          const FString& XMLDir, const FString& AssetImportPath,
                                          const TMap<FString, FString>& MeshAssets,
                                          const TMap<FString, FVector>& MeshScales,
                                          const TMap<FString, FString>& TextureAssets,
                                          const TMap<FString, FMuJoCoMaterialData>& MaterialData,
                                          const TMap<FString, UTexture2D*>& ImportedTextures,
                                          const FMjCompilerSettings& CompilerSettings,
                                          bool bIsDefaultContext,
                                          USCS_Node* ReuseNode)
{
    if (!Node || !BP) return;

    const FString Tag = Node->GetTag();
    USCS_Node* CreatedNode = nullptr;
    USceneComponent* CreatedTemplate = nullptr;

    // --- INCLUDE ---
    if (Tag.Equals(TEXT("include")))
    {
        FString FileAttr = Node->GetAttribute(TEXT("file"));
        if (!FileAttr.IsEmpty())
        {
             FString IncludePath = FPaths::Combine(XMLDir, FileAttr);
             FXmlFile IncludedFile(IncludePath);
             if (IncludedFile.IsValid())
             {
                 // 遍历包含根的子节点
                 for (const FXmlNode* Child : IncludedFile.GetRootNode()->GetChildrenNodes())
                 {
                     ImportNodeRecursive(Child, ParentNode, BP, FPaths::GetPath(IncludePath), AssetImportPath, MeshAssets, MeshScales, TextureAssets, MaterialData, ImportedTextures, CompilerSettings, bIsDefaultContext, ReuseNode);
                 }
             }
             else
             {
                 UE_LOG(LogURLabEditor, Warning, TEXT("Failed to load include: %s"), *IncludePath);
             }
        }
        return; // 已完成包含节点本身的操作。
    }

    // --- BODY ---
    if (Tag.Equals(TEXT("body")) || Tag.Equals(TEXT("worldbody")))
    {
        if (Tag.Equals(TEXT("worldbody")))
        {
             USCS_Node* WorldBodyNode = BP->SimpleConstructionScript->CreateNode(UMjWorldBody::StaticClass(), TEXT("worldbody"));
             WorldBodyNode->SetVariableName(TEXT("worldbody"));
             if (ParentNode) ParentNode->AddChildNode(WorldBodyNode);

             UMjWorldBody* WorldBodyComp = Cast<UMjWorldBody>(WorldBodyNode->ComponentTemplate);
             if (WorldBodyComp)
             {
                 WorldBodyComp->bIsDefault = bIsDefaultContext;
             }

             for (const FXmlNode* Child : Node->GetChildrenNodes())
             {
                 ImportNodeRecursive(Child, WorldBodyNode, BP, XMLDir, AssetImportPath, MeshAssets, MeshScales, TextureAssets, MaterialData, ImportedTextures, CompilerSettings, bIsDefaultContext, ReuseNode);
             }
             return;
        }

        // Regular Body
        FString Name = Node->GetAttribute(TEXT("name"));
        if (Name.IsEmpty())
        {
            FString ParentName = ParentNode ? ParentNode->GetVariableName().ToString() : TEXT("Body");
            Name = ParentName + TEXT("_Body");
        }

        if (ReuseNode)
        {
            CreatedNode = ReuseNode;
            UE_LOG(LogURLabEditor, Log, TEXT("Reusing Root Body for: %s"), *Name);
        }
        else
        {
            CreatedNode = BP->SimpleConstructionScript->CreateNode(UMjBody::StaticClass(), *Name);
        }

        UMjBody* BodyComp = Cast<UMjBody>(CreatedNode->ComponentTemplate);
        if (BodyComp)
        {
            BodyComp->ImportFromXml(Node, CompilerSettings);
            BodyComp->bIsDefault = bIsDefaultContext;
            FString NameAttr = Node->GetAttribute(TEXT("name"));
            if (!NameAttr.IsEmpty()) BodyComp->MjName = NameAttr;
        }
    }
    // --- FRAME ---
    // <frame> 会对嵌套的子元素应用位置/区偏移，并在编译时被解散。
    // 我们将其表示为一个 UMjFrame SCS 节点，其子元素附加到该节点上。
    else if (Tag.Equals(TEXT("frame")))
    {
        FString Name = Node->GetAttribute(TEXT("name"));
        if (Name.IsEmpty())
        {
            FString ParentName = ParentNode ? ParentNode->GetVariableName().ToString() : TEXT("Frame");
            Name = ParentName + TEXT("_Frame");
        }

        CreatedNode = BP->SimpleConstructionScript->CreateNode(UMjFrame::StaticClass(), *Name);
        UMjFrame* FrameComp = Cast<UMjFrame>(CreatedNode->ComponentTemplate);
        if (FrameComp)
        {
            FrameComp->ImportFromXml(Node, CompilerSettings);
            FString NameAttr = Node->GetAttribute(TEXT("name"));
            if (!NameAttr.IsEmpty()) FrameComp->MjName = NameAttr;
        }

        // 将帧节点附加到其父节点，然后递归地将它的子节点附加到该父节点上。
        if (ParentNode) ParentNode->AddChildNode(CreatedNode);
        else BP->SimpleConstructionScript->GetDefaultSceneRootNode()->AddChildNode(CreatedNode);

        for (const FXmlNode* Child : Node->GetChildrenNodes())
        {
            ImportNodeRecursive(Child, CreatedNode, BP, XMLDir, AssetImportPath, MeshAssets, MeshScales,
                TextureAssets, MaterialData, ImportedTextures, CompilerSettings, bIsDefaultContext);
        }
        return; // 以上已处理子节点
    }
    // --- 几何体 GEOM ---
    else if (Tag.Equals(TEXT("geom")))
    {
        FString Name = Node->GetAttribute(TEXT("name"));
        FString TypeStr = Node->GetAttribute(TEXT("type"));
        FString MeshAttr = Node->GetAttribute(TEXT("mesh"));

        // 如果没有显式类型但具有网格属性，则它是网格几何体（geom）。
        if (TypeStr.IsEmpty() && !MeshAttr.IsEmpty())
        {
            TypeStr = TEXT("mesh");
        }

        if (Name.IsEmpty())
        {
            FString GeomTypeName = TypeStr.IsEmpty() ? TEXT("Sphere") : TypeStr;
            GeomTypeName[0] = FChar::ToUpper(GeomTypeName[0]);
            Name = TEXT("Geom_") + GeomTypeName;
        }
        UClass* Class = UMjGeom::StaticClass();
        if (TypeStr == "box") Class = UMjBox::StaticClass();
        else if (TypeStr == "sphere") Class = UMjSphere::StaticClass();
        else if (TypeStr == "cylinder") Class = UMjCylinder::StaticClass();
        else if (TypeStr == "capsule") Class = UMjCapsule::StaticClass();
        else if (TypeStr == "mesh") Class = UMjMeshGeom::StaticClass();

        CreatedNode = BP->SimpleConstructionScript->CreateNode(Class, *Name);
        UMjGeom* GeomComp = Cast<UMjGeom>(CreatedNode->ComponentTemplate);
        if (GeomComp)
        {
            GeomComp->ImportFromXml(Node, CompilerSettings);
            GeomComp->bIsDefault = bIsDefaultContext;

            // 保留 MJCF 的“名称”，
            // 以便其他组件（包裹几何体的肌腱、引用它的接触对）
            // 可以通过原始名称而不是我们自动生成的 SCS 变量名称（Simple Construction Script 中用于标识这些组件节点的变量名（即组件在蓝图中的字段/变量名））来解析。
            FString NameAttr = Node->GetAttribute(TEXT("name"));
            if (!NameAttr.IsEmpty()) GeomComp->MjName = NameAttr;

            // 从类属性解析默认类
            {
                FString ClassAttr = Node->GetAttribute(TEXT("class"));
                if (ClassAttr.IsEmpty()) ClassAttr = TEXT("main");
                if (CreatedDefaultNodes.Contains(ClassAttr))
                {
                    UMjDefault* DefComp = Cast<UMjDefault>(CreatedDefaultNodes[ClassAttr]->ComponentTemplate);
                    if (DefComp)
                    {
                        GeomComp->DefaultClass = DefComp;
                    }
                }
            }

            // 解析用于视觉网格放置的默认类变换。
            // 遍历默认类层次结构（子类 -> 父类 -> ... -> 主类），
            // 找到第一个具有变换覆盖的默认几何体。
            // MjGeom 组件本身不会被修改（否则会在 ExportTo 中重复应用）。
            // 我们仅将偏移量应用于视觉 StaticMeshComponent 子组件。
            FTransform DefaultVisualOffset = FTransform::Identity;
            {
                FString SearchClassName = Node->GetAttribute(TEXT("class"));
                if (SearchClassName.IsEmpty()) SearchClassName = TEXT("main");

                bool bFoundRot = GeomComp->bOverride_Quat;
                bool bFoundPos = GeomComp->bOverride_Pos;

                // 按照默认层级结构向上走
                while ((!bFoundRot || !bFoundPos) && !SearchClassName.IsEmpty())
                {
                    if (CreatedDefaultNodes.Contains(SearchClassName))
                    {
                        USCS_Node* DefNode = CreatedDefaultNodes[SearchClassName];
                        if (DefNode)
                        {
                            // 在此默认类下找到默认的 geom 组件
                            for (USCS_Node* DefChild : DefNode->GetChildNodes())
                            {
                                UMjGeom* DefGeom = Cast<UMjGeom>(DefChild->ComponentTemplate);
                                if (DefGeom)
                                {
                                    if (!bFoundRot && DefGeom->bOverride_Quat)
                                    {
                                        DefaultVisualOffset.SetRotation(DefGeom->GetRelativeRotation().Quaternion());
                                        bFoundRot = true;
                                        UE_LOG(LogURLabEditor, Log, TEXT("[Geom Default] '%s': visual rotation from default '%s'"),
                                            *Name, *SearchClassName);
                                    }
                                    if (!bFoundPos && DefGeom->bOverride_Pos)
                                    {
                                        DefaultVisualOffset.SetLocation(DefGeom->GetRelativeLocation());
                                        bFoundPos = true;
                                        UE_LOG(LogURLabEditor, Log, TEXT("[Geom Default] '%s': visual position from default '%s'"),
                                            *Name, *SearchClassName);
                                    }
                                    break;
                                }
                            }

                            // 向上走到父类
                            UMjDefault* DefComp = Cast<UMjDefault>(DefNode->ComponentTemplate);
                            if (DefComp && !DefComp->ParentClassName.IsEmpty())
                            {
                                SearchClassName = DefComp->ParentClassName;
                            }
                            else
                            {
                                break; // 没有父类，则停止
                            }
                        }
                        else
                        {
                            break;
                        }
                    }
                    else
                    {
                        break; // 类没有找到
                    }
                }
            }

            // 处理网格视觉
            if (GeomComp->Type == EMjGeomType::Mesh)
            {
                 FString MeshName = Node->GetAttribute(TEXT("mesh"));
                 FString GeomClass = Node->GetAttribute(TEXT("class"));
                 int32 GeomGroup = GeomComp->Group;
                 UE_LOG(LogURLabEditor, Log, TEXT("[Mesh Import] Geom '%s': mesh='%s', class='%s', group=%d"),
                     *Name, *MeshName, *GeomClass, GeomGroup);

                 if (!MeshName.IsEmpty() && MeshAssets.Contains(MeshName))
                 {
                      FString MeshFile = MeshAssets[MeshName];
                      UE_LOG(LogURLabEditor, Log, TEXT("[Mesh Import]   -> Resolved mesh '%s' to file: %s"), *MeshName, *MeshFile);
                      FString MeshImportPath = AssetImportPath + TEXT("/Meshes");
                      UStaticMesh* NewMesh = ImportSingleMesh(MeshFile, MeshImportPath);
                      if (NewMesh)
                      {
                            FString VizNodeName = FString::Printf(TEXT("Viz_%s"), *MeshName);
                            USCS_Node* MeshNode = BP->SimpleConstructionScript->CreateNode(UStaticMeshComponent::StaticClass(), *VizNodeName);
                            CreatedNode->AddChildNode(MeshNode);

                            UStaticMeshComponent* MeshTemplate = Cast<UStaticMeshComponent>(MeshNode->ComponentTemplate);
                            if (MeshTemplate)
                            {
                                 MeshTemplate->SetStaticMesh(NewMesh);
                                 MeshTemplate->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
                                 MeshTemplate->SetCollisionResponseToAllChannels(ECR_Overlap);

                                 // 检查父级几何体是否具有 Group=3（碰撞/隐藏）
                                 if (GeomComp && GeomComp->Group == 3)
                                 {
                                     MeshTemplate->SetVisibility(false);
                                     MeshTemplate->bHiddenInGame = true;
                                 }

                                 // 如果存在，请应用 缩放（Scale）
                                 if (MeshScales.Contains(MeshName))
                                 {
                                     FVector Scale = MeshScales[MeshName];
                                     if (!Scale.Equals(FVector::OneVector))
                                     {
                                         MeshTemplate->SetRelativeScale3D(Scale);
                                         UE_LOG(LogURLabEditor, Log, TEXT("Applying scale %s to mesh %s"), *Scale.ToString(), *MeshName);
                                     }
                                 }

                                 // 应用默认类视觉偏移（例如，visual_zflip 旋转）
                                 // 这只会影响视觉网格，不会影响 MjGeom 组件，
                                 // 因此 ExportTo 不会重复应用默认值。
                                 if (!DefaultVisualOffset.GetRotation().IsIdentity(SMALL_NUMBER))
                                 {
                                     MeshTemplate->SetRelativeRotation(DefaultVisualOffset.GetRotation());
                                     UE_LOG(LogURLabEditor, Log, TEXT("Applied default visual rotation to mesh '%s'"), *MeshName);
                                 }
                                 if (!DefaultVisualOffset.GetLocation().IsNearlyZero())
                                 {
                                     MeshTemplate->SetRelativeLocation(DefaultVisualOffset.GetLocation());
                                     UE_LOG(LogURLabEditor, Log, TEXT("Applied default visual position to mesh '%s'"), *MeshName);
                                 }

                                 // 创建并分配材质实例
                                 FMuJoCoMaterialData MatData;
                                 MatData.Rgba = GeomComp->Rgba; // 默认使用几何体颜色

                                 // 如果引用了材质名称（通过默认链解析），则使用材质名称作为键；否则，回退到网格名称。
                                 FString ResolvedMat = ResolveMaterialFromDefaults(GeomComp, BP);
                                 FString MaterialKey = MeshName;
                                 if (!ResolvedMat.IsEmpty() && MaterialData.Contains(ResolvedMat))
                                 {
                                     MatData = MaterialData[ResolvedMat];
                                     MaterialKey = ResolvedMat;
                                     UE_LOG(LogURLabEditor, Log, TEXT("Using shared material '%s' for mesh '%s'"), *ResolvedMat, *MeshName);
                                 }

                                 // 创建或重用材质实例
                                 UMaterialInstanceConstant* MaterialInstance = CreateMaterialInstance(
                                     MaterialKey,
                                     MatData,
                                     ImportedTextures,
                                     AssetImportPath
                                 );

                                 if (MaterialInstance)
                                 {
                                     MeshTemplate->SetMaterial(0, MaterialInstance);
                                     UE_LOG(LogURLabEditor, Log, TEXT("Assigned material instance to mesh '%s'"), *MeshName);
                                 }
                            }
                      }
                 }
                 else if (!MeshName.IsEmpty())
                 {
                      UE_LOG(LogURLabEditor, Warning, TEXT("[Mesh Import]   -> Mesh '%s' NOT FOUND in MeshAssets map (%d entries)"),
                          *MeshName, MeshAssets.Num());
                 }
            }
            // 处理原始（Primitive）视觉效果（内置）
            else if (UStaticMeshComponent* BuiltInViz = GeomComp->GetVisualizerMesh())
            {
                 UE_LOG(LogURLabEditor, Log, TEXT("Applying visual properties to built-in visualizer for '%s'"), *Name);

                 // 创建并分配材质实例
                 FMuJoCoMaterialData MatData;
                 MatData.Rgba = GeomComp->Rgba; // 默认使用几何体颜色

                 // 如果引用了材质（通过默认链解析），则使用材料名称作为键；否则，回退到几何体名称。
                 FString ResolvedMat = ResolveMaterialFromDefaults(GeomComp, BP);
                 FString MaterialKey = Name;
                 if (!ResolvedMat.IsEmpty() && MaterialData.Contains(ResolvedMat))
                 {
                     MatData = MaterialData[ResolvedMat];
                     MaterialKey = ResolvedMat;
                 }

                 // 创建或重用材质实例
                 UMaterialInstanceConstant* MaterialInstance = CreateMaterialInstance(
                     MaterialKey,
                     MatData,
                     ImportedTextures,
                     AssetImportPath
                 );

                 if (MaterialInstance)
                 {
                     BuiltInViz->SetMaterial(0, MaterialInstance);
                 }

                 // Check for Group 3 visibility
                 if (GeomComp->Group == 3)
                 {
                     BuiltInViz->SetVisibility(false);
                     BuiltInViz->bHiddenInGame = true;
                 }
            }
        }
    }
    // --- 关节（JOINT） ---
    else if (Tag.Equals(TEXT("joint")))
    {
        FString Name = Node->GetAttribute(TEXT("name"));
        FString TypeStr = Node->GetAttribute(TEXT("type"));
        if (Name.IsEmpty())
        {
            FString JointTypeName = TypeStr.IsEmpty() ? TEXT("Hinge") : TypeStr;
            JointTypeName[0] = FChar::ToUpper(JointTypeName[0]);
            Name = JointTypeName + TEXT("Joint");
        }
        UClass* Class = UMjHingeJoint::StaticClass();
        if (TypeStr == "hinge") Class = UMjHingeJoint::StaticClass();
        else if (TypeStr == "slide") Class = UMjSlideJoint::StaticClass();
        else if (TypeStr == "ball") Class = UMjBallJoint::StaticClass();
        else if (TypeStr == "free") Class = UMjFreeJoint::StaticClass();

        CreatedNode = BP->SimpleConstructionScript->CreateNode(Class, *Name);
        UMjJoint* JointComp = Cast<UMjJoint>(CreatedNode->ComponentTemplate);
        if (JointComp)
        {
            JointComp->ImportFromXml(Node, CompilerSettings);
            JointComp->bIsDefault = bIsDefaultContext;

            // 保留 MJCF 的“名称”，以便在 SCS 唯一性消除 UE 变量名称的歧义后，
            // 引用此关节的执行器/等式/肌腱仍能继续解析
            // （例如，默认类“waist”已拥有 SCS 名称“waist”，
            // 则强制关节的变量名称为“waist1”）。
            // 否则，
            // 关节的规范名称将是消除歧义后的 UE 名称，
            // 并且执行器的 joint="waist" 引用将在编译时失败。
            FString NameAttr = Node->GetAttribute(TEXT("name"));
            if (!NameAttr.IsEmpty()) JointComp->MjName = NameAttr;

            FString ClassAttr = Node->GetAttribute(TEXT("class"));
            if (!ClassAttr.IsEmpty() && CreatedDefaultNodes.Contains(ClassAttr))
            {
                UMjDefault* DefComp = Cast<UMjDefault>(CreatedDefaultNodes[ClassAttr]->ComponentTemplate);
                if (DefComp) JointComp->DefaultClass = DefComp;
            }
        }
    }
    // --- FREEJOINT (Standalone) ---
    else if (Tag.Equals(TEXT("freejoint")))
    {
        FString Name = Node->GetAttribute(TEXT("name"));
        if (Name.IsEmpty()) Name = TEXT("FreeJoint");

        CreatedNode = BP->SimpleConstructionScript->CreateNode(UMjFreeJoint::StaticClass(), *Name);
        UMjJoint* JointComp = Cast<UMjJoint>(CreatedNode->ComponentTemplate);
        if (JointComp)
        {
            // 注意：独立的 `<freejoint/>` 没有属性。
            // 我们省略了 `ImportFromXml` 导入步骤，以避免任何不必要的移动或轴覆盖。
            JointComp->bIsDefault = bIsDefaultContext;
            FString NameAttr = Node->GetAttribute(TEXT("name"));
            if (!NameAttr.IsEmpty()) JointComp->MjName = NameAttr;
        }
    }
    // --- FLEXCOMP ---
    else if (Tag.Equals(TEXT("flexcomp")))
    {
        FString Name = Node->GetAttribute(TEXT("name"));
        if (Name.IsEmpty()) Name = TEXT("AUTONAME_Flexcomp");

        CreatedNode = BP->SimpleConstructionScript->CreateNode(UMjFlexcomp::StaticClass(), *Name);
        UMjFlexcomp* FlexComp = Cast<UMjFlexcomp>(CreatedNode->ComponentTemplate);
        if (FlexComp)
        {
            FlexComp->ImportFromXml(Node);

            // 对于网格类型，导入网格文件并创建一个子类 UStaticMeshComponent。
            FString FlexMeshFile = Node->GetAttribute(TEXT("file"));
            if (FlexComp->Type == EMjFlexcompType::Mesh && !FlexMeshFile.IsEmpty())
            {
                FString MeshName = FPaths::GetBaseFilename(FlexMeshFile);

                // Flexcomp 直接引用网格文件（而非通过 `<asset><mesh>` 标签）。
                // Python 预处理器会将其转换为 GLB 格式，并与原始文件一起转换。
                // 首先尝试使用 GLB 格式（预处理后），如果失败则回退到原始文件。
                FString MeshFilePath;
                if (MeshAssets.Contains(MeshName))
                {
                    MeshFilePath = MeshAssets[MeshName];
                }
                else
                {
                    // 先在 meshdir 中尝试 GLB，然后在 XMLDir 中尝试。
                    FString GlbName = FPaths::GetBaseFilename(FlexMeshFile) + TEXT(".glb");
                    FString GlbPath = FPaths::Combine(XMLDir, TEXT("asset"), GlbName);
                    if (FPaths::FileExists(GlbPath))
                    {
                        MeshFilePath = GlbPath;
                    }
                    else
                    {
                        GlbPath = FPaths::Combine(XMLDir, GlbName);
                        if (FPaths::FileExists(GlbPath))
                        {
                            MeshFilePath = GlbPath;
                        }
                        else
                        {
                            // 恢复到原始文件
                            MeshFilePath = FPaths::Combine(XMLDir, TEXT("asset"), FlexMeshFile);
                            if (!FPaths::FileExists(MeshFilePath))
                            {
                                MeshFilePath = FPaths::Combine(XMLDir, FlexMeshFile);
                            }
                        }
                    }
                }

                if (FPaths::FileExists(MeshFilePath))
                {
                    FString MeshImportPath = AssetImportPath + TEXT("/Meshes");
                    UStaticMesh* NewMesh = ImportSingleMesh(MeshFilePath, MeshImportPath);
                    if (NewMesh)
                    {
                        FString VizNodeName = FString::Printf(TEXT("Viz_%s"), *MeshName);
                        USCS_Node* MeshNode = BP->SimpleConstructionScript->CreateNode(UStaticMeshComponent::StaticClass(), *VizNodeName);
                        CreatedNode->AddChildNode(MeshNode);

                        UStaticMeshComponent* MeshTemplate = Cast<UStaticMeshComponent>(MeshNode->ComponentTemplate);
                        if (MeshTemplate)
                        {
                            MeshTemplate->SetStaticMesh(NewMesh);
                            MeshTemplate->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
                            MeshTemplate->SetCollisionResponseToAllChannels(ECR_Overlap);

                            if (MeshScales.Contains(MeshName))
                            {
                                FVector MeshScale = MeshScales[MeshName];
                                if (!MeshScale.Equals(FVector::OneVector))
                                {
                                    MeshTemplate->SetRelativeScale3D(MeshScale);
                                }
                            }
                        }
                    }
                }
                else
                {
                    UE_LOG(LogURLabEditor, Warning, TEXT("[Flexcomp] Mesh file not found: %s"), *MeshFilePath);
                }
            }
        }
    }
    // --- 位点（SITE） ---
    else if (Tag.Equals(TEXT("site")))
    {
        FString Name = Node->GetAttribute(TEXT("name"));
        if (Name.IsEmpty()) Name = TEXT("Site");
        CreatedNode = BP->SimpleConstructionScript->CreateNode(UMjSite::StaticClass(), *Name);
        UMjSite* SiteComp = Cast<UMjSite>(CreatedNode->ComponentTemplate);
        if (SiteComp)
        {
            SiteComp->ImportFromXml(Node, CompilerSettings);
            SiteComp->bIsDefault = bIsDefaultContext;
            FString NameAttr = Node->GetAttribute(TEXT("name"));
            if (!NameAttr.IsEmpty()) SiteComp->MjName = NameAttr;

            FString ClassAttr = Node->GetAttribute(TEXT("class"));
            if (!ClassAttr.IsEmpty() && CreatedDefaultNodes.Contains(ClassAttr))
            {
                UMjDefault* DefComp = Cast<UMjDefault>(CreatedDefaultNodes[ClassAttr]->ComponentTemplate);
                if (DefComp) SiteComp->DefaultClass = DefComp;
            }
        }
    }
    // --- INERTIAL ---
    else if (Tag.Equals(TEXT("inertial")))
    {
        CreatedNode = BP->SimpleConstructionScript->CreateNode(UMjInertial::StaticClass(), TEXT("Inertial"));
        UMjInertial* InertialComp = Cast<UMjInertial>(CreatedNode->ComponentTemplate);
        if (InertialComp)
        {
            InertialComp->ImportFromXml(Node, CompilerSettings);
            InertialComp->bIsDefault = bIsDefaultContext;
            FString NameAttr = Node->GetAttribute(TEXT("name"));
            if (!NameAttr.IsEmpty()) InertialComp->MjName = NameAttr;
        }
    }
    // --- 传感器（SENSOR） ---
    else if (Tag.Equals(TEXT("sensor")) || Tag.EndsWith(TEXT("sensor")) ||
             Tag == "touch" || Tag == "accelerometer" || Tag == "velocimeter" ||
             Tag == "gyro" || Tag == "force" || Tag == "torque" ||
             Tag == "magnetometer" || Tag == "camprojection" || Tag == "rangefinder" ||
             Tag == "jointpos" || Tag == "jointvel" || Tag == "tendonpos" ||
             Tag == "tendonvel" || Tag == "actuatorpos" || Tag == "actuatorvel" ||
             Tag == "actuatorfrc" || Tag == "jointactuatorfrc" || Tag == "tendonactuatorfrc" ||
             Tag == "ballquat" || Tag == "ballangvel" || Tag == "jointlimitpos" ||
             Tag == "jointlimitvel" || Tag == "jointlimitfrc" || Tag == "tendonlimitpos" ||
             Tag == "tendonlimitvel" || Tag == "tendonlimitfrc" || Tag == "framepos" ||
             Tag == "framequat" || Tag == "framexaxis" || Tag == "frameyaxis" ||
             Tag == "framezaxis" || Tag == "framelinvel" || Tag == "frameangvel" ||
             Tag == "framelinacc" || Tag == "frameangacc" || Tag == "insidesite" ||
             Tag == "subtreecom" || Tag == "subtreelinvel" || Tag == "subtreeangmom" ||
             Tag == "distance" || Tag == "normal" || Tag == "fromto" ||
             Tag == "contact" || Tag == "e_potential" || Tag == "e_kinetic" ||
             Tag == "clock" || Tag == "tactile" || Tag == "user" || Tag == "plugin")
    {
         FString Name = Node->GetAttribute(TEXT("name"));
         if (Name.IsEmpty())
         {
             FString SensorTag = Tag;
             SensorTag[0] = FChar::ToUpper(SensorTag[0]);
             Name = SensorTag + TEXT("Sensor");
         }

         UClass* Class = UMjSensor::StaticClass();
         if (Tag == "touch") Class = UMjTouchSensor::StaticClass();
         else if (Tag == "accelerometer") Class = UMjAccelerometer::StaticClass();
         else if (Tag == "velocimeter") Class = UMjVelocimeter::StaticClass();
         else if (Tag == "gyro") Class = UMjGyro::StaticClass();
         else if (Tag == "force") Class = UMjForceSensor::StaticClass();
         else if (Tag == "torque") Class = UMjTorqueSensor::StaticClass();
         else if (Tag == "magnetometer") Class = UMjMagnetometer::StaticClass();
         else if (Tag == "camprojection") Class = UMjCamProjectionSensor::StaticClass();
         else if (Tag == "rangefinder") Class = UMjRangeFinderSensor::StaticClass();
         else if (Tag == "jointpos") Class = UMjJointPosSensor::StaticClass();
         else if (Tag == "jointvel") Class = UMjJointVelSensor::StaticClass();
         else if (Tag == "tendonpos") Class = UMjTendonPosSensor::StaticClass();
         else if (Tag == "tendonvel") Class = UMjTendonVelSensor::StaticClass();
         else if (Tag == "actuatorpos") Class = UMjActuatorPosSensor::StaticClass();
         else if (Tag == "actuatorvel") Class = UMjActuatorVelSensor::StaticClass();
         else if (Tag == "actuatorfrc") Class = UMjActuatorFrcSensor::StaticClass();
         else if (Tag == "jointactuatorfrc") Class = UMjJointActFrcSensor::StaticClass();
         else if (Tag == "tendonactuatorfrc") Class = UMjTendonActFrcSensor::StaticClass();
         else if (Tag == "ballquat") Class = UMjBallQuatSensor::StaticClass();
         else if (Tag == "ballangvel") Class = UMjBallAngVelSensor::StaticClass();
         else if (Tag == "jointlimitpos") Class = UMjJointLimitPosSensor::StaticClass();
         else if (Tag == "jointlimitvel") Class = UMjJointLimitVelSensor::StaticClass();
         else if (Tag == "jointlimitfrc") Class = UMjJointLimitFrcSensor::StaticClass();
         else if (Tag == "tendonlimitpos") Class = UMjTendonLimitPosSensor::StaticClass();
         else if (Tag == "tendonlimitvel") Class = UMjTendonLimitVelSensor::StaticClass();
         else if (Tag == "tendonlimitfrc") Class = UMjTendonLimitFrcSensor::StaticClass();
         else if (Tag == "framepos") Class = UMjFramePosSensor::StaticClass();
         else if (Tag == "framequat") Class = UMjFrameQuatSensor::StaticClass();
         else if (Tag == "framexaxis") Class = UMjFrameXAxisSensor::StaticClass();
         else if (Tag == "frameyaxis") Class = UMjFrameYAxisSensor::StaticClass();
         else if (Tag == "framezaxis") Class = UMjFrameZAxisSensor::StaticClass();
         else if (Tag == "framelinvel") Class = UMjFrameLinVelSensor::StaticClass();
         else if (Tag == "frameangvel") Class = UMjFrameAngVelSensor::StaticClass();
         else if (Tag == "framelinacc") Class = UMjFrameLinAccSensor::StaticClass();
         else if (Tag == "frameangacc") Class = UMjFrameAngAccSensor::StaticClass();
         else if (Tag == "insidesite") Class = UMjInsideSiteSensor::StaticClass();
         else if (Tag == "subtreecom") Class = UMjSubtreeComSensor::StaticClass();
         else if (Tag == "subtreelinvel") Class = UMjSubtreeLinVelSensor::StaticClass();
         else if (Tag == "subtreeangmom") Class = UMjSubtreeAngMomSensor::StaticClass();
         else if (Tag == "distance") Class = UMjGeomDistSensor::StaticClass();
         else if (Tag == "normal") Class = UMjGeomNormalSensor::StaticClass();
         else if (Tag == "fromto") Class = UMjGeomFromToSensor::StaticClass();
         else if (Tag == "contact") Class = UMjContactSensor::StaticClass();
         else if (Tag == "e_potential") Class = UMjEPotentialSensor::StaticClass();
         else if (Tag == "e_kinetic") Class = UMjEKineticSensor::StaticClass();
         else if (Tag == "clock") Class = UMjClockSensor::StaticClass();
         else if (Tag == "tactile") Class = UMjTactileSensor::StaticClass();

         CreatedNode = BP->SimpleConstructionScript->CreateNode(Class, *Name);
         UMjSensor* SensComp = Cast<UMjSensor>(CreatedNode->ComponentTemplate);
         if (SensComp)
         {
            SensComp->ImportFromXml(Node);
            SensComp->bIsDefault = bIsDefaultContext;
            FString NameAttr = Node->GetAttribute(TEXT("name"));
            if (!NameAttr.IsEmpty()) SensComp->MjName = NameAttr;

            FString ClassAttr = Node->GetAttribute(TEXT("class"));
            if (!ClassAttr.IsEmpty() && CreatedDefaultNodes.Contains(ClassAttr))
            {
                UMjDefault* DefComp = Cast<UMjDefault>(CreatedDefaultNodes[ClassAttr]->ComponentTemplate);
                if (DefComp) SensComp->DefaultClass = DefComp;
            }
         }
    }
    // --- 相机（CAMERA） ---
    else if (Tag.Equals(TEXT("camera")))
    {
         FString Name = Node->GetAttribute(TEXT("name"));
         if (Name.IsEmpty()) Name = TEXT("Camera");

         CreatedNode = BP->SimpleConstructionScript->CreateNode(UMjCamera::StaticClass(), *Name);
         UMjCamera* CamComp = Cast<UMjCamera>(CreatedNode->ComponentTemplate);
         if (CamComp)
         {
             CamComp->ImportFromXml(Node, CompilerSettings);
             CamComp->bIsDefault = bIsDefaultContext;
             FString NameAttr = Node->GetAttribute(TEXT("name"));
             if (!NameAttr.IsEmpty()) CamComp->MjName = NameAttr;
         }
    }
    // --- 执行器（ACTUATOR） ---
    else if (Tag.Equals(TEXT("actuator")))
    {
         // <actuator> 容器本身只是一个包装器
         // ——递归地遍历每个类型的子项（电机、肌肉、位置等）来创建组件。
         for (const FXmlNode* Child : Node->GetChildrenNodes())
         {
             ImportNodeRecursive(Child, ParentNode, BP, XMLDir, AssetImportPath, MeshAssets, MeshScales, TextureAssets, MaterialData, ImportedTextures, CompilerSettings, bIsDefaultContext);
         }
         return;
    }
    else if (Tag == "motor" || Tag == "position" || Tag == "velocity" || Tag == "cylinder" || Tag == "muscle" || Tag == "general" || Tag == "damper" || Tag == "intvelocity" || Tag == "adhesion" || Tag == "dcmotor")
    {
         FString Name = Node->GetAttribute(TEXT("name"));
         if (Name.IsEmpty())
         {
             FString ActTag = Tag;
             ActTag[0] = FChar::ToUpper(ActTag[0]);
             Name = ActTag + TEXT("Actuator");
         }

         UClass* Class = UMjActuator::StaticClass();
         if (Tag == "motor") Class = UMjMotorActuator::StaticClass();
         else if (Tag == "position") Class = UMjPositionActuator::StaticClass();
         else if (Tag == "velocity") Class = UMjVelocityActuator::StaticClass();
         else if (Tag == "muscle") Class = UMjMuscleActuator::StaticClass();
         else if (Tag == "cylinder") Class = UMjCylinderActuator::StaticClass();
         else if (Tag == "damper") Class = UMjDamperActuator::StaticClass();
         else if (Tag == "intvelocity") Class = UMjIntVelocityActuator::StaticClass();
         else if (Tag == "adhesion") Class = UMjAdhesionActuator::StaticClass();
         else if (Tag == "dcmotor") Class = UMjDcMotorActuator::StaticClass();
         else if (Tag == "general") Class = UMjGeneralActuator::StaticClass();

         CreatedNode = BP->SimpleConstructionScript->CreateNode(Class, *Name);
         UMjActuator* ActComp = Cast<UMjActuator>(CreatedNode->ComponentTemplate);
         if (ActComp)
         {
            ActComp->ImportFromXml(Node);
            ActComp->bIsDefault = bIsDefaultContext;
            FString NameAttr = Node->GetAttribute(TEXT("name"));
            if (!NameAttr.IsEmpty()) ActComp->MjName = NameAttr;

            FString ClassAttr = Node->GetAttribute(TEXT("class"));
            if (!ClassAttr.IsEmpty() && CreatedDefaultNodes.Contains(ClassAttr))
            {
                UMjDefault* DefComp = Cast<UMjDefault>(CreatedDefaultNodes[ClassAttr]->ComponentTemplate);
                if (DefComp) ActComp->DefaultClass = DefComp;
            }
         }
    }
    // --- 肌腱（TENDON） ---
    else if (Tag.Equals(TEXT("tendon")) || Tag.Equals(TEXT("fixed")) || Tag.Equals(TEXT("spatial")))
    {
         if (Tag.Equals(TEXT("tendon")))
         {
              for (const FXmlNode* Child : Node->GetChildrenNodes())
              {
                  ImportNodeRecursive(Child, ParentNode, BP, XMLDir, AssetImportPath, MeshAssets, MeshScales, TextureAssets, MaterialData, ImportedTextures, CompilerSettings, bIsDefaultContext);
              }
              return;
         }

         FString Name = Node->GetAttribute(TEXT("name"));
         if (Name.IsEmpty())
         {
             FString TendonTag = Tag;
             TendonTag[0] = FChar::ToUpper(TendonTag[0]);
             Name = TendonTag + TEXT("Tendon");
         }

         CreatedNode = BP->SimpleConstructionScript->CreateNode(UMjTendon::StaticClass(), *Name);
         UMjTendon* TendonComp = Cast<UMjTendon>(CreatedNode->ComponentTemplate);
         if (TendonComp)
         {
            TendonComp->ImportFromXml(Node);
            TendonComp->bIsDefault = bIsDefaultContext;
            FString NameAttr = Node->GetAttribute(TEXT("name"));
            if (!NameAttr.IsEmpty()) TendonComp->MjName = NameAttr;
         }
    }

    // --- 附着和递归（ATTACH & RECURSE） ---
    if (CreatedNode)
    {
        if (ParentNode) ParentNode->AddChildNode(CreatedNode);
        else BP->SimpleConstructionScript->GetDefaultSceneRootNode()->AddChildNode(CreatedNode); // Fallback attach to root

        // Recurse for Children
        if (Tag.Equals(TEXT("body")) || Tag.Equals(TEXT("worldbody")))
        {
            for (const FXmlNode* Child : Node->GetChildrenNodes())
            {
                // 如果 CreatedNode 为 NULL（例如，重用了根节点），是否使用 ParentNode？否，CreatedNode 是子节点的父节点。
                // 如果我们重用了根节点，则 CreatedNode 就是根节点。
                ImportNodeRecursive(Child, CreatedNode, BP, XMLDir, AssetImportPath, MeshAssets, MeshScales, TextureAssets, MaterialData, ImportedTextures, CompilerSettings, bIsDefaultContext);
            }
        }
    }
}

void UMujocoGenerationAction::CollectDefaultMeshScales(const FXmlNode* Node, const FString& CurrentClass)
{
    if (!Node) return;
    const FString Tag = Node->GetTag();  // 获取根节点的标签，比如：mujoco、compiler

    if (Tag.Equals(TEXT("default")))
    {
        FString ClassName = Node->GetAttribute(TEXT("class"));
        if (ClassName.IsEmpty()) ClassName = TEXT("main");

        for (const FXmlNode* Child : Node->GetChildrenNodes())
        {
            if (Child->GetTag().Equals(TEXT("mesh")))
            {
                FString ScaleStr = Child->GetAttribute(TEXT("scale"));
                if (!ScaleStr.IsEmpty())
                {
                    TArray<FString> Parts;
                    ScaleStr.ParseIntoArray(Parts, TEXT(" "), true);
                    if (Parts.Num() >= 3)
                    {
                        FVector Scale(FCString::Atof(*Parts[0]), FCString::Atof(*Parts[1]), FCString::Atof(*Parts[2]));
                        DefaultMeshScales.Add(ClassName, Scale);  // 如果 XML 里有 <default class="xxx">，且它下面有<mesh scale = "a b c">，就把这个缩放记录到 DefaultMeshScales
                        UE_LOG(LogURLabEditor, Log, TEXT("[Default Mesh Scale] class='%s' scale=%s"), *ClassName, *Scale.ToString());
                    }
                }
            }
            else if (Child->GetTag().Equals(TEXT("default")))
            {
                CollectDefaultMeshScales(Child, ClassName);
            }
        }
    }
    else
    {
        for (const FXmlNode* Child : Node->GetChildrenNodes())
        {
            CollectDefaultMeshScales(Child, CurrentClass);  // 如果节点的标签不是default，则递归进入它的每一个子节点
        }  // 没有子节点则正常结束该层递归
    }
}

void UMujocoGenerationAction::ParseAssetsRecursive(const FXmlNode* Node, const FString& XMLDir, TMap<FString, FString>& OutMeshAssets, TMap<FString, FVector>& OutMeshScales, TMap<FString, FString>& OutTextureAssets, TMap<FString, FMuJoCoMaterialData>& OutMaterialData, const FString& MeshDir, const FString& TextureDir, const FString& AssetDir)
{
    if (!Node) return;
    const FString Tag = Node->GetTag();

    // 此上下文的目录覆盖（当前或继承的）
    FString CurrentMeshDir = MeshDir;
    FString CurrentTextureDir = TextureDir;
    FString CurrentAssetDir = AssetDir;

    // 如果这是一个容器，则在其直接子元素中查找编译器（compiler）标签，以便为所有同级元素设置目录覆盖。
    if (Tag.Equals(TEXT("mujoco")) || Tag.Equals(TEXT("include")) || Tag.Equals(TEXT("asset")))
    {
        for (const FXmlNode* Child : Node->GetChildrenNodes())
        {
            if (Child->GetTag().Equals(TEXT("compiler")))
            {
                FString AttributeMeshDir = Child->GetAttribute(TEXT("meshdir"));
                if (!AttributeMeshDir.IsEmpty()) CurrentMeshDir = AttributeMeshDir;

                FString AttributeTextureDir = Child->GetAttribute(TEXT("texturedir"));
                if (!AttributeTextureDir.IsEmpty()) CurrentTextureDir = AttributeTextureDir;

                FString AttributeAssetDir = Child->GetAttribute(TEXT("assetdir"));
                if (!AttributeAssetDir.IsEmpty()) CurrentAssetDir = AttributeAssetDir;
                break; // 假设每个节只有一个编译器（compiler）标签
            }
        }
    }

    // <include>：如果包含其他文件，则调用 ParseAssetsRecursive 进行递归解析
    if (Tag.Equals(TEXT("include")))
    {
        FString FileAttr = Node->GetAttribute(TEXT("file"));
        if (!FileAttr.IsEmpty())
        {
             FString IncludePath = FPaths::Combine(XMLDir, FileAttr);
             FXmlFile IncludedFile(IncludePath);
             if (IncludedFile.IsValid())
             {
                 ParseAssetsRecursive(IncludedFile.GetRootNode(), FPaths::GetPath(IncludePath), OutMeshAssets, OutMeshScales, OutTextureAssets, OutMaterialData, CurrentMeshDir, CurrentTextureDir, CurrentAssetDir);
             }
        }
    }
    // 解析 <asset> 标签内的内容
    else if (Tag.Equals(TEXT("asset")))
    {
        for (const FXmlNode* Child : Node->GetChildrenNodes())
        {
            ParseAssetsRecursive(Child, XMLDir, OutMeshAssets, OutMeshScales, OutTextureAssets, OutMaterialData, CurrentMeshDir, CurrentTextureDir, CurrentAssetDir);
        }
    }
    // 将网格 <mesh> 内容解析为映射表
    else if (Tag.Equals(TEXT("mesh")))
    {
        FString MeshName = Node->GetAttribute(TEXT("name"));
        FString MeshFile = Node->GetAttribute(TEXT("file"));
        if (MeshFile.IsEmpty()) MeshFile = MeshName;

        if (!MeshFile.IsEmpty())
        {
            if (MeshName.IsEmpty()) MeshName = FPaths::GetBaseFilename(MeshFile);  // 如果网格的名字标签缺失，则默认网格名为不带后缀名的文件名

            // 优先级：meshdir > assetdir > xml 当前目录（XMLDir）
            FString EffectiveMeshBase = XMLDir;
            if (!CurrentMeshDir.IsEmpty())
            {
                EffectiveMeshBase = FPaths::Combine(XMLDir, CurrentMeshDir);
            }
            else if (!CurrentAssetDir.IsEmpty())
            {
                EffectiveMeshBase = FPaths::Combine(XMLDir, CurrentAssetDir);
            }

            FString FullPath = FPaths::Combine(EffectiveMeshBase, MeshFile);

            if (!OutMeshAssets.Contains(MeshName))
            {
                UE_LOG(LogURLabEditor, Log, TEXT("[Mesh Map] Adding mesh: name='%s' -> file='%s'"), *MeshName, *FullPath);
                OutMeshAssets.Add(MeshName, FullPath);

                // 缩放：显式属性 > 默认类 > 主要默认值 > (1,1,1) 
                FVector Scale(1.0f);  // 3. 默认值
                FString ScaleStr = Node->GetAttribute(TEXT("scale"));  // 1. 显示 scale 属性
                if (!ScaleStr.IsEmpty())
                {
                    TArray<FString> Parts;
                    ScaleStr.ParseIntoArray(Parts, TEXT(" "), true);
                    if (Parts.Num() >= 3)
                    {
                        Scale.X = FCString::Atof(*Parts[0]);
                        Scale.Y = FCString::Atof(*Parts[1]);
                        Scale.Z = FCString::Atof(*Parts[2]);
                    }
                }
                else
                {
                    // 恢复到默认网格比例
                    FString MeshClass = Node->GetAttribute(TEXT("class"));  // 2. 默认类
                    if (MeshClass.IsEmpty()) MeshClass = TEXT("main");
                    if (DefaultMeshScales.Contains(MeshClass))
                    {
                        Scale = DefaultMeshScales[MeshClass];
                    }
                }
                OutMeshScales.Add(MeshName, Scale);

                UE_LOG(LogURLabEditor, Log, TEXT("Pure XML Found Mesh: %s -> %s (Scale: %s)"), *MeshName, *FullPath, *Scale.ToString());
            }
        }
    }
    // 将纹理 <texture> 内容解析为映射表
    else if (Tag.Equals(TEXT("texture")))
    {
        FString TexName = Node->GetAttribute(TEXT("name"));
        FString TexFile = Node->GetAttribute(TEXT("file"));
        if (TexFile.IsEmpty()) TexFile = TexName;

        if (!TexFile.IsEmpty())
        {
            if (TexName.IsEmpty()) TexName = FPaths::GetBaseFilename(TexFile);

            // 优先级：texturedir > assetdir > 当前目录（XMLDir）
            FString EffectiveTextureBase = XMLDir;  // 3. 当前目录
            if (!CurrentTextureDir.IsEmpty())
            {
                EffectiveTextureBase = FPaths::Combine(XMLDir, CurrentTextureDir);  // 1. 使用纹理目录
            }
            else if (!CurrentAssetDir.IsEmpty())
            {
                EffectiveTextureBase = FPaths::Combine(XMLDir, CurrentAssetDir);  // 2. 使用资产目录
            }

            FString FullPath = FPaths::Combine(EffectiveTextureBase, TexFile);

            if (!OutTextureAssets.Contains(TexName))
            {
                OutTextureAssets.Add(TexName, FullPath);
                UE_LOG(LogURLabEditor, Log, TEXT("Found Texture: %s -> %s"), *TexName, *FullPath);
            }
        }
    }
    // 将材质 <material> 内容解析为映射表 
    else if (Tag.Equals(TEXT("material")))
    {
        FString MatName = Node->GetAttribute(TEXT("name"));

        if (!MatName.IsEmpty() && !OutMaterialData.Contains(MatName))
        {
            FMuJoCoMaterialData MatData;

            // 解析 RGBA 颜色
            FString RgbaStr = Node->GetAttribute(TEXT("rgba"));
            if (!RgbaStr.IsEmpty())
            {
                TArray<FString> Parts;
                RgbaStr.ParseIntoArray(Parts, TEXT(" "), true);
                if (Parts.Num() >= 4)
                {
                    MatData.Rgba.R = FCString::Atof(*Parts[0]);
                    MatData.Rgba.G = FCString::Atof(*Parts[1]);
                    MatData.Rgba.B = FCString::Atof(*Parts[2]);
                    MatData.Rgba.A = FCString::Atof(*Parts[3]);
                }
            }

            // 解析纹理引用
            FString TexName = Node->GetAttribute(TEXT("texture"));
            if (!TexName.IsEmpty())
            {
                MatData.BaseColorTextureName = TexName;
            }

            // MuJoCo 通常不会在 XML 中显式地使用 normal/ORM，但我们支持它。
            FString NormalTex = Node->GetAttribute(TEXT("texnormal"));
            if (!NormalTex.IsEmpty())
            {
                MatData.NormalTextureName = NormalTex;
            }

            FString ORMTex = Node->GetAttribute(TEXT("texorm"));
            if (!ORMTex.IsEmpty())
            {
                MatData.ORMTextureName = ORMTex;
            }

            FString RoughnessTex = Node->GetAttribute(TEXT("texroughness"));
            if (!RoughnessTex.IsEmpty())
            {
                MatData.RoughnessTextureName = RoughnessTex;
            }

            FString MetallicTex = Node->GetAttribute(TEXT("texmetallic"));
            if (!MetallicTex.IsEmpty())
            {
                MatData.MetallicTextureName = MetallicTex;
            }

            OutMaterialData.Add(MatName, MatData);
            UE_LOG(LogURLabEditor, Log, TEXT("Found Material: %s (RGBA: %s, Texture: %s)"),
                *MatName, *MatData.Rgba.ToString(), *MatData.BaseColorTextureName);
        }
    }
    // 递归查找顶级容器（不包括上面已处理的标签，例如 include/asset）
    else if (Tag.Equals(TEXT("mujoco")))
    {
         for (const FXmlNode* Child : Node->GetChildrenNodes())
         {
             ParseAssetsRecursive(Child, XMLDir, OutMeshAssets, OutMeshScales, OutTextureAssets, OutMaterialData, CurrentMeshDir, CurrentTextureDir, CurrentAssetDir);
         }
    }
}

void UMujocoGenerationAction::ParseDefaultsRecursive(const FXmlNode* Node, UBlueprint* BP, USCS_Node* RootNode, const FString& XMLDir, const FMjCompilerSettings& CompilerSettings, const FString& ParentClassName, bool bIsDefaultContext)
{
    if (!Node || !BP || !RootNode) return;

    const FString Tag = Node->GetTag();

    // <include>
    if (Tag.Equals(TEXT("include")))
    {
        FString FileAttr = Node->GetAttribute(TEXT("file"));
        if (!FileAttr.IsEmpty())
        {
             FString IncludePath = FPaths::Combine(XMLDir, FileAttr);
             FXmlFile IncludedFile(IncludePath);
             if (IncludedFile.IsValid())
             {
                 ParseDefaultsRecursive(IncludedFile.GetRootNode(), BP, RootNode, FPaths::GetPath(IncludePath), CompilerSettings, ParentClassName, bIsDefaultContext);
             }
        }
    }
    // <default>
    else if (Tag.Equals(TEXT("default")))
    {
        FString ClassName = Node->GetAttribute(TEXT("class"));

        if (ClassName.IsEmpty()) ClassName = TEXT("main");

        FString NodeName = ClassName;
        UE_LOG(LogURLabEditor, Log, TEXT("Creating Default Component: %s (Parent: %s)"), *NodeName, *ParentClassName);

        USCS_Node* DefNode = BP->SimpleConstructionScript->CreateNode(UMjDefault::StaticClass(), *NodeName);
        RootNode->AddChildNode(DefNode);

        UMjDefault* DefComp = Cast<UMjDefault>(DefNode->ComponentTemplate);
        if (DefComp)
        {
            DefComp->ImportFromXml(Node);
            DefComp->ClassName = ClassName;
            DefComp->ParentClassName = ParentClassName;
            DefComp->bIsDefault = bIsDefaultContext;
        }

        // 缓存节点以供将来引用（可选，与 ProcessDefault 逻辑一致）
        CreatedDefaultNodes.Add(ClassName, DefNode);

        // 对嵌套标签（几何体、关节、执行器以及嵌套默认值）进行递归
        for (const FXmlNode* Child : Node->GetChildrenNodes())
        {
            FString ChildTag = Child->GetTag();

            // 递归处理嵌套的 <default>
            if (ChildTag.Equals(TEXT("default")))
            {
                // 将 DefNode 作为 RootNode 传递以建立层次结构
                ParseDefaultsRecursive(Child, BP, DefNode, XMLDir, CompilerSettings, ClassName, true);
            }
            // 处理子组件
            else if (ChildTag.Equals(TEXT("geom")))
            {
                FString GeomName = Child->GetAttribute(TEXT("name"));
                if (GeomName.IsEmpty()) GeomName = TEXT("DefaultGeom");

                USCS_Node* GeomNode = BP->SimpleConstructionScript->CreateNode(UMjGeom::StaticClass(), *GeomName);
                DefNode->AddChildNode(GeomNode);

                UMjGeom* GeomComp = Cast<UMjGeom>(GeomNode->ComponentTemplate);
                if (GeomComp)
                {
                    GeomComp->ImportFromXml(Child, CompilerSettings);
                    GeomComp->bIsDefault = true;
                }
            }
            else if (ChildTag.Equals(TEXT("joint")))
            {
                FString JointName = Child->GetAttribute(TEXT("name"));
                if (JointName.IsEmpty()) JointName = TEXT("DefaultJoint");

                USCS_Node* JointNode = BP->SimpleConstructionScript->CreateNode(UMjJoint::StaticClass(), *JointName);
                DefNode->AddChildNode(JointNode);

                UMjJoint* JointComp = Cast<UMjJoint>(JointNode->ComponentTemplate);
                if (JointComp)
                {
                    JointComp->ImportFromXml(Child, CompilerSettings);
                    JointComp->bIsDefault = true;
                    UE_LOG(LogURLabEditor, Log, TEXT("  - Found Default Joint: %s (Type Overridden: %s)"), *JointName, JointComp->bOverride_Type ? TEXT("True") : TEXT("False"));
                }
            }
            else if (ChildTag.Equals(TEXT("site")))
            {
                FString SiteName = Child->GetAttribute(TEXT("name"));
                if (SiteName.IsEmpty()) SiteName = TEXT("DefaultSite");

                USCS_Node* SiteNode = BP->SimpleConstructionScript->CreateNode(UMjSite::StaticClass(), *SiteName);
                DefNode->AddChildNode(SiteNode);

                UMjSite* SiteComp = Cast<UMjSite>(SiteNode->ComponentTemplate);
                if (SiteComp)
                {
                    SiteComp->ImportFromXml(Child, CompilerSettings);
                    SiteComp->bIsDefault = true;
                }
            }
            else if (ChildTag.Equals(TEXT("camera")))
            {
                FString CamName = Child->GetAttribute(TEXT("name"));
                if (CamName.IsEmpty()) CamName = TEXT("DefaultCamera");

                USCS_Node* CamNode = BP->SimpleConstructionScript->CreateNode(UMjCamera::StaticClass(), *CamName);
                DefNode->AddChildNode(CamNode);

                UMjCamera* CamComp = Cast<UMjCamera>(CamNode->ComponentTemplate);
                if (CamComp)
                {
                    CamComp->ImportFromXml(Child, CompilerSettings);
                    CamComp->bIsDefault = true;
                }
            }
            // Actuators
            else if (ChildTag.Equals(TEXT("motor")) || ChildTag.Equals(TEXT("position")) ||
                     ChildTag.Equals(TEXT("velocity")) || ChildTag.Equals(TEXT("cylinder")) ||
                     ChildTag.Equals(TEXT("muscle")) || ChildTag.Equals(TEXT("general")) ||
                     ChildTag.Equals(TEXT("damper")) || ChildTag.Equals(TEXT("actuator")) ||
                     ChildTag.Equals(TEXT("adhesion")) || ChildTag.Equals(TEXT("intvelocity")) ||
                     ChildTag.Equals(TEXT("dcmotor")))
            {
                 FString ActName = Child->GetAttribute(TEXT("name"));
                 if (ActName.IsEmpty())
                 {
                     FString DefaultActTag = ChildTag;
                     DefaultActTag[0] = FChar::ToUpper(DefaultActTag[0]);
                     ActName = TEXT("Default") + DefaultActTag;
                 }

                 UClass* ActClass = UMjActuator::StaticClass();
                 if (ChildTag == "motor") ActClass = UMjMotorActuator::StaticClass();
                 else if (ChildTag == "position") ActClass = UMjPositionActuator::StaticClass();
                 else if (ChildTag == "velocity") ActClass = UMjVelocityActuator::StaticClass();
                 else if (ChildTag == "muscle") ActClass = UMjMuscleActuator::StaticClass();
                 else if (ChildTag == "adhesion") ActClass = UMjAdhesionActuator::StaticClass();
                 else if (ChildTag == "intvelocity") ActClass = UMjIntVelocityActuator::StaticClass();
                 else if (ChildTag == "dcmotor") ActClass = UMjDcMotorActuator::StaticClass();

                 USCS_Node* ActNode = BP->SimpleConstructionScript->CreateNode(ActClass, *ActName);
                 DefNode->AddChildNode(ActNode);

                 UMjActuator* ActComp = Cast<UMjActuator>(ActNode->ComponentTemplate);
                 if (ActComp)
                 {
                     ActComp->ImportFromXml(Child);
                     ActComp->bIsDefault = true;
                 }
            }
        }
    }
    // Recurse through root/mujoco
    else if (Tag.Equals(TEXT("mujoco")))
    {
         for (const FXmlNode* Child : Node->GetChildrenNodes())
         {
             ParseDefaultsRecursive(Child, BP, RootNode, XMLDir, CompilerSettings, ParentClassName, bIsDefaultContext);
         }
    }
}

void UMujocoGenerationAction::ParseContactSection(const FXmlNode* Node, UBlueprint* BP, USCS_Node* RootNode, const FString& XMLDir)
{
    if (!Node || !BP || !RootNode) return;

    const FString Tag = Node->GetTag();

    // Handle <include> elements
    if (Tag.Equals(TEXT("include")))
    {
        FString FileAttr = Node->GetAttribute(TEXT("file"));
        if (!FileAttr.IsEmpty())
        {
            FString IncludePath = FPaths::Combine(XMLDir, FileAttr);
            FXmlFile IncludedFile(IncludePath);
            if (IncludedFile.IsValid())
            {
                ParseContactSection(IncludedFile.GetRootNode(), BP, RootNode, FPaths::GetPath(IncludePath));
            }
        }
    }
    // Handle <contact> section
    else if (Tag.Equals(TEXT("contact")))
    {
        UE_LOG(LogURLabEditor, Log, TEXT("Parsing <contact> section"));

        // Iterate through children to find <pair> and <exclude> elements
        for (const FXmlNode* Child : Node->GetChildrenNodes())
        {
            FString ChildTag = Child->GetTag();

            if (ChildTag.Equals(TEXT("pair")))
            {
                // Create UMjContactPair component
                FString Geom1 = Child->GetAttribute(TEXT("geom1"));
                FString Geom2 = Child->GetAttribute(TEXT("geom2"));
                FString PairName = Child->GetAttribute(TEXT("name"));

                if (PairName.IsEmpty())
                {
                    PairName = FString::Printf(TEXT("ContactPair_%s_%s"), *Geom1, *Geom2);
                }

                UE_LOG(LogURLabEditor, Log, TEXT("Creating Contact Pair: %s (geom1=%s, geom2=%s)"), *PairName, *Geom1, *Geom2);

                USCS_Node* PairNode = BP->SimpleConstructionScript->CreateNode(UMjContactPair::StaticClass(), *PairName);
                RootNode->AddChildNode(PairNode);

                UMjContactPair* PairComp = Cast<UMjContactPair>(PairNode->ComponentTemplate);
                if (PairComp)
                {
                    PairComp->ImportFromXml(Child);
                }
            }
            else if (ChildTag.Equals(TEXT("exclude")))
            {
                // Create UMjContactExclude component
                FString Body1 = Child->GetAttribute(TEXT("body1"));
                FString Body2 = Child->GetAttribute(TEXT("body2"));
                FString ExcludeName = Child->GetAttribute(TEXT("name"));

                if (ExcludeName.IsEmpty())
                {
                    ExcludeName = FString::Printf(TEXT("ContactExclude_%s_%s"), *Body1, *Body2);
                }

                UE_LOG(LogURLabEditor, Log, TEXT("Creating Contact Exclude: %s (body1=%s, body2=%s)"), *ExcludeName, *Body1, *Body2);

                USCS_Node* ExcludeNode = BP->SimpleConstructionScript->CreateNode(UMjContactExclude::StaticClass(), *ExcludeName);
                RootNode->AddChildNode(ExcludeNode);

                UMjContactExclude* ExcludeComp = Cast<UMjContactExclude>(ExcludeNode->ComponentTemplate);
                if (ExcludeComp)
                {
                    ExcludeComp->ImportFromXml(Child);
                }
            }
        }
    }
    // Recurse through root/mujoco to find <contact>
    else if (Tag.Equals(TEXT("mujoco")))
    {
        for (const FXmlNode* Child : Node->GetChildrenNodes())
        {
            ParseContactSection(Child, BP, RootNode, XMLDir);
        }
    }
}

void UMujocoGenerationAction::ParseEqualitySection(const FXmlNode* Node, UBlueprint* BP, USCS_Node* RootNode, const FString& XMLDir)
{
    if (!Node || !BP || !RootNode) return;
    const FString Tag = Node->GetTag();

    if (Tag.Equals(TEXT("include")))
    {
        FString FileAttr = Node->GetAttribute(TEXT("file"));
        if (!FileAttr.IsEmpty())
        {
            FString IncludePath = FPaths::Combine(XMLDir, FileAttr);
            FXmlFile IncludedFile(IncludePath);
            if (IncludedFile.IsValid())
            {
                ParseEqualitySection(IncludedFile.GetRootNode(), BP, RootNode, FPaths::GetPath(IncludePath));
            }
        }
    }
    else if (Tag.Equals(TEXT("equality")))
    {
        UE_LOG(LogURLabEditor, Log, TEXT("Parsing <equality> section"));
        for (const FXmlNode* Child : Node->GetChildrenNodes())
        {
            FString ChildTag = Child->GetTag();
            // Equality tags: connect, weld, joint, tendon, flex, flexvert, flexstrain
            if (ChildTag.Equals(TEXT("connect")) || ChildTag.Equals(TEXT("weld")) || ChildTag.Equals(TEXT("joint")) || ChildTag.Equals(TEXT("tendon"))
             || ChildTag.Equals(TEXT("flex")) || ChildTag.Equals(TEXT("flexvert")) || ChildTag.Equals(TEXT("flexstrain")))
            {
                FString EqName = Child->GetAttribute(TEXT("name"));
                if (EqName.IsEmpty())
                {
                    FString EqTag = ChildTag;
                    EqTag[0] = FChar::ToUpper(EqTag[0]);
                    EqName = TEXT("Eq_") + EqTag;
                }

                USCS_Node* EqNode = BP->SimpleConstructionScript->CreateNode(UMjEquality::StaticClass(), *EqName);
                RootNode->AddChildNode(EqNode);

                UMjEquality* EqComp = Cast<UMjEquality>(EqNode->ComponentTemplate);
                if (EqComp)
                {
                    EqComp->ImportFromXml(Child);
                    FString NameAttr = Child->GetAttribute(TEXT("name"));
                    if (!NameAttr.IsEmpty()) EqComp->MjName = NameAttr;
                }
            }
        }
    }
    else if (Tag.Equals(TEXT("mujoco")))
    {
        for (const FXmlNode* Child : Node->GetChildrenNodes())
        {
            ParseEqualitySection(Child, BP, RootNode, XMLDir);
        }
    }
}

void UMujocoGenerationAction::ParseKeyframeSection(const FXmlNode* Node, UBlueprint* BP, USCS_Node* RootNode, const FString& XMLDir)
{
    if (!Node || !BP || !RootNode) return;
    const FString Tag = Node->GetTag();

    if (Tag.Equals(TEXT("include")))
    {
        FString FileAttr = Node->GetAttribute(TEXT("file"));
        if (!FileAttr.IsEmpty())
        {
            FString IncludePath = FPaths::Combine(XMLDir, FileAttr);
            FXmlFile IncludedFile(IncludePath);
            if (IncludedFile.IsValid())
            {
                ParseKeyframeSection(IncludedFile.GetRootNode(), BP, RootNode, FPaths::GetPath(IncludePath));
            }
        }
    }
    else if (Tag.Equals(TEXT("keyframe")))
    {
        UE_LOG(LogURLabEditor, Log, TEXT("Parsing <keyframe> section"));
        for (const FXmlNode* Child : Node->GetChildrenNodes())
        {
            if (Child->GetTag().Equals(TEXT("key")))
            {
                FString KeyName = Child->GetAttribute(TEXT("name"));
                if (KeyName.IsEmpty()) KeyName = TEXT("Keyframe");

                USCS_Node* KeyNode = BP->SimpleConstructionScript->CreateNode(UMjKeyframe::StaticClass(), *KeyName);
                RootNode->AddChildNode(KeyNode);

                UMjKeyframe* KeyComp = Cast<UMjKeyframe>(KeyNode->ComponentTemplate);
                if (KeyComp)
                {
                    KeyComp->ImportFromXml(Child);
                    FString NameAttr = Child->GetAttribute(TEXT("name"));
                    if (!NameAttr.IsEmpty()) KeyComp->MjName = NameAttr;
                }
            }
        }
    }
    else if (Tag.Equals(TEXT("mujoco")))
    {
        for (const FXmlNode* Child : Node->GetChildrenNodes())
        {
            ParseKeyframeSection(Child, BP, RootNode, XMLDir);
        }
    }
}
