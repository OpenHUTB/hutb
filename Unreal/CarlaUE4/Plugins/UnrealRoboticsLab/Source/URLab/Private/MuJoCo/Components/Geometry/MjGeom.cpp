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

#include "MuJoCo/Components/Geometry/MjGeom.h"
#include "Components/StaticMeshComponent.h"
#include "MuJoCo/Utils/MjXmlUtils.h"
#include "MuJoCo/Utils/MjUtils.h"
#include "MuJoCo/Utils/MjOrientationUtils.h"
#include "XmlNode.h"
#include "Utils/URLabLogging.h"
#include "MuJoCo/Components/Defaults/MjDefault.h"
#include "MuJoCo/Components/Defaults/MujocoDefaults.h"
#include "MuJoCo/Core/Spec/MjSpecWrapper.h"
#include "Utils/IO.h"
#include "Utils/MeshUtils.h"
#include "Misc/ScopedSlowTask.h"
#include "Chaos/TriangleMeshImplicitObject.h"
#include "PhysicsEngine/BodySetup.h"

#if WITH_EDITOR
#include "Engine/Blueprint.h"
#include "Engine/SCS_Node.h"
#include "Engine/SimpleConstructionScript.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "AssetToolsModule.h"
#include "IAssetTools.h"
#include "AutomatedAssetImportData.h"
#endif


UMjGeom::UMjGeom()
{
	PrimaryComponentTick.bCanEverTick = false;
    
    // Default collision settings (matching MuJoCo defaults)
    Contype = MujocoDefaults::Geom::Contype;
    Conaffinity = MujocoDefaults::Geom::Conaffinity;
    bOverride_Pos = false;
    bOverride_Quat = false;
}








void UMjGeom::ImportFromXml(const FXmlNode* Node)
{
    FMjCompilerSettings DefaultSettings;
    ImportFromXml(Node, DefaultSettings);
}

void UMjGeom::ImportFromXml(const FXmlNode* Node, const FMjCompilerSettings& CompilerSettings)
{
    if (!Node) return;


    // Physics
    MjXmlUtils::ReadAttrFloatArray(Node, TEXT("friction"), Friction, bOverride_Friction);
    MjXmlUtils::ReadAttrFloatArray(Node, TEXT("solref"), SolRef, bOverride_SolRef);
    MjXmlUtils::ReadAttrFloatArray(Node, TEXT("solimp"), SolImp, bOverride_SolImp);
    MjXmlUtils::ReadAttrFloat(Node, TEXT("density"), Density, bOverride_Density);
    MjXmlUtils::ReadAttrFloat(Node, TEXT("mass"), Mass, bOverride_Mass);
    MjXmlUtils::ReadAttrFloat(Node, TEXT("margin"), Margin, bOverride_Margin);
    MjXmlUtils::ReadAttrFloat(Node, TEXT("gap"), Gap, bOverride_Gap);
    MjXmlUtils::ReadAttrInt(Node, TEXT("condim"), Condim, bOverride_Condim);
    MjXmlUtils::ReadAttrInt(Node, TEXT("contype"), Contype, bOverride_Contype);
    MjXmlUtils::ReadAttrInt(Node, TEXT("conaffinity"), Conaffinity, bOverride_Conaffinity);
    MjXmlUtils::ReadAttrInt(Node, TEXT("priority"), Priority, bOverride_Priority);
    MjXmlUtils::ReadAttrInt(Node, TEXT("group"), Group, bOverride_Group);

    // Visuals
    TArray<float> RgbaParts;
    if (MjXmlUtils::ReadAttrFloatArray(Node, TEXT("rgba"), RgbaParts, bOverride_Rgba))
    {
        if (RgbaParts.Num() >= 4)
            Rgba = FLinearColor(RgbaParts[0], RgbaParts[1], RgbaParts[2], RgbaParts[3]);
    }
    
    // Type & Size
    FString TypeStr = Node->GetAttribute(TEXT("type"));
    bOverride_Type = !TypeStr.IsEmpty();
    if (bOverride_Type)
    {
        if (TypeStr == "plane") Type = EMjGeomType::Plane;
        else if (TypeStr == "hfield") Type = EMjGeomType::Hfield;
        else if (TypeStr == "sphere") Type = EMjGeomType::Sphere;
        else if (TypeStr == "capsule") Type = EMjGeomType::Capsule;
        else if (TypeStr == "ellipsoid") Type = EMjGeomType::Ellipsoid;
        else if (TypeStr == "cylinder") Type = EMjGeomType::Cylinder;
        else if (TypeStr == "box") Type = EMjGeomType::Box;
        else if (TypeStr == "mesh") Type = EMjGeomType::Mesh;
        else if (TypeStr == "sdf") Type = EMjGeomType::SDF;
    }
    
    // Class Name
    MjXmlUtils::ReadAttrString(Node, TEXT("class"), MjClassName);

    // Mesh Name & Implicit Detection
    FString MeshAttr;
    if (MjXmlUtils::ReadAttrString(Node, TEXT("mesh"), MeshAttr))
    {
        MeshName = MeshAttr;
        
        // If Type wasn't explicitly set in the XML, but we have a mesh attribute, assume Mesh
        if (!bOverride_Type)
        {
            Type = EMjGeomType::Mesh;
            bOverride_Type = true;
            UE_LOG(LogURLabImport, Log, TEXT("[MjGeom::ImportFromXml] '%s' -> Implicitly setting Type to Mesh due to mesh attribute."), *GetName());
        }
    }
    
    TArray<float> SizeParts;
    if (MjXmlUtils::ReadAttrFloatArray(Node, TEXT("size"), SizeParts, bOverride_Size))
    {
        SizeParamsCount = SizeParts.Num();
        // Populate Size directly from the parsed parts so 1- and 2-element
        // size strings (sphere, capsule/cylinder) are stored correctly.
        // ParseVector requires 3 values and returns zero for shorter strings.
        Size = FVector::ZeroVector;
        if (SizeParts.Num() >= 1) Size.X = SizeParts[0];
        if (SizeParts.Num() >= 2) Size.Y = SizeParts[1];
        if (SizeParts.Num() >= 3) Size.Z = SizeParts[2];
    }
    
    // Support for "fromto" (overrides pos/quat/size)
    FString FromToStr = Node->GetAttribute(TEXT("fromto"));
    
    UE_LOG(LogURLabImport, Verbose, TEXT("[MjGeom::ImportFromXml] '%s' TypeStr='%s' bOverride_Type=%s bOverride_Size=%s Size=%s"),
        *GetName(), *TypeStr, bOverride_Type ? TEXT("TRUE") : TEXT("FALSE"),
        bOverride_Size ? TEXT("TRUE") : TEXT("FALSE"), *Size.ToString());
    
    if (!FromToStr.IsEmpty())
    {
         UE_LOG(LogURLabImport, Verbose, TEXT("  - FromToStr: '%s' (Resolving to pos/quat/size)"), *FromToStr);

         if (MjUtils::ParseFromTo(FromToStr, FromToStart, FromToEnd))
         {
             // Resolve fromto into explicit pos, quat, size — same decomposition MuJoCo does internally.
             FVector Midpoint = (FromToStart + FromToEnd) * 0.5f;
             SetRelativeLocation(Midpoint);
             bOverride_Pos = true;

             FVector Dir = (FromToEnd - FromToStart).GetSafeNormal();
             if (!Dir.IsNearlyZero())
             {
                 FQuat Rot = FQuat::FindBetweenNormals(FVector(0.f, 0.f, 1.f), Dir);
                 SetRelativeRotation(Rot);
             }
             bOverride_Quat = true;

             float HalfLength = (FromToEnd - FromToStart).Size() * 0.5f / 100.0f;

             if (Type == EMjGeomType::Box || Type == EMjGeomType::Ellipsoid)
             {
                 Size.Z = HalfLength;
                 SizeParamsCount = FMath::Max(SizeParamsCount, 3);
             }
             else
             {
                 Size.Y = HalfLength;
                 SizeParamsCount = FMath::Max(SizeParamsCount, 2);
             }
             // Only mark size as overridden if the element also had an explicit size attr.
             // The half-length from fromto is stored in Size.Y/Z but the radius (Size.X)
             // may come from a parent default — don't clobber it by writing size[0]=0.
             bFromToResolvedHalfLength = true;
             bOverride_FromTo = false;

             UE_LOG(LogURLabImport, Verbose, TEXT("[MjGeom] '%s' FromTo resolved: pos=%s, halfLen=%.4f, size=(%.4f, %.4f, %.4f)"),
                *GetName(), *Midpoint.ToString(), HalfLength, Size.X, Size.Y, Size.Z);
         }
    }
    
    else
    {
        FString PosStr = Node->GetAttribute(TEXT("pos"));
        bOverride_Pos = !PosStr.IsEmpty();
        if (bOverride_Pos)
        {
            FVector MjPos = MjXmlUtils::ParseVector(PosStr);
            double p[3] = { (double)MjPos.X, (double)MjPos.Y, (double)MjPos.Z };
            SetRelativeLocation(MjUtils::MjToUEPosition(p));
        }
        else
        {
            SetRelativeLocation(FVector::ZeroVector);
            bOverride_Pos = false;
        }

        // Orientation (quat, axisangle, euler, xyaxes, zaxis — priority order)
        double MjQuat[4];
        bOverride_Quat = MjOrientationUtils::OrientationToMjQuat(Node, CompilerSettings, MjQuat);
        if (bOverride_Quat)
        {
            SetRelativeRotation(MjUtils::MjToUERotation(MjQuat));
        }
        else
        {
            bOverride_Quat = false;
            SetRelativeRotation(FQuat::Identity);
        }
    }

    // Name
    FString NameStr = Node->GetAttribute(TEXT("name"));
    if (!NameStr.IsEmpty())
    {
         UE_LOG(LogURLabImport, Verbose, TEXT("[MjGeom] Parsed name: %s"), *NameStr);
    }

    if (MjXmlUtils::ReadAttrString(Node, TEXT("material"), MaterialName))
    {
        UE_LOG(LogURLabImport, Log, TEXT("[MjGeom XML Import] '%s' -> Material: '%s'"), *GetName(), *MaterialName);
    }

    UE_LOG(LogURLabImport, Log, TEXT("[MjGeom XML Import] '%s' -> Contype: %s (%d), Conaffinity: %s (%d)"), 
        *GetName(), 
        bOverride_Contype ? TEXT("Set") : TEXT("Inherited"), Contype,
        bOverride_Conaffinity ? TEXT("Set") : TEXT("Inherited"), Conaffinity);

    // Mark this geom as having been produced by XML import.
    // User-authored components (bWasImported=false) use ShouldOverrideSize() to always
    // export their UE scale, even when bOverride_Size was never set.
    bWasImported = true;
}

void UMjGeom::ExportTo(mjsGeom* Geom, mjsDefault* Default)
{
    if (!Geom) return;


    if (!bOverride_Type && !MeshName.IsEmpty())
    {
        Type = EMjGeomType::Mesh;
        bOverride_Type = true;
    }

    int mjtType = mjGEOM_MESH;
    switch(Type)
    {
        case EMjGeomType::Plane:    mjtType = mjGEOM_PLANE;    break;
        case EMjGeomType::Hfield:   mjtType = mjGEOM_HFIELD;   break;
        case EMjGeomType::Sphere:   mjtType = mjGEOM_SPHERE;   break;
        case EMjGeomType::Capsule:  mjtType = mjGEOM_CAPSULE;  break;
        case EMjGeomType::Ellipsoid:mjtType = mjGEOM_ELLIPSOID;break;
        case EMjGeomType::Cylinder: mjtType = mjGEOM_CYLINDER; break;
        case EMjGeomType::Box:      mjtType = mjGEOM_BOX;      break;
        case EMjGeomType::Mesh:     mjtType = mjGEOM_MESH;     break;
        case EMjGeomType::SDF:      mjtType = mjGEOM_SDF;      break;
    }

    if (bOverride_Type) Geom->type = (mjtGeom)mjtType;
    
    bool bHasDefault = (Default != nullptr);
    int DefaultTypeInt = bHasDefault ? Default->geom->type : -1;
    int FinalType = bOverride_Type ? mjtType : (bHasDefault ? DefaultTypeInt : mjtType);
    
    if (FinalType == mjGEOM_MESH && !MeshName.IsEmpty())
    {
        mjs_setString(Geom->meshname, TCHAR_TO_UTF8(*MeshName));
    }

    if (bOverride_FromTo)
    {
        // Legacy path: if someone manually set fromto in the editor (not from import),
        // pass it through to MuJoCo raw.
        double Start[3], End[3];
        MjUtils::UEToMjPosition(FromToStart, Start);
        MjUtils::UEToMjPosition(FromToEnd, End);

        Geom->fromto[0] = Start[0]; Geom->fromto[1] = Start[1]; Geom->fromto[2] = Start[2];
        Geom->fromto[3] = End[0];   Geom->fromto[4] = End[1];   Geom->fromto[5] = End[2];
        UE_LOG(LogURLabBind, Log, TEXT("[MjGeom] Geom '%s' using raw fromto (manual override)"), *GetName());
    }
    else
    {
        FVector Location = GetRelativeLocation();
        if (!Location.IsZero())
        {
            double pos[3] = { 0, 0, 0 };
            MjUtils::UEToMjPosition(Location, pos);
            Geom->pos[0] = pos[0];
            Geom->pos[1] = pos[1];
            Geom->pos[2] = pos[2];
        }

        FQuat Quat = GetRelativeRotation().Quaternion();
        if (!Quat.IsIdentity())
        {
            double quat[4] = { 1, 0, 0, 0 };
            MjUtils::UEToMjRotation(Quat, quat);
            Geom->quat[0] = quat[0];
            Geom->quat[1] = quat[1];
            Geom->quat[2] = quat[2];
            Geom->quat[3] = quat[3];
        }
    }

    if (bFromToResolvedHalfLength && !bOverride_Size)
    {
        // FromTo was resolved: only write the half-length slot
        if (Type == EMjGeomType::Box || Type == EMjGeomType::Ellipsoid)
        {
            Geom->size[2] = Size.Z;
        }
        else
        {
            Geom->size[1] = Size.Y;
        }
    }
    else if (bOverride_Size)
    {
        if (SizeParamsCount >= 1) Geom->size[0] = Size.X;
        if (SizeParamsCount >= 2) Geom->size[1] = Size.Y;
        if (SizeParamsCount >= 3) Geom->size[2] = Size.Z;

        if (SizeParamsCount == 0)
        {
            Geom->size[0] = Size.X;
            Geom->size[1] = Size.Y;
            Geom->size[2] = Size.Z;
        }
    }


    if (bOverride_Friction)
    {
        for (int i = 0; i < Friction.Num() && i < 3; ++i)
        {
            Geom->friction[i] = Friction[i];
        }
    }

    if (bOverride_SolRef)
    {
        for (int i = 0; i < SolRef.Num() && i < 2; ++i)
        {
            Geom->solref[i] = SolRef[i];
        }
    }

    if (bOverride_SolImp)
    {
        for (int i = 0; i < SolImp.Num() && i < 5; ++i)
        {
            Geom->solimp[i] = SolImp[i];
        }
    }

    if (bOverride_Density)
        Geom->density = Density;

    if (bOverride_Mass)
        Geom->mass = Mass;
    
    if (bOverride_Margin)
        Geom->margin = Margin;
    
    if (bOverride_Gap)
        Geom->gap = Gap;

    if (bOverride_Condim)
        Geom->condim = Condim;

    if (bOverride_Contype)
        Geom->contype = Contype;
        
    if (bOverride_Conaffinity)
        Geom->conaffinity = Conaffinity;

    if (bOverride_Priority)
        Geom->priority = Priority;
        
    if (bOverride_Group)
        Geom->group = Group;

    if (bOverride_Rgba)
    {
        Geom->rgba[0] = Rgba.R;
        Geom->rgba[1] = Rgba.G;
        Geom->rgba[2] = Rgba.B;
        Geom->rgba[3] = Rgba.A;
    }
}

void UMjGeom::Bind(mjModel* Model, mjData* Data, const FString& Prefix)
{
    Super::Bind(Model, Data, Prefix);
    m_GeomView = BindToView<GeomView>(Prefix);

    if (m_GeomView.id != -1)
    {
        m_ID = m_GeomView.id;
        SyncUnrealTransformFromMj();
        UE_LOG(LogURLabBind, Log, TEXT("[MjGeom] Successfully bound '%s' to ID %d (MjName: %s)"), *GetName(), m_ID, *MjName);
    }
    else
    {
        UE_LOG(LogURLabBind, Warning, TEXT("[MjGeom] Geom '%s' FAILED bind. Prefix: %s, MjName: %s"), 
            *GetName(), *Prefix, *MjName);
    }
}

void UMjGeom::UpdateGlobalTransform()
{
    if (m_GeomView._m && m_GeomView._d && m_GeomView.id >= 0)
    {
        // Get world position from MuJoCo
        FVector WorldPos = MjUtils::MjToUEPosition(m_GeomView.xpos);
        FQuat WorldRot;
        mjtNum quat[4];
        mju_mat2Quat(quat, m_GeomView.xmat);
        WorldRot = MjUtils::MjToUERotation(quat);
        
        SetWorldLocation(WorldPos);
        SetWorldRotation(WorldRot);
    }
}

FVector UMjGeom::GetWorldLocation() const
{
    if (m_GeomView._m && m_GeomView._d && m_GeomView.id >= 0)
    {
        return MjUtils::MjToUEPosition(m_GeomView.xpos);
    }
    return GetComponentLocation();
}
void UMjGeom::SetFriction(float NewFriction)
{
    if (Friction.Num() == 0) Friction.Add(NewFriction);
    else Friction[0] = NewFriction;
    bOverride_Friction = true;
    
    // Update runtime model if bound
    if (m_GeomView._m && m_GeomView._d && m_GeomView.id >= 0 && m_GeomView.friction)
    {
        m_GeomView.friction[0] = NewFriction;
    }
}

void UMjGeom::SyncUnrealTransformFromMj()
{
    // Base implementation does nothing, specialized in primitive subtypes.
}

FString UMjGeom::GetResolvedMaterialName() const
{
    // Explicit material on this geom wins
    if (!MaterialName.IsEmpty()) return MaterialName;

    // Walk the default class chain
    if (UMjDefault* Def = FindEditorDefault())
    {
        TSet<FString> Visited;
        UMjDefault* Cur = Def;
        while (Cur && !Visited.Contains(Cur->ClassName))
        {
            Visited.Add(Cur->ClassName);
            if (UMjGeom* G = Cur->FindChildOfType<UMjGeom>())
            {
                if (!G->MaterialName.IsEmpty()) return G->MaterialName;
            }
            Cur = Cur->ParentClassName.IsEmpty() ? nullptr
                : FindDefaultByClassName(GetOwner(), Cur->ParentClassName);
        }
    }

    return FString(); // No material found
}

#if WITH_EDITOR
void UMjGeom::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
    Super::PostEditChangeProperty(PropertyChangedEvent);

    FName PropertyName = (PropertyChangedEvent.Property != nullptr) ? PropertyChangedEvent.Property->GetFName() : NAME_None;
    FName MemberPropertyName = (PropertyChangedEvent.MemberProperty != nullptr) ? PropertyChangedEvent.MemberProperty->GetFName() : NAME_None;

    // If the user manually changes the scale in the editor, mark it as an override.
    if (PropertyName == FName(TEXT("RelativeScale3D")) || MemberPropertyName == FName(TEXT("RelativeScale3D")))
    {
        // Don't auto-override if the scale hasn't actually changed meaningfully
        if (!GetRelativeScale3D().Equals(FVector(1.0f)))
        {
            bOverride_Size = true;
            UE_LOG(LogURLab, Log, TEXT("[MjGeom] User manually changed Scale for '%s'. Marking bOverride_Size = true."), *GetName());
        }
    }

    // Sync MjClassName when DefaultClass changes
    if (PropertyName == GET_MEMBER_NAME_CHECKED(UMjGeom, DefaultClass))
    {
        if (DefaultClass)
            MjClassName = DefaultClass->ClassName;
        else
            MjClassName.Empty();
    }

    // Apply OverrideMaterial to the visual mesh
    if (PropertyName == GET_MEMBER_NAME_CHECKED(UMjGeom, OverrideMaterial) ||
        MemberPropertyName == GET_MEMBER_NAME_CHECKED(UMjGeom, OverrideMaterial))
    {
        ApplyOverrideMaterial(OverrideMaterial);
    }
}
#endif

void UMjGeom::ApplyOverrideMaterial(UMaterialInterface* Material)
{
    // Base implementation is a no-op. Mesh geoms should not have their imported
    // materials overwritten — they already have materials from the import pipeline.
    // Primitive subclasses (Box, Sphere, Cylinder) override this with direct
    // VisualizerMesh access.
}

void UMjGeom::RegisterToSpec(FMujocoSpecWrapper& Wrapper, mjsBody* ParentBody)
{
    if (!ParentBody) return;
    if (bDisabledByDecomposition) return; // Decomposed source geom — hull sub-geoms register instead

    // When MjClassName is empty, pass nullptr so MuJoCo applies the parent body's
    // childclass default automatically (MuJoCo 3.x spec API behaviour).
    // Only resolve an explicit default when a class name was explicitly set.
    mjsDefault* SpecDef = nullptr;
    if (!MjClassName.IsEmpty())
    {
        SpecDef = mjs_findDefault(Wrapper.Spec, TCHAR_TO_UTF8(*MjClassName));
        if (!SpecDef) SpecDef = mjs_getSpecDefault(Wrapper.Spec);
    }

    mjsGeom* geom = mjs_addGeom(ParentBody, SpecDef);
    m_SpecElement = geom->element;

    FString NameToRegister = MjName.IsEmpty() ? GetName() : MjName;
    FString UniqueName = Wrapper.GetUniqueName(NameToRegister, mjOBJ_GEOM, GetOwner());
    mjs_setName(geom->element, TCHAR_TO_UTF8(*UniqueName));
    MjName = UniqueName;

    TArray<FString> ExtraMeshNames;

    if (Type == EMjGeomType::Mesh)
    {
        if (bIsDecomposedHull && !MeshName.IsEmpty())
        {
            // Hull sub-geom: OBJ files already exist from editor decomposition.
            // MeshName format: "{AssetName}_{index}" → file: "Complex_{AssetName}_sub_{index}.obj"
            int32 LastUnderscore;
            FString BaseAssetName = MeshName;
            FString IndexStr = TEXT("0");
            if (MeshName.FindLastChar('_', LastUnderscore))
            {
                BaseAssetName = MeshName.Left(LastUnderscore);
                IndexStr = MeshName.Mid(LastUnderscore + 1);
            }

            FString SubDir = Wrapper.MeshCacheSubDir.IsEmpty() ? TEXT("Shared") : Wrapper.MeshCacheSubDir;
            FString ObjPath = FString::Printf(TEXT("%s/URLab/ConvertedMeshes/%s/Complex_%s_sub_%s.obj"),
                *FPaths::ProjectSavedDir(), *SubDir, *BaseAssetName, *IndexStr);
            FString ObjFullPath = FPaths::ConvertRelativePathToFull(ObjPath);

            if (FPaths::FileExists(ObjFullPath))
            {
                Wrapper.AddMeshAsset(MeshName, ObjFullPath, FVector::OneVector);
            }
            else
            {
                // Fallback: export the child visualization SMC as a simple convex mesh.
                // This re-exports the already-decomposed hull geometry, NOT re-running CoACD.
                UE_LOG(LogURLab, Warning, TEXT("[MjGeom] Hull OBJ not found at '%s'. Re-exporting from child SMC."), *ObjFullPath);
                TArray<USceneComponent*> FallbackChildren;
                GetChildrenComponents(true, FallbackChildren);
                for (USceneComponent* Child : FallbackChildren)
                {
                    if (UStaticMeshComponent* SMC = Cast<UStaticMeshComponent>(Child))
                    {
                        TArray<FString> Names = Wrapper.PrepareMeshForMuJoCo(SMC, false);
                        if (Names.Num() > 0)
                        {
                            MeshName = Names[0];
                        }
                        break;
                    }
                }
            }
        }
        else
        {
            // Normal geom: find child SMC and prepare mesh (may run CoACD for complex)
            TArray<USceneComponent*> Children;
            GetChildrenComponents(true, Children);

            for (USceneComponent* Child : Children)
            {
                if (UStaticMeshComponent* SMC = Cast<UStaticMeshComponent>(Child))
                {
                    TArray<FString> AssetNames = Wrapper.PrepareMeshForMuJoCo(SMC, bComplexMeshRequired);
                    if (AssetNames.Num() > 0)
                    {
                        MeshName = AssetNames[0];
                        for (int32 i = 1; i < AssetNames.Num(); ++i) ExtraMeshNames.Add(AssetNames[i]);
                        break;
                    }
                }
            }
        }
    }

    ExportTo(geom, SpecDef);

    // Extra mesh names from complex decomposition — hull sub-geoms should already
    // exist as persistent components (created by DecomposeMesh editor action).
    // They register themselves via their own RegisterToSpec calls.
    // Log a warning if we have extra meshes but no hull sub-geoms.
    if (ExtraMeshNames.Num() > 0)
    {
        UE_LOG(LogURLab, Warning, TEXT("[MjGeom] '%s' has %d extra mesh parts from complex decomposition. "
            "These should be persistent hull sub-geom components. Use 'Decompose Mesh' in the editor."),
            *GetName(), ExtraMeshNames.Num());
    }
}

void UMjGeom::SetGeomVisibility(bool bNewVisibility)
{
    TArray<USceneComponent*> Children;
    GetChildrenComponents(true, Children);
    
    for (USceneComponent* Child : Children)
    {
        if (UStaticMeshComponent* MeshComp = Cast<UStaticMeshComponent>(Child))
        {
            MeshComp->SetVisibility(bNewVisibility, false);
            MeshComp->bHiddenInGame = !bNewVisibility;
            
#if WITH_EDITOR
            MeshComp->Modify();
            MeshComp->MarkRenderStateDirty();
            MeshComp->RecreateRenderState_Concurrent();
#endif
        }
    }
}

#if WITH_EDITOR
void UMjGeom::DecomposeMesh()
{
    UE_LOG(LogURLab, Log, TEXT("[MjGeom] DecomposeMesh called on '%s'. Type=%d, bOverride_Type=%d"),
        *GetName(), (int)Type, (int)bOverride_Type);

    if (Type != EMjGeomType::Mesh)
    {
        UE_LOG(LogURLab, Warning, TEXT("[MjGeom] DecomposeMesh: '%s' is not a Mesh geom (Type=%d)."), *GetName(), (int)Type);
        return;
    }

    // Find the child StaticMeshComponent.
    // In the Blueprint editor, GetChildrenComponents() doesn't work on SCS templates —
    // we must walk the SCS node tree instead.
    UStaticMeshComponent* SMC = nullptr;

    // Try runtime attachment hierarchy first (works on placed instances)
    TArray<USceneComponent*> Children;
    GetChildrenComponents(true, Children);
    for (USceneComponent* Child : Children)
    {
        if (UStaticMeshComponent* Found = Cast<UStaticMeshComponent>(Child))
        {
            SMC = Found;
            break;
        }
    }

    // If not found, try SCS node tree (Blueprint editor context).
    // SCS templates aren't attached via the runtime hierarchy, so we walk
    // the outer chain to find the Blueprint and its SCS node tree.
    if (!SMC)
    {
        UBlueprint* BP = nullptr;

        // Walk outer chain to find the Blueprint
        for (UObject* Outer = GetOuter(); Outer; Outer = Outer->GetOuter())
        {
            if (UBlueprint* Found = Cast<UBlueprint>(Outer))
            {
                BP = Found;
                break;
            }
            if (UBlueprintGeneratedClass* BPGC = Cast<UBlueprintGeneratedClass>(Outer))
            {
                BP = Cast<UBlueprint>(BPGC->ClassGeneratedBy);
                break;
            }
        }

        if (BP && BP->SimpleConstructionScript)
        {
            USCS_Node* MyNode = nullptr;
            TArray<USCS_Node*> AllNodes = BP->SimpleConstructionScript->GetAllNodes();
            for (USCS_Node* Node : AllNodes)
            {
                if (Node->ComponentTemplate == this)
                {
                    MyNode = Node;
                    break;
                }
            }
            if (MyNode)
            {
                for (USCS_Node* ChildNode : MyNode->ChildNodes)
                {
                    if (UStaticMeshComponent* Found = Cast<UStaticMeshComponent>(ChildNode->ComponentTemplate))
                    {
                        SMC = Found;
                        break;
                    }
                }
                UE_LOG(LogURLab, Log, TEXT("[MjGeom] DecomposeMesh: SCS node '%s' has %d children. SMC found: %s"),
                    *MyNode->GetVariableName().ToString(), MyNode->ChildNodes.Num(),
                    SMC ? *SMC->GetName() : TEXT("none"));
            }
            else
            {
                UE_LOG(LogURLab, Warning, TEXT("[MjGeom] DecomposeMesh: Could not find SCS node for this component."));
            }
        }
        else
        {
            UE_LOG(LogURLab, Warning, TEXT("[MjGeom] DecomposeMesh: Could not find Blueprint. BP=%p"), BP);
        }
    }

    if (!SMC || !SMC->GetStaticMesh())
    {
        UE_LOG(LogURLab, Warning, TEXT("[MjGeom] DecomposeMesh: '%s' has no StaticMesh child."), *GetName());
        return;
    }

    UStaticMesh* Mesh = SMC->GetStaticMesh();
    UBodySetup* BodySetup = Mesh->GetBodySetup();
    if (!BodySetup || BodySetup->TriMeshGeometries.Num() == 0)
    {
        UE_LOG(LogURLab, Warning, TEXT("[MjGeom] DecomposeMesh: '%s' has no collision geometry."), *GetName());
        return;
    }

    // Remove any existing decomposition first
    RemoveDecomposition();

    FScopedSlowTask SlowTask(2.f, NSLOCTEXT("URLab", "DecomposingMesh", "Running CoACD mesh decomposition..."));
    SlowTask.MakeDialog(/*bShowCancelButton=*/false);

    SlowTask.EnterProgressFrame(1.f, NSLOCTEXT("URLab", "DecompStep1", "Decomposing mesh with CoACD..."));

    // Export and decompose
    FString AssetName = Mesh->GetName();
    FString OwnerDir = GetOwner() ? GetOwner()->GetClass()->GetName() : TEXT("Shared");
    FString FilePath = FString::Printf(TEXT("%s/URLab/ConvertedMeshes/%s/Complex_%s.obj"),
        *FPaths::ProjectSavedDir(), *OwnerDir, *AssetName);
    FString FullFilePath = FPaths::ConvertRelativePathToFull(FilePath);

    const int32 GeometryIndex = 0;
    auto& TriGeom = BodySetup->TriMeshGeometries[GeometryIndex];
    auto& Vertices = TriGeom.GetReference()->Particles().X();

    int MeshCount = 0;
    IO::DeleteMeshCache(FullFilePath, true);

    if (TriGeom.GetReference()->Elements().RequiresLargeIndices())
    {
        const auto& Indices = TriGeom.GetReference()->Elements().GetLargeIndexBuffer();
        MeshCount = MeshUtils::SaveMesh(FullFilePath, Vertices, Indices, true, CoACDThreshold);
        { FString Hash = IO::ComputeMeshHash(Vertices, Indices) + FString::Printf(TEXT("_complex_%.4f"), CoACDThreshold);
        IO::SaveMeshHash(FullFilePath, Hash); }
    }
    else
    {
        const auto& Indices = TriGeom.GetReference()->Elements().GetSmallIndexBuffer();
        MeshCount = MeshUtils::SaveMesh(FullFilePath, Vertices, Indices, true, CoACDThreshold);
        { FString Hash = IO::ComputeMeshHash(Vertices, Indices) + FString::Printf(TEXT("_complex_%.4f"), CoACDThreshold);
        IO::SaveMeshHash(FullFilePath, Hash); }
    }

    if (MeshCount == 0)
    {
        UE_LOG(LogURLab, Error, TEXT("[MjGeom] DecomposeMesh: CoACD produced 0 hulls for '%s'."), *GetName());
        return;
    }

    SlowTask.EnterProgressFrame(1.f, FText::Format(
        NSLOCTEXT("URLab", "DecompStep2", "Creating {0} hull components..."),
        FText::AsNumber(MeshCount)));

    // Create hull sub-geom components.
    // In the BP editor we must create SCS nodes; at runtime we use instance components.
    UBlueprint* BP = nullptr;
    USCS_Node* MyNode = nullptr;
    for (UObject* Outer = GetOuter(); Outer; Outer = Outer->GetOuter())
    {
        if (UBlueprint* Found = Cast<UBlueprint>(Outer)) { BP = Found; break; }
        if (UBlueprintGeneratedClass* BPGC = Cast<UBlueprintGeneratedClass>(Outer))
        { BP = Cast<UBlueprint>(BPGC->ClassGeneratedBy); break; }
    }
    if (BP && BP->SimpleConstructionScript)
    {
        for (USCS_Node* Node : BP->SimpleConstructionScript->GetAllNodes())
        {
            if (Node->ComponentTemplate == this) { MyNode = Node; break; }
        }
    }

    // Find the parent node (hull siblings go under the same parent as this geom)
    USCS_Node* ParentNode = nullptr;
    if (MyNode && BP)
    {
        for (USCS_Node* Node : BP->SimpleConstructionScript->GetAllNodes())
        {
            if (Node->ChildNodes.Contains(MyNode)) { ParentNode = Node; break; }
        }
    }

    // Runtime fallback
    USceneComponent* ParentComp = GetAttachParent();
    bool bIsSCSContext = (MyNode != nullptr && ParentNode != nullptr);

    if (!bIsSCSContext && !ParentComp)
    {
        UE_LOG(LogURLab, Error, TEXT("[MjGeom] DecomposeMesh: '%s' has no parent to attach hulls to."), *GetName());
        return;
    }

    if (BP) BP->Modify(); // UE undo for Blueprint

    for (int32 i = 0; i < MeshCount; ++i)
    {
        FString HullNameStr = FString::Printf(TEXT("%s_hull_%d"), *GetName(), i);

        UMjGeom* Hull = nullptr;

        if (bIsSCSContext)
        {
            // Blueprint editor: create an SCS node
            USCS_Node* HullNode = BP->SimpleConstructionScript->CreateNode(UMjGeom::StaticClass(), *HullNameStr);
            Hull = Cast<UMjGeom>(HullNode->ComponentTemplate);
            ParentNode->AddChildNode(HullNode);
        }
        else
        {
            // Runtime: create instance component
            Hull = NewObject<UMjGeom>(GetOwner(), UMjGeom::StaticClass(),
                MakeUniqueObjectName(GetOwner(), UMjGeom::StaticClass(), *HullNameStr));
            Hull->CreationMethod = EComponentCreationMethod::Instance;
            GetOwner()->AddInstanceComponent(Hull);
            Hull->RegisterComponent();
            Hull->AttachToComponent(ParentComp, FAttachmentTransformRules::KeepRelativeTransform);
        }

        if (!Hull) continue;

        Hull->bIsDecomposedHull = true;
        Hull->bOverride_Type = true;
        Hull->Type = EMjGeomType::Mesh;
        Hull->MeshName = FString::Printf(TEXT("%s_%d"), *AssetName, i);
        Hull->MjName = FString::Printf(TEXT("%s_hull_%d"), *(MjName.IsEmpty() ? GetName() : MjName), i);

        // Copy physics properties from source geom
        Hull->Friction = Friction;           Hull->bOverride_Friction = bOverride_Friction;
        Hull->SolRef = SolRef;               Hull->bOverride_SolRef = bOverride_SolRef;
        Hull->SolImp = SolImp;               Hull->bOverride_SolImp = bOverride_SolImp;
        Hull->Density = Density;             Hull->bOverride_Density = bOverride_Density;
        Hull->Mass = Mass;                   Hull->bOverride_Mass = bOverride_Mass;
        Hull->Margin = Margin;               Hull->bOverride_Margin = bOverride_Margin;
        Hull->Gap = Gap;                     Hull->bOverride_Gap = bOverride_Gap;
        Hull->Condim = Condim;               Hull->bOverride_Condim = bOverride_Condim;
        Hull->Contype = Contype;             Hull->bOverride_Contype = bOverride_Contype;
        Hull->Conaffinity = Conaffinity;     Hull->bOverride_Conaffinity = bOverride_Conaffinity;
        Hull->Priority = Priority;           Hull->bOverride_Priority = bOverride_Priority;
        Hull->Group = 3;                     Hull->bOverride_Group = true;
        Hull->Rgba = Rgba;                   Hull->bOverride_Rgba = bOverride_Rgba;
        Hull->MjClassName = MjClassName;

        // Import the OBJ file as a UStaticMesh asset for editor visualization
        FString SubObjPath = FString::Printf(TEXT("%s/URLab/ConvertedMeshes/%s/Complex_%s_sub_%d.obj"),
            *FPaths::ProjectSavedDir(), *OwnerDir, *AssetName, i);
        FString SubObjFullPath = FPaths::ConvertRelativePathToFull(SubObjPath);

        if (FPaths::FileExists(SubObjFullPath))
        {
            // Use articulation name as subfolder for organization
            FString ArticName = GetOwner() ? GetOwner()->GetName() : TEXT("Unknown");
            FString DestPath = FString::Printf(TEXT("/Game/URLab/DecomposedMeshes/%s"), *ArticName);

            UAutomatedAssetImportData* ImportData = NewObject<UAutomatedAssetImportData>();
            ImportData->Filenames.Add(SubObjFullPath);
            ImportData->DestinationPath = DestPath;
            ImportData->bReplaceExisting = true;
            ImportData->bSkipReadOnly = true;

            IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
            TArray<UObject*> ImportedAssets = AssetTools.ImportAssetsAutomated(ImportData);

            UStaticMesh* ImportedMesh = nullptr;
            for (UObject* Asset : ImportedAssets)
            {
                if (UStaticMesh* SM = Cast<UStaticMesh>(Asset))
                {
                    ImportedMesh = SM;
                    break;
                }
            }

            if (ImportedMesh)
            {
                // Create a child StaticMeshComponent on the hull for visualization
                FString SMCName = FString::Printf(TEXT("%s_vis"), *HullNameStr);

                if (bIsSCSContext && BP)
                {
                    USCS_Node* HullNode = nullptr;
                    // Find the hull's SCS node we just created
                    for (USCS_Node* Node : BP->SimpleConstructionScript->GetAllNodes())
                    {
                        if (Node->ComponentTemplate == Hull) { HullNode = Node; break; }
                    }
                    if (HullNode)
                    {
                        USCS_Node* SMCNode = BP->SimpleConstructionScript->CreateNode(UStaticMeshComponent::StaticClass(), *SMCName);
                        UStaticMeshComponent* VisMesh = Cast<UStaticMeshComponent>(SMCNode->ComponentTemplate);
                        if (VisMesh)
                        {
                            VisMesh->SetStaticMesh(ImportedMesh);
                            VisMesh->SetRelativeScale3D(FVector(100.0f)); // OBJ is in meters, UE in cm
                        }
                        HullNode->AddChildNode(SMCNode);
                    }
                }
                else
                {
                    UStaticMeshComponent* VisMesh = NewObject<UStaticMeshComponent>(GetOwner(), *SMCName);
                    VisMesh->SetStaticMesh(ImportedMesh);
                    VisMesh->SetRelativeScale3D(FVector(100.0f));
                    VisMesh->CreationMethod = EComponentCreationMethod::Instance;
                    GetOwner()->AddInstanceComponent(VisMesh);
                    VisMesh->RegisterComponent();
                    VisMesh->AttachToComponent(Hull, FAttachmentTransformRules::KeepRelativeTransform);
                }

                UE_LOG(LogURLab, Log, TEXT("[MjGeom] Imported hull mesh '%s' for visualization"), *ImportedMesh->GetName());
            }
        }

        UE_LOG(LogURLab, Log, TEXT("[MjGeom] Created hull '%s' (mesh: %s)"), *Hull->GetName(), *Hull->MeshName);
    }

    if (BP)
    {
        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);
    }

    // Disable source geom
    bDisabledByDecomposition = true;

    UE_LOG(LogURLab, Log, TEXT("[MjGeom] Decomposed '%s' into %d hull sub-geoms."), *GetName(), MeshCount);
}

void UMjGeom::RemoveDecomposition()
{
    FString SourceName = MjName.IsEmpty() ? GetName() : MjName;
    int32 Removed = 0;

    // Try SCS path first (Blueprint editor)
    UBlueprint* BP = nullptr;
    for (UObject* Outer = GetOuter(); Outer; Outer = Outer->GetOuter())
    {
        if (UBlueprint* Found = Cast<UBlueprint>(Outer)) { BP = Found; break; }
        if (UBlueprintGeneratedClass* BPGC = Cast<UBlueprintGeneratedClass>(Outer))
        { BP = Cast<UBlueprint>(BPGC->ClassGeneratedBy); break; }
    }

    if (BP && BP->SimpleConstructionScript)
    {
        BP->Modify();
        TArray<USCS_Node*> AllNodes = BP->SimpleConstructionScript->GetAllNodes();
        TArray<USCS_Node*> NodesToRemove;

        for (USCS_Node* Node : AllNodes)
        {
            UMjGeom* Geom = Cast<UMjGeom>(Node->ComponentTemplate);
            if (Geom && Geom != this && Geom->bIsDecomposedHull)
            {
                if (Geom->MjName.StartsWith(SourceName + TEXT("_hull_")))
                {
                    NodesToRemove.Add(Node);
                }
            }
        }

        for (USCS_Node* Node : NodesToRemove)
        {
            BP->SimpleConstructionScript->RemoveNode(Node);
            Removed++;
        }

        if (Removed > 0)
        {
            FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);
        }
    }
    else if (GetOwner())
    {
        // Runtime path: destroy instance components
        TArray<UMjGeom*> AllGeoms;
        GetOwner()->GetComponents<UMjGeom>(AllGeoms);

        for (UMjGeom* Geom : AllGeoms)
        {
            if (Geom == this) continue;
            if (!Geom->bIsDecomposedHull) continue;
            if (Geom->MjName.StartsWith(SourceName + TEXT("_hull_")))
            {
                Geom->DestroyComponent();
                Removed++;
            }
        }
    }

    bDisabledByDecomposition = false;
    UE_LOG(LogURLab, Log, TEXT("[MjGeom] Removed %d hull sub-geoms for '%s'. Source geom re-enabled."), Removed, *GetName());
}
#else
void UMjGeom::DecomposeMesh() {}
void UMjGeom::RemoveDecomposition() {}
#endif

#if WITH_EDITOR
TArray<FString> UMjGeom::GetDefaultClassOptions() const
{
    return GetSiblingComponentOptions(this, UMjDefault::StaticClass(), true);
}
#endif
