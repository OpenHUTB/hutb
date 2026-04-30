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

#include "MuJoCo/Components/QuickConvert/MjQuickConvertComponent.h"
#include "MuJoCo/Components/Bodies/MjBody.h"
#include "MuJoCo/Components/Joints/MjFreeJoint.h"
#include "MuJoCo/Utils/MjUtils.h"

#include "MuJoCo/Components/Geometry/MjGeom.h"

#include "RHI.h"
#include "RHIResources.h"
#include "CoreMinimal.h"
#include "Chaos/TriangleMeshImplicitObject.h"

#include "GameFramework/Actor.h"
#include "PhysicsEngine/BodySetup.h"
#include "Image/ImageBuilder.h"
#include "Utils/MeshUtils.h"
#include "Utils/URLabLogging.h"
#include "Utils/IO.h"

UMjQuickConvertComponent::UMjQuickConvertComponent() {
    PrimaryComponentTick.bCanEverTick = true;
}


FString UMjQuickConvertComponent::GetBodyName() {
    return m_BodyName;
}

int32 UMjQuickConvertComponent::GetMjBodyId() const {
    return m_CreatedBody ? m_CreatedBody->GetMj().id : -1;
}

void UMjQuickConvertComponent::BeginPlay() {
    Super::BeginPlay();
}

void UMjQuickConvertComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (wrapper)
    {
        delete wrapper;
        wrapper = nullptr;
    }
    Super::EndPlay(EndPlayReason);
}

void UMjQuickConvertComponent::DrawDebugCollision() {

    if (!m_debug_meshes || !m_model || !m_data || !m_CreatedBody)
        return;

    float Multiplier = 100.0f;
    UWorld* World = GetWorld();
    if (!World) return;

    for (auto geom_view : m_CreatedBody->GetMj().Geoms()) {
        int meshId = geom_view.dataid;

        mjtNum* pos = geom_view.xpos; 
        mjtNum* mat = geom_view.xmat;

        FVector Position = FVector(pos[0], -pos[1], pos[2]);
        Position *= Multiplier;

        mjtNum _quat[4];
        mju_mat2Quat(_quat, mat);
        const FQuat quat = MjUtils::MjToUERotation(_quat);

        if (m_model->mesh_graphadr[meshId] == -1) {
            continue;
        }
        int graphStart = m_model->mesh_graphadr[meshId];
        int* graphData = m_model->mesh_graph + graphStart;

            int numVert = graphData[0];
        int numFace = graphData[1];
        int* edgeLocalId = &graphData[2 + numVert * 2];
        int* faceGlobalId = &edgeLocalId[numVert + 3 * numFace];

        float* vertices = m_model->mesh_vert + m_model->mesh_vertadr[meshId] * 3;
        for (int j = 0; j < numFace; ++j) {
            int v1_global = faceGlobalId[3 * j];
            int v2_global = faceGlobalId[3 * j + 1];
            int v3_global = faceGlobalId[3 * j + 2];

            // Negate Y for MJ -> UE handedness conversion
            FVector vertex1(vertices[3 * v1_global], -vertices[3 * v1_global + 1], vertices[3 * v1_global + 2]);
            FVector vertex2(vertices[3 * v2_global], -vertices[3 * v2_global + 1], vertices[3 * v2_global + 2]);
            FVector vertex3(vertices[3 * v3_global], -vertices[3 * v3_global + 1], vertices[3 * v3_global + 2]);

            vertex1 *= Multiplier;
            vertex2 *= Multiplier;
            vertex3 *= Multiplier;

            vertex1 = quat.RotateVector(vertex1);
            vertex2 = quat.RotateVector(vertex2);
            vertex3 = quat.RotateVector(vertex3);

            vertex1 += Position;
            vertex2 += Position;
            vertex3 += Position;

            DrawDebugLine(World, vertex1, vertex2, FColor::Magenta, false, -1, 0, 0.15f);
            DrawDebugLine(World, vertex2, vertex3, FColor::Magenta, false, -1, 0, 0.15f);
            DrawDebugLine(World, vertex3, vertex1, FColor::Magenta, false, -1, 0, 0.15f);
        }
    }
}

void UMjQuickConvertComponent::Setup(mjSpec* spec, mjVFS* vfs) {

    if (!spec)
    {
        UE_LOG(LogURLab, Error, TEXT("[MjQuickConvertComponent] '%s': Setup() called with null mjSpec. Is AAMjManager present in the level?"), *GetName());
        return;
    }

    spec_ = spec;
    vfs_ = vfs;
    wrapper = new FMujocoSpecWrapper(spec_, vfs_);
    m_actor = GetOwner();

    m_isStatic = m_actor->IsRootComponentStationary() || m_actor->IsRootComponentStatic();
    
    FString BodyName = m_actor->GetName() + TEXT("_MjBody");
    UMjBody* MjBody = NewObject<UMjBody>(m_actor, FName(*BodyName));
    if (MjBody)
    {
        MjBody->RegisterComponent();
        MjBody->AttachToComponent(m_actor->GetRootComponent(), FAttachmentTransformRules::KeepRelativeTransform);
        
        MjBody->bIsQuickConverted = true;
        MjBody->bDrivenByUnreal = bDrivenByUnreal;
        
        if (!Static && !bDrivenByUnreal)
        {
            UMjFreeJoint* FreeJoint = NewObject<UMjFreeJoint>(MjBody, FName(FString(m_actor->GetName() +TEXT("FreeJoint"))));

            if (FreeJoint)
            {
                FreeJoint->RegisterComponent();
                FreeJoint->AttachToComponent(MjBody, FAttachmentTransformRules::KeepRelativeTransform);
                UE_LOG(LogURLab, Log, TEXT("Added free joint to QuickConvert body %s (via Setup)"), *m_actor->GetName());
            }
            else
            {
                UE_LOG(LogURLab, Warning, TEXT("Failed to create free joint for QuickConvert body %s"), *m_actor->GetName());
            }
        }
        
        TInlineComponentArray<UActorComponent*, 30> Components;
        m_actor->GetComponents(UStaticMeshComponent::StaticClass(), Components);

        int MeshIndex = 0;
        for (auto&& Comp : Components) {
            UStaticMeshComponent* SMC = Cast<UStaticMeshComponent>(Comp);
            if (!SMC || !SMC->GetStaticMesh()) continue;

             
             FString AssetName = SMC->GetStaticMesh()->GetName();
             
             TArray<FString> MjAssetNames = wrapper->PrepareMeshForMuJoCo(SMC, ComplexMeshRequired, CoACDThreshold);

             if (ComplexMeshRequired)
             {
                 TArray<FString> VisualMeshNames = wrapper->PrepareMeshForMuJoCo(SMC, false);
                 if (VisualMeshNames.Num() > 0)
                 {
                     FString VisualGeomName = FString::Printf(TEXT("Geom_%d_visual"), MeshIndex);
                     UMjGeom* VisualGeom = NewObject<UMjGeom>(MjBody, FName(*VisualGeomName));
                     if (VisualGeom)
                     {
                         VisualGeom->RegisterComponent();
                         VisualGeom->AttachToComponent(MjBody, FAttachmentTransformRules::KeepRelativeTransform);
                         VisualGeom->Type = EMjGeomType::Mesh;
                         VisualGeom->bOverride_Type = true;
                         VisualGeom->MeshName = VisualMeshNames[0];

                         VisualGeom->bOverride_Contype = true;
                         VisualGeom->Contype = 0;
                         VisualGeom->bOverride_Conaffinity = true;
                         VisualGeom->Conaffinity = 0;
                         VisualGeom->bOverride_Group = true;
                         VisualGeom->Group = 2;
                     }
                 }

                 for (int i = 0; i < MjAssetNames.Num(); ++i)
                 {
                     FString GeomName = FString::Printf(TEXT("Geom_%d_%d"), MeshIndex, i);
                     UMjGeom* Geom = NewObject<UMjGeom>(MjBody, FName(*GeomName));
                     if (Geom)
                     {
                         Geom->RegisterComponent();
                         Geom->AttachToComponent(MjBody, FAttachmentTransformRules::KeepRelativeTransform);
                         Geom->Type = EMjGeomType::Mesh;
                         Geom->bOverride_Type = true;
                         Geom->MeshName = MjAssetNames[i];
                         Geom->bOverride_Group = true;
                         Geom->Group = 3;

                         Geom->bOverride_Friction = true;
                         Geom->Friction = { (float)friction.X, (float)friction.Y, (float)friction.Z };
                         Geom->bOverride_SolRef = true;
                         Geom->SolRef = { (float)solref.X, (float)solref.Y };
                         Geom->bOverride_SolImp = true;
                         Geom->SolImp = { (float)solimp.X, (float)solimp.Y, (float)solimp.Z };
                     }
                 }
             }
             else
             {
                 for (int i = 0; i < MjAssetNames.Num(); ++i)
                 {
                     FString MjAssetName = MjAssetNames[i];
                     FString GeomName = FString::Printf(TEXT("Geom_%d_%d"), MeshIndex, i);
                     UMjGeom* Geom = NewObject<UMjGeom>(MjBody, FName(*GeomName));
                     if (Geom)
                     {
                         Geom->RegisterComponent();
                         Geom->AttachToComponent(MjBody, FAttachmentTransformRules::KeepRelativeTransform);
                         Geom->Type = EMjGeomType::Mesh;
                         Geom->bOverride_Type = true;
                         Geom->MeshName = MjAssetName;

                         Geom->bOverride_Friction = true;
                         Geom->Friction = { (float)friction.X, (float)friction.Y, (float)friction.Z };
                         Geom->bOverride_SolRef = true;
                         Geom->SolRef = { (float)solref.X, (float)solref.Y };
                         Geom->bOverride_SolImp = true;
                         Geom->SolImp = { (float)solimp.X, (float)solimp.Y, (float)solimp.Z };
                     }
                 }
             }
             MeshIndex++;
        }
        
        mjsBody* WorldBody = mjs_findBody(spec_, "world");
        
        MjBody->Setup(m_actor->GetRootComponent(), WorldBody, wrapper);
        
        m_CreatedBody = MjBody;
    }
}

void UMjQuickConvertComponent::PostSetup(mjModel* model, mjData* data) {

    m_model = model;
    m_data = data;
    
    if (m_CreatedBody)
    {
        m_CreatedBody->Bind(model, data);
    }
}




void UMjQuickConvertComponent::UpdateUETransform() {

    if (!m_model || !m_data) {
        return;
    }

    if (bDrivenByUnreal && MocapPos && MocapQuat)
    {
         MjUtils::UEToMjPosition(m_actor->GetActorLocation(), MocapPos);
         MjUtils::UEToMjRotation(m_actor->GetActorQuat(), MocapQuat);
         return;
    }
        FVector _pos = MjUtils::MjToUEPosition(m_CreatedBody->GetBodyView().xpos);
        FQuat _quat = MjUtils::MjToUERotation(m_CreatedBody->GetBodyView().xquat);

        m_actor->SetActorRotation(_quat);
        m_actor->SetActorLocation(_pos);
}

void UMjQuickConvertComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) {
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    if (!bDrivenByUnreal && m_CreatedBody)
    {
        UpdateUETransform();
    }

    if (m_debug_meshes)
    {
        DrawDebugCollision();
    }
}
