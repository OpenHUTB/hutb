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

#include "MuJoCo/Core/Spec/MjSpecWrapper.h"
#include "Utils/URLabLogging.h"
#include "MuJoCo/Components/Defaults/MjDefault.h"
#include "MuJoCo/Components/Geometry/MjGeom.h"
#include "MuJoCo/Core/MjArticulation.h"
#include "MuJoCo/Utils/MjUtils.h"

#include "PhysicsEngine/BodySetup.h"
#include "Misc/Paths.h"
#include "Utils/IO.h"
#include "Utils/MeshUtils.h"

#include "Chaos/TriangleMeshImplicitObject.h"
#include "MuJoCo/Components/Bodies/MjBody.h"

FMujocoSpecWrapper::FMujocoSpecWrapper(mjSpec* InSpec, mjVFS* InVFS)
    : Spec(InSpec), VFS(InVFS)
{
}


mjsBody* FMujocoSpecWrapper::CreateBody(const FString& Name, const FString& ParentName, const FTransform& WorldTransform)
{
    mjsBody* ParentBody = mjs_findBody(Spec, TCHAR_TO_UTF8(*ParentName));
    if (!ParentBody) {
        ParentBody = mjs_findBody(Spec, "world");
    }

    mjsBody* NewBody = mjs_addBody(ParentBody, nullptr);
    mjs_setName(NewBody->element, TCHAR_TO_UTF8(*Name));

    MjUtils::UEToMjPosition(WorldTransform.GetLocation(), NewBody->pos);
    MjUtils::UEToMjRotation(WorldTransform.GetRotation(), NewBody->quat);

    CreatedBodyNames.Add(Name);

    return NewBody;
}

mjsBody* FMujocoSpecWrapper::CreateBody(const FString& Name, mjsBody* ParentBody, const FTransform& WorldTransform)
{
    if (!ParentBody) {
        ParentBody = mjs_findBody(Spec, "world");
    }

    mjsBody* NewBody = mjs_addBody(ParentBody, nullptr);
    mjs_setName(NewBody->element, TCHAR_TO_UTF8(*Name));

    MjUtils::UEToMjPosition(WorldTransform.GetLocation(), NewBody->pos);
    MjUtils::UEToMjRotation(WorldTransform.GetRotation(), NewBody->quat); 

    CreatedBodyNames.Add(Name);

    return NewBody;
}

mjsFrame* FMujocoSpecWrapper::CreateFrame(const FString& Name, mjsBody* ParentBody, const FTransform& WorldTransform)
{
    if (!ParentBody) {
        ParentBody = mjs_findBody(Spec, "world");
    }

    mjsFrame* NewFrame = mjs_addFrame(ParentBody, nullptr);
    mjs_setName(NewFrame->element, TCHAR_TO_UTF8(*Name));

    MjUtils::UEToMjPosition(WorldTransform.GetLocation(), NewFrame->pos);
    MjUtils::UEToMjRotation(WorldTransform.GetRotation(), NewFrame->quat);

    return NewFrame;
}

FString FMujocoSpecWrapper::AddMeshAsset(const FString& MeshName, const FString& FilePath, const FVector& Scale)
{
    FString UniqueMeshName = GetUniqueName(MeshName, mjOBJ_MESH, nullptr);

    mjsMesh* MeshAsset = mjs_addMesh(Spec, nullptr);
    mjs_setName(MeshAsset->element, TCHAR_TO_UTF8(*UniqueMeshName));
    mjs_setString(MeshAsset->file, TCHAR_TO_UTF8(*FilePath));

    MeshAsset->scale[0] = Scale.X;
    MeshAsset->scale[1] = Scale.Y;
    MeshAsset->scale[2] = Scale.Z;

    FString Directory = FPaths::GetPath(FilePath);
    FString FileName = FPaths::GetCleanFilename(FilePath);

    UE_LOG(LogURLabWrapper, Log, TEXT("MuJoCo VFS: Loading %s from %s with name %s"), *FileName, *Directory, *UniqueMeshName);

    int Result = mj_addFileVFS(VFS, TCHAR_TO_UTF8(*Directory), TCHAR_TO_UTF8(*FileName));
    if (Result != 0) {
        UE_LOG(LogURLabWrapper, Error, TEXT("MuJoCo VFS Error: Failed to load %s (Code %d)"), *FilePath, Result);
    }
    
    return UniqueMeshName;
}

mjsGeom* FMujocoSpecWrapper::AddPrimitiveGeom(mjsBody* Body, mjtGeom Type, const FVector& Size, const FVector4& RGBA, double Density)
{
    if (!Body) return nullptr;

    mjsGeom* Geom = mjs_addGeom(Body, nullptr);
    Geom->type = Type;
    
    Geom->size[0] = Size.X;
    Geom->size[1] = Size.Y;
    Geom->size[2] = Size.Z;

    Geom->rgba[0] = RGBA.X;
    Geom->rgba[1] = RGBA.Y;
    Geom->rgba[2] = RGBA.Z;
    Geom->rgba[3] = RGBA.W;

    Geom->density = Density;

    return Geom;
}

void FMujocoSpecWrapper::AddFreeJoint(mjsBody* Body, const FString& JointName)
{
    if (!Body) return;

    mjsJoint* Jnt = mjs_addJoint(Body, nullptr);
    Jnt->type = mjJNT_FREE;

    Jnt->pos[0] = 0.0;
    Jnt->pos[1] = 0.0;
    Jnt->pos[2] = 0.0;

    Jnt->damping[0] = 0.1;
    mjs_setName(Jnt->element, TCHAR_TO_UTF8(*JointName));
}




void FMujocoSpecWrapper::AddDefault(UMjDefault* DefaultComp)
{
    if (!DefaultComp) return;

    const char* ClassName = DefaultComp->ClassName.IsEmpty() ? nullptr : TCHAR_TO_UTF8(*DefaultComp->ClassName);
    
    mjsDefault* parentDef = nullptr;
    if (!DefaultComp->ParentClassName.IsEmpty())
    {
        parentDef = mjs_findDefault(Spec, TCHAR_TO_UTF8(*DefaultComp->ParentClassName));
        if (parentDef)
        {
             UE_LOG(LogURLabWrapper, Log, TEXT("[AddDefault] Linked Class '%s' to Parent '%s'"), *DefaultComp->ClassName, *DefaultComp->ParentClassName);
        }
        else
        {
             UE_LOG(LogURLabWrapper, Warning, TEXT("[AddDefault] Could not find Parent Class '%s' for '%s'"), *DefaultComp->ParentClassName, *DefaultComp->ClassName);
        }
    }

    mjsDefault* def = nullptr;

    if (DefaultComp->ClassName.IsEmpty() || DefaultComp->ClassName == TEXT("main"))
    {
        if (parentDef == nullptr)
        {
            // This is the root default — use the spec's built-in default
            def = mjs_getSpecDefault(Spec);
            UE_LOG(LogURLabWrapper, Log, TEXT("[AddDefault] Using root default for class '%s'"), *DefaultComp->ClassName);
        }
        else
        {
            def = mjs_addDefault(Spec, ClassName, parentDef);
        }
    }
    else
    {
        def = mjs_addDefault(Spec, ClassName, parentDef);
    }

    if (!def) return;

    DefaultComp->ExportTo(def);
}

TArray<BodyView> FMujocoSpecWrapper::ReconstructViews(const mjModel* m, mjData* d, const FString& Prefix)
{
    TArray<BodyView> Views;
    UE_LOG(LogURLabWrapper, Warning, TEXT("ReconstructViews: %i bodies, prefix='%s'"), CreatedBodyNames.Num(), *Prefix);
    Views.Reserve(CreatedBodyNames.Num());

    for (const FString& Name : CreatedBodyNames)
    {
        FString PrefixedName = Prefix + Name;
        BodyView View = bind<BodyView>(m, d, TCHAR_TO_UTF8(*PrefixedName));
        if (View.id >= 0)
        {
            Views.Add(View);
        }
        else
        {
            UE_LOG(LogURLabWrapper, Warning, TEXT("MuJoCo SpecWrapper: Failed to bind view for body '%s'."), *PrefixedName);
        }
    }

    return Views;
}

TArray<FString> FMujocoSpecWrapper::PrepareMeshForMuJoCo(UStaticMeshComponent* SMC, bool bComplexMeshRequired, float CoACDThreshold)
{
    TArray<FString> ResultNames;
    if (!SMC || !SMC->GetStaticMesh()) return ResultNames;

    UStaticMesh* Mesh = SMC->GetStaticMesh();
    UBodySetup* BodySetup = Mesh->GetBodySetup();
    if (!BodySetup || BodySetup->TriMeshGeometries.Num() == 0) return ResultNames;

    FString MeshType = bComplexMeshRequired ? "Complex" : "Simple";
    UStaticMesh* StaticMesh = Cast<UStaticMesh>(BodySetup->GetOuter());
    FString BaseAssetName = StaticMesh ? StaticMesh->GetName() : SMC->GetName();
    FVector Scale = SMC->GetComponentScale();

    FString AssetName = BaseAssetName;
    if (!Scale.Equals(FVector::OneVector, 0.001f))
    {
        AssetName = FString::Printf(TEXT("%s_s%.3f_%.3f_%.3f"), *BaseAssetName,
            Scale.X, Scale.Y, Scale.Z);
    }

    FString SubDir = MeshCacheSubDir.IsEmpty() ? TEXT("Shared") : MeshCacheSubDir;
    FString FilePath = FString::Printf(
        TEXT("%s/URLab/ConvertedMeshes/%s/%s_%s.obj"),
        *FPaths::ProjectSavedDir(),
        *SubDir,
        *MeshType,
        *AssetName
    );
    FString FullFilePath = FPaths::ConvertRelativePathToFull(FilePath);

    if (!bComplexMeshRequired)
    {
        if (mjs_findElement(Spec, mjOBJ_MESH, TCHAR_TO_UTF8(*AssetName)))
        {
            UE_LOG(LogURLabWrapper, Log, TEXT("Mesh Asset '%s' already exists in Spec. Reusing."), *AssetName);
            ResultNames.Add(AssetName);
            return ResultNames;
        }
    }
    else
    {
        FString FirstSubMeshName = FString::Printf(TEXT("%s_0"), *AssetName);
        if (mjs_findElement(Spec, mjOBJ_MESH, TCHAR_TO_UTF8(*FirstSubMeshName)))
        {
            UE_LOG(LogURLabWrapper, Log, TEXT("Complex Mesh Asset '%s' already exists in Spec. Reusing all parts."), *AssetName);
            int i = 0;
            while (true)
            {
                FString SubMeshName = FString::Printf(TEXT("%s_%d"), *AssetName, i);
                if (mjs_findElement(Spec, mjOBJ_MESH, TCHAR_TO_UTF8(*SubMeshName)))
                {
                    ResultNames.Add(SubMeshName);
                    i++;
                }
                else
                {
                    break;
                }
            }
            return ResultNames;
        }
    }

    const int32 GeometryIndex = 0;
    auto& TriGeom = BodySetup->TriMeshGeometries[GeometryIndex];
    auto& Vertices = TriGeom.GetReference()->Particles().X();

    FString CurrentHash;
    bool bLargeIndices = TriGeom.GetReference()->Elements().RequiresLargeIndices();
    if (bLargeIndices)
    {
        const auto& Indices = TriGeom.GetReference()->Elements().GetLargeIndexBuffer();
        CurrentHash = IO::ComputeMeshHash(Vertices, Indices);
    }
    else
    {
        const auto& Indices = TriGeom.GetReference()->Elements().GetSmallIndexBuffer();
        CurrentHash = IO::ComputeMeshHash(Vertices, Indices);
    }
    CurrentHash += bComplexMeshRequired ? TEXT("_complex") : TEXT("_simple");
    if (bComplexMeshRequired)
    {
        CurrentHash += FString::Printf(TEXT("_t%.4f"), CoACDThreshold);
    }

    int MeshCount = IO::NumFilesExist(FullFilePath, bComplexMeshRequired);
    if (MeshCount > 0)
    {
        FString CachedHash = IO::LoadMeshHash(FullFilePath);
        if (CachedHash != CurrentHash)
        {
            UE_LOG(LogURLabWrapper, Log, TEXT("Mesh '%s' has changed (hash mismatch). Re-exporting."), *AssetName);
            IO::DeleteMeshCache(FullFilePath, bComplexMeshRequired);
            MeshCount = 0;
        }
    }

    if (MeshCount == 0)
    {
        UE_LOG(LogURLabWrapper, Log, TEXT("Saving mesh geometry for: %s"), *AssetName);

        if (bLargeIndices)
        {
            const auto& Indices = TriGeom.GetReference()->Elements().GetLargeIndexBuffer();
            MeshCount = MeshUtils::SaveMesh(FullFilePath, Vertices, Indices, bComplexMeshRequired, CoACDThreshold);
        }
        else
        {
            const auto& Indices = TriGeom.GetReference()->Elements().GetSmallIndexBuffer();
            MeshCount = MeshUtils::SaveMesh(FullFilePath, Vertices, Indices, bComplexMeshRequired, CoACDThreshold);
        }

        if (MeshCount == 0)
        {
            UE_LOG(LogURLabWrapper, Error, TEXT("MeshUtils::SaveMesh failed to save any meshes for %s."), *AssetName);
            return ResultNames;
        }

        IO::SaveMeshHash(FullFilePath, CurrentHash);
    }
    
    FString BaseName = FPaths::GetBaseFilename(FilePath);
    FString Directory = FPaths::GetPath(FilePath);
    
    if (!bComplexMeshRequired) 
    {
        FString sub_file_path = FString::Printf(TEXT("%s/%s.obj"), *Directory, *BaseName);
        ResultNames.Add(AddMeshAsset(AssetName, sub_file_path, Scale));
    } 
    else
    {
        for (int i = 0; i < MeshCount; i++) 
        {
            FString sub_file_path = FString::Printf(TEXT("%s/%s_sub_%d.obj"), *Directory, *BaseName, i);
            FString SubMeshName = FString::Printf(TEXT("%s_%d"), *AssetName, i);
            ResultNames.Add(AddMeshAsset(SubMeshName, sub_file_path, Scale));
        }
    }
    
    return ResultNames; 
}





FString FMujocoSpecWrapper::GetUniqueName(const FString& BaseName, mjtObj Type, const AActor* ContextActor)
{
    FString Candidate = BaseName;
    
    FString TestName = Candidate;
    int32 Counter = 0;
    while (mjs_findElement(Spec, Type, TCHAR_TO_UTF8(*TestName)))
    {
        ++Counter;
        TestName = FString::Printf(TEXT("%s_%d"), *Candidate, Counter);
    }
    
    return TestName;
}



