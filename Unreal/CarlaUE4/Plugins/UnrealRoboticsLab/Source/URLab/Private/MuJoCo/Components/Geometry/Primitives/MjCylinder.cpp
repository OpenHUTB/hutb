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

#include "MuJoCo/Components/Geometry/Primitives/MjCylinder.h"

#include "MuJoCo/Utils/MjOrientationUtils.h"
#include "Utils/URLabLogging.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "UObject/ConstructorHelpers.h"

UMjCylinder::UMjCylinder()
{
	Type = EMjGeomType::Cylinder;
	bOverride_Type = true;
}

void UMjCylinder::EnsureVisualizerMesh()
{
    if (VisualizerMesh) return;

    VisualizerMesh = NewObject<UStaticMeshComponent>(this, TEXT("VisualizerMesh"));
    if (VisualizerMesh)
    {
        VisualizerMesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
        VisualizerMesh->SetCollisionResponseToAllChannels(ECR_Overlap);

        UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
        if (Mesh) VisualizerMesh->SetStaticMesh(Mesh);

        if (IsRegistered())
        {
            VisualizerMesh->SetupAttachment(this);
            VisualizerMesh->RegisterComponent();
        }
    }
}

void UMjCylinder::OnRegister()
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


void UMjCylinder::ImportFromXml(const FXmlNode* Node)
{
	ImportFromXml(Node, FMjCompilerSettings());
}

void UMjCylinder::ImportFromXml(const FXmlNode* Node, const FMjCompilerSettings& CompilerSettings)
{
	Super::ImportFromXml(Node, CompilerSettings);
	Radius = Size.X;
	HalfLength = Size.Y;

    // Sync Unreal Scale immediately on import so the editor visual matches the data
    const float BaseSize = 50.0f;
    const float UnitScale = 100.0f;
    FVector NewScale;
    NewScale.X = NewScale.Y = (Radius * UnitScale) / BaseSize;
    NewScale.Z = (HalfLength * UnitScale) / BaseSize;
    SetRelativeScale3D(NewScale);
}

void UMjCylinder::ExportTo(mjsGeom* geom, mjsDefault* def)
{
    // Derive Size from Unreal scale
    // Unreal Cylinder: Diameter=100, Height=100
    // MuJoCo Cylinder size: [radius, half-length]
    FVector Scale = GetRelativeScale3D();
    Size.X = Scale.X * 0.5f; // Radius
    Size.Y = Scale.Z * 0.5f; // Half-length

    // For user-authored geoms (bWasImported=false), SizeParamsCount was never set by the
    // XML parser, so we set it here to tell the base ExportTo to write both components.
    if (!bWasImported)
    {
       
        bOverride_Size = true;
    }

	Super::ExportTo(geom, def);
}

void UMjCylinder::ApplyOverrideMaterial(UMaterialInterface* Material)
{
    EnsureVisualizerMesh();
    if (VisualizerMesh && Material && IsValid(Material)) VisualizerMesh->SetMaterial(0, Material);
}

void UMjCylinder::SyncUnrealTransformFromMj()
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
        // MuJoCo cylinder size: [radius, half-length]
        // Unreal Cylinder: Diameter=100, Height=100
        float RadiusVal = m_GeomView.size[0];
        float HalfLengthVal = m_GeomView.size[1];

        FVector NewScale = FVector(RadiusVal * 2.0f, RadiusVal * 2.0f, HalfLengthVal * 2.0f);
        SetRelativeScale3D(NewScale);

        UE_LOG(LogURLabBind, Log, TEXT("[MjCylinder] Syncing Scale for '%s' from MuJoCo radius: %f, half-length: %f -> NewScale: %s"), 
            *GetName(), RadiusVal, HalfLengthVal, *NewScale.ToString());
    }
}

#if WITH_EDITOR
void UMjCylinder::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

    FName PropertyName = (PropertyChangedEvent.Property != nullptr) ? PropertyChangedEvent.Property->GetFName() : NAME_None;
    FName MemberPropertyName = (PropertyChangedEvent.MemberProperty != nullptr) ? PropertyChangedEvent.MemberProperty->GetFName() : NAME_None;

    // Enforce uniform radius (X/Y) scaling for Cylinder
    if (PropertyName == FName(TEXT("RelativeScale3D")) || MemberPropertyName == FName(TEXT("RelativeScale3D")))
    {
        FVector Scale = GetRelativeScale3D();
        if (!FMath::IsNearlyEqual(Scale.X, Scale.Y))
        {
            // Force Y to match X
            Scale.Y = Scale.X;
            SetRelativeScale3D(Scale);
        }
    }
}
#endif

void UMjCylinder::SetGeomVisibility(bool bNewVisibility)
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

