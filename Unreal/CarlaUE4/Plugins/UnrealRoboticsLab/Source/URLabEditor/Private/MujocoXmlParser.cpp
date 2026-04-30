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
                 // Iterate children of included root
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
        return; // Done with include node itself
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
    // <frame> applies a pos/quat offset to nested children and is dissolved at compile time.
    // We represent it as a UMjFrame SCS node whose children are attached to it.
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

        // Attach the frame node to its parent, then recurse its children onto it
        if (ParentNode) ParentNode->AddChildNode(CreatedNode);
        else BP->SimpleConstructionScript->GetDefaultSceneRootNode()->AddChildNode(CreatedNode);

        for (const FXmlNode* Child : Node->GetChildrenNodes())
        {
            ImportNodeRecursive(Child, CreatedNode, BP, XMLDir, AssetImportPath, MeshAssets, MeshScales,
                TextureAssets, MaterialData, ImportedTextures, CompilerSettings, bIsDefaultContext);
        }
        return; // Children already processed above
    }
    // --- GEOM ---
    else if (Tag.Equals(TEXT("geom")))
    {
        FString Name = Node->GetAttribute(TEXT("name"));
        FString TypeStr = Node->GetAttribute(TEXT("type"));
        FString MeshAttr = Node->GetAttribute(TEXT("mesh"));

        // If no explicit type but has a mesh attribute, it's a mesh geom
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

            // Preserve the MJCF 'name' so other components (tendons wrapping the
            // geom, contact pairs referencing it) can resolve by the original
            // name, not our auto-generated SCS variable name.
            FString NameAttr = Node->GetAttribute(TEXT("name"));
            if (!NameAttr.IsEmpty()) GeomComp->MjName = NameAttr;

            // Resolve DefaultClass from the class attribute
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

            // Resolve default class transform for visual mesh placement.
            // Walk the default class hierarchy (child -> parent -> ... -> main) to find
            // the first default geom with a transform override.
            // The MjGeom component itself is NOT modified (that would double-apply in ExportTo).
            // We only apply the offset to the visual StaticMeshComponent child.
            FTransform DefaultVisualOffset = FTransform::Identity;
            {
                FString SearchClassName = Node->GetAttribute(TEXT("class"));
                if (SearchClassName.IsEmpty()) SearchClassName = TEXT("main");

                bool bFoundRot = GeomComp->bOverride_Quat;
                bool bFoundPos = GeomComp->bOverride_Pos;

                // Walk up the default hierarchy
                while ((!bFoundRot || !bFoundPos) && !SearchClassName.IsEmpty())
                {
                    if (CreatedDefaultNodes.Contains(SearchClassName))
                    {
                        USCS_Node* DefNode = CreatedDefaultNodes[SearchClassName];
                        if (DefNode)
                        {
                            // Find the default geom component under this default class
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

                            // Walk up to parent class
                            UMjDefault* DefComp = Cast<UMjDefault>(DefNode->ComponentTemplate);
                            if (DefComp && !DefComp->ParentClassName.IsEmpty())
                            {
                                SearchClassName = DefComp->ParentClassName;
                            }
                            else
                            {
                                break; // No parent, stop
                            }
                        }
                        else
                        {
                            break;
                        }
                    }
                    else
                    {
                        break; // Class not found
                    }
                }
            }

            // Handle Mesh Visual
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

                                 // Check if parent Geom has Group=3 (collision/hidden)
                                 if (GeomComp && GeomComp->Group == 3)
                                 {
                                     MeshTemplate->SetVisibility(false);
                                     MeshTemplate->bHiddenInGame = true;
                                 }

                                 // Apply Scale if present
                                 if (MeshScales.Contains(MeshName))
                                 {
                                     FVector Scale = MeshScales[MeshName];
                                     if (!Scale.Equals(FVector::OneVector))
                                     {
                                         MeshTemplate->SetRelativeScale3D(Scale);
                                         UE_LOG(LogURLabEditor, Log, TEXT("Applying scale %s to mesh %s"), *Scale.ToString(), *MeshName);
                                     }
                                 }

                                 // Apply default class visual offset (e.g., visual_zflip rotation)
                                 // This only affects the visual mesh, NOT the MjGeom component,
                                 // so ExportTo won't double-apply the default.
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

                                 // Create and assign material instance
                                 FMuJoCoMaterialData MatData;
                                 MatData.Rgba = GeomComp->Rgba; // Use geom color as default

                                 // Key by material name (resolved through default chain) if referenced, else fall back to mesh name
                                 FString ResolvedMat = ResolveMaterialFromDefaults(GeomComp, BP);
                                 FString MaterialKey = MeshName;
                                 if (!ResolvedMat.IsEmpty() && MaterialData.Contains(ResolvedMat))
                                 {
                                     MatData = MaterialData[ResolvedMat];
                                     MaterialKey = ResolvedMat;
                                     UE_LOG(LogURLabEditor, Log, TEXT("Using shared material '%s' for mesh '%s'"), *ResolvedMat, *MeshName);
                                 }

                                 // Create or reuse material instance
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
            // Handle Primitive Visuals (Built-in)
            else if (UStaticMeshComponent* BuiltInViz = GeomComp->GetVisualizerMesh())
            {
                 UE_LOG(LogURLabEditor, Log, TEXT("Applying visual properties to built-in visualizer for '%s'"), *Name);

                 // Create and assign material instance
                 FMuJoCoMaterialData MatData;
                 MatData.Rgba = GeomComp->Rgba; // Use geom color as default

                 // Key by material name (resolved through default chain) if referenced, else fall back to geom name
                 FString ResolvedMat = ResolveMaterialFromDefaults(GeomComp, BP);
                 FString MaterialKey = Name;
                 if (!ResolvedMat.IsEmpty() && MaterialData.Contains(ResolvedMat))
                 {
                     MatData = MaterialData[ResolvedMat];
                     MaterialKey = ResolvedMat;
                 }

                 // Create or reuse material instance
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
    // --- JOINT ---
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

            // Preserve the MJCF 'name' so actuators / equality / tendons that
            // reference this joint by name continue to resolve after SCS
            // uniqueness disambiguates the UE variable name (e.g. a Default
            // class "waist" already owning the SCS name "waist" forces the
            // joint's variable name to "waist1"). Without this, the joint's
            // spec name would be the disambiguated UE name and the actuator's
            // joint="waist" reference would fail at compile.
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
            // Note: Standalone <freejoint/> has no attributes.
            // We skip ImportFromXml to avoid any unwanted movement or axis overrides.
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

            // For mesh type, import the mesh file and create a child UStaticMeshComponent
            FString FlexMeshFile = Node->GetAttribute(TEXT("file"));
            if (FlexComp->Type == EMjFlexcompType::Mesh && !FlexMeshFile.IsEmpty())
            {
                FString MeshName = FPaths::GetBaseFilename(FlexMeshFile);

                // Flexcomp references the mesh file directly (not via <asset><mesh>).
                // The Python preprocessor converts it to GLB alongside the original.
                // Try GLB first (preprocessed), then fall back to raw file.
                FString MeshFilePath;
                if (MeshAssets.Contains(MeshName))
                {
                    MeshFilePath = MeshAssets[MeshName];
                }
                else
                {
                    // Try GLB in meshdir, then XMLDir
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
                            // Fall back to original file
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
    // --- SITE ---
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
    // --- SENSOR ---
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
    // --- CAMERA ---
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
    // --- ACTUATOR ---
    else if (Tag.Equals(TEXT("actuator")))
    {
         // The <actuator> container itself is just a wrapper — recurse into each
         // per-type child (motor, muscle, position, etc.) to create components.
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
    // --- TENDON ---
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

    // --- ATTACH & RECURSE ---
    if (CreatedNode)
    {
        if (ParentNode) ParentNode->AddChildNode(CreatedNode);
        else BP->SimpleConstructionScript->GetDefaultSceneRootNode()->AddChildNode(CreatedNode); // Fallback attach to root

        // Recurse for Children
        if (Tag.Equals(TEXT("body")) || Tag.Equals(TEXT("worldbody")))
        {
            for (const FXmlNode* Child : Node->GetChildrenNodes())
            {
                // If CreatedNode is NULL (e.g. reused root), use ParentNode? No, CreatedNode is the parent for children.
                // If we reused root, CreatedNode is the root.
                ImportNodeRecursive(Child, CreatedNode, BP, XMLDir, AssetImportPath, MeshAssets, MeshScales, TextureAssets, MaterialData, ImportedTextures, CompilerSettings, bIsDefaultContext);
            }
        }
    }
}

void UMujocoGenerationAction::CollectDefaultMeshScales(const FXmlNode* Node, const FString& CurrentClass)
{
    if (!Node) return;
    const FString Tag = Node->GetTag();

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
                        DefaultMeshScales.Add(ClassName, Scale);
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
            CollectDefaultMeshScales(Child, CurrentClass);
        }
    }
}

void UMujocoGenerationAction::ParseAssetsRecursive(const FXmlNode* Node, const FString& XMLDir, TMap<FString, FString>& OutMeshAssets, TMap<FString, FVector>& OutMeshScales, TMap<FString, FString>& OutTextureAssets, TMap<FString, FMuJoCoMaterialData>& OutMaterialData, const FString& MeshDir, const FString& TextureDir, const FString& AssetDir)
{
    if (!Node) return;
    const FString Tag = Node->GetTag();

    // Directory overrides for this context (current or inherited)
    FString CurrentMeshDir = MeshDir;
    FString CurrentTextureDir = TextureDir;
    FString CurrentAssetDir = AssetDir;

    // If this is a container, look for compiler tag among immediate children to set directory overrides for all siblings
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
                break; // Assume only one compiler tag per section
            }
        }
    }

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
                 ParseAssetsRecursive(IncludedFile.GetRootNode(), FPaths::GetPath(IncludePath), OutMeshAssets, OutMeshScales, OutTextureAssets, OutMaterialData, CurrentMeshDir, CurrentTextureDir, CurrentAssetDir);
             }
        }
    }
    // <asset>
    else if (Tag.Equals(TEXT("asset")))
    {
        for (const FXmlNode* Child : Node->GetChildrenNodes())
        {
            ParseAssetsRecursive(Child, XMLDir, OutMeshAssets, OutMeshScales, OutTextureAssets, OutMaterialData, CurrentMeshDir, CurrentTextureDir, CurrentAssetDir);
        }
    }
    // <mesh>
    else if (Tag.Equals(TEXT("mesh")))
    {
        FString MeshName = Node->GetAttribute(TEXT("name"));
        FString MeshFile = Node->GetAttribute(TEXT("file"));
        if (MeshFile.IsEmpty()) MeshFile = MeshName;

        if (!MeshFile.IsEmpty())
        {
            if (MeshName.IsEmpty()) MeshName = FPaths::GetBaseFilename(MeshFile);

            // Priority: meshdir > assetdir > current directory (XMLDir)
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

                // Scale: explicit attribute > default class > main default > (1,1,1)
                FVector Scale(1.0f);
                FString ScaleStr = Node->GetAttribute(TEXT("scale"));
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
                    // Fall back to default mesh scale
                    FString MeshClass = Node->GetAttribute(TEXT("class"));
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
    // <texture>
    else if (Tag.Equals(TEXT("texture")))
    {
        FString TexName = Node->GetAttribute(TEXT("name"));
        FString TexFile = Node->GetAttribute(TEXT("file"));
        if (TexFile.IsEmpty()) TexFile = TexName;

        if (!TexFile.IsEmpty())
        {
            if (TexName.IsEmpty()) TexName = FPaths::GetBaseFilename(TexFile);

            // Priority: texturedir > assetdir > current directory (XMLDir)
            FString EffectiveTextureBase = XMLDir;
            if (!CurrentTextureDir.IsEmpty())
            {
                EffectiveTextureBase = FPaths::Combine(XMLDir, CurrentTextureDir);
            }
            else if (!CurrentAssetDir.IsEmpty())
            {
                EffectiveTextureBase = FPaths::Combine(XMLDir, CurrentAssetDir);
            }

            FString FullPath = FPaths::Combine(EffectiveTextureBase, TexFile);

            if (!OutTextureAssets.Contains(TexName))
            {
                OutTextureAssets.Add(TexName, FullPath);
                UE_LOG(LogURLabEditor, Log, TEXT("Found Texture: %s -> %s"), *TexName, *FullPath);
            }
        }
    }
    // <material>
    else if (Tag.Equals(TEXT("material")))
    {
        FString MatName = Node->GetAttribute(TEXT("name"));

        if (!MatName.IsEmpty() && !OutMaterialData.Contains(MatName))
        {
            FMuJoCoMaterialData MatData;

            // Parse RGBA color
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

            // Parse texture references
            FString TexName = Node->GetAttribute(TEXT("texture"));
            if (!TexName.IsEmpty())
            {
                MatData.BaseColorTextureName = TexName;
            }

            // MuJoCo doesn't have explicit normal/ORM in XML typically, but we support it
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
    // Recurse for top-level containers (excluding tags handled above like include/asset)
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

        // Cache the node for future reference (optional, matches ProcessDefault logic)
        CreatedDefaultNodes.Add(ClassName, DefNode);

        // Recurse for nested tags (geoms, joints, actuators, AND nested defaults)
        for (const FXmlNode* Child : Node->GetChildrenNodes())
        {
            FString ChildTag = Child->GetTag();

            // Recurse for nested <default>
            if (ChildTag.Equals(TEXT("default")))
            {
                // Pass DefNode as RootNode to establish hierarchy
                ParseDefaultsRecursive(Child, BP, DefNode, XMLDir, CompilerSettings, ClassName, true);
            }
            // Handle Child Components
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
