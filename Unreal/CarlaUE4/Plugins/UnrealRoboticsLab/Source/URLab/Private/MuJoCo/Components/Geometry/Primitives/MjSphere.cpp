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

#include "MuJoCo/Components/Geometry/Primitives/MjSphere.h"

#include "MuJoCo/Utils/MjOrientationUtils.h"
#include "Utils/URLabLogging.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "UObject/ConstructorHelpers.h"

UMjSphere::UMjSphere()
{
	Type = EMjGeomType::Sphere;
	bOverride_Type = true;
}

void UMjSphere::EnsureVisualizerMesh()
{
    if (VisualizerMesh) return;

    VisualizerMesh = NewObject<UStaticMeshComponent>(this, TEXT("VisualizerMesh"));
    if (VisualizerMesh)
    {
        VisualizerMesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
        VisualizerMesh->SetCollisionResponseToAllChannels(ECR_Overlap);

        UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere"));
        if (Mesh) VisualizerMesh->SetStaticMesh(Mesh);

        if (IsRegistered())
        {
            VisualizerMesh->SetupAttachment(this);
            VisualizerMesh->RegisterComponent();
        }
    }
}

void UMjSphere::OnRegister()
{
    Super::OnRegister();
    EnsureVisualizerMesh();

    if (VisualizerMesh && !VisualizerMesh->IsRegistered())
    {
        VisualizerMesh->SetupAttachment(this);
        VisualizerMesh->RegisterComponent();
    }

    if (VisualizerMesh && OverrideMaterial && IsValid(OverrideMaterial) && GetOwner())
    {
        VisualizerMesh->SetMaterial(0, OverrideMaterial);
    }
}


void UMjSphere::ImportFromXml(const FXmlNode* Node)
{
	ImportFromXml(Node, FMjCompilerSettings());
}

void UMjSphere::ImportFromXml(const FXmlNode* Node, const FMjCompilerSettings& CompilerSettings)
{
	Super::ImportFromXml(Node, CompilerSettings);
	Radius = Size.X;

    // Sync Unreal Scale immediately on import so the editor visual matches the data
    const float BaseSize = 50.0f;
    const float UnitScale = 100.0f;
    FVector NewScale = FVector((Radius * UnitScale) / BaseSize);
    SetRelativeScale3D(NewScale);
}

void UMjSphere::ExportTo(mjsGeom* geom, mjsDefault* def)
{
    // Derive Size from Unreal scale
    // Unreal Sphere is 100 units diameter, MuJoCo sphere size is radius in meters
    // 1.0 Unreal Scale = 50cm radius = 0.5 size
    FVector Scale = GetRelativeScale3D();
    Size.X = Scale.X * 0.5f;

    // For user-authored geoms (bWasImported=false), SizeParamsCount was never set by the
    // XML parser, so we set it here to tell the base ExportTo to write the radius.
    if (!bWasImported)
    {
       
        bOverride_Size = true;
    }

	Super::ExportTo(geom, def);
}

void UMjSphere::ApplyOverrideMaterial(UMaterialInterface* Material)
{
    EnsureVisualizerMesh();
    if (VisualizerMesh && Material && IsValid(Material)) VisualizerMesh->SetMaterial(0, Material);
}

void UMjSphere::SyncUnrealTransformFromMj()
{
    if (m_GeomView.id == -1) return;

    if (bOverride_FromTo)
    {
        if (VisualizerMesh)
        {
            VisualizerMesh->DestroyComponent();
            VisualizerMesh = nullptr;
        }
        return;
    }

    if (!bOverride_Size)
    {
        // MuJoCo sphere size is radius. Unreal Sphere is 100 units diameter.
        float RadiusVal = m_GeomView.size[0];
        FVector NewScale = FVector(RadiusVal * 2.0f);
        SetRelativeScale3D(NewScale);

        UE_LOG(LogURLabBind, Log, TEXT("[MjSphere] Syncing Scale for '%s' from MuJoCo radius: %f -> NewScale: %s"), 
            *GetName(), RadiusVal, *NewScale.ToString());
    }
}

#if WITH_EDITOR
void UMjSphere::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

    FName PropertyName = (PropertyChangedEvent.Property != nullptr) ? PropertyChangedEvent.Property->GetFName() : NAME_None;
    FName MemberPropertyName = (PropertyChangedEvent.MemberProperty != nullptr) ? PropertyChangedEvent.MemberProperty->GetFName() : NAME_None;

    // Enforce uniform scaling for Sphere
    if (PropertyName == FName(TEXT("RelativeScale3D")) || MemberPropertyName == FName(TEXT("RelativeScale3D")))
    {
        FVector Scale = GetRelativeScale3D();
        if (!Scale.AllComponentsEqual())
        {
            // Force Y and Z to match X
            Scale.Y = Scale.Z = Scale.X;
            SetRelativeScale3D(Scale);
        }
    }
}
#endif

void UMjSphere::SetGeomVisibility(bool bNewVisibility)
{
    if (VisualizerMesh)
    {
        VisualizerMesh->SetVisibility(bNewVisibility, false);
        VisualizerMesh->bHiddenInGame = !bNewVisibility;
        
#if WITH_EDITOR
        VisualizerMesh->Modify();
        VisualizerMesh->MarkRenderStateDirty();
        VisualizerMesh->RecreateRenderState_Concurrent();
#endif
    }
}
