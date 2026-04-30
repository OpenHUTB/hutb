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

#include "MuJoCo/Components/Geometry/Primitives/MjCapsule.h"

#include "MuJoCo/Utils/MjOrientationUtils.h"
#include "Utils/URLabLogging.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"

namespace
{
    // Engine-content defaults: Cylinder.Cylinder is 100 cm tall × 100 cm
    // diameter, Sphere.Sphere is 100 cm diameter. A scale of 1.0 on a
    // UMjCapsule maps to a 50 cm radius / 50 cm half-length (matches
    // UMjCylinder's convention). Centimetres-per-metre = 100.
    constexpr float kCmPerM    = 100.0f;
    constexpr float kBaseHalf  = 50.0f;   // 50cm half-extent of engine base shape
    constexpr float kMinScaleZ = 0.001f;  // guard for counter-scale divide
}

UMjCapsule::UMjCapsule()
{
    Type = EMjGeomType::Capsule;
    bOverride_Type = true;
}

void UMjCapsule::EnsureVisualizerMesh()
{
    if (IsValid(VisualizerShaft) && IsValid(VisualizerCapTop) && IsValid(VisualizerCapBottom)) return;

    UStaticMesh* CylinderMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
    UStaticMesh* SphereMesh   = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere"));

    auto MakeSubMesh = [&](const FName InName, UStaticMesh* InMesh) -> UStaticMeshComponent*
    {
        UStaticMeshComponent* SMC = NewObject<UStaticMeshComponent>(this, InName);
        if (!SMC) return nullptr;
        SMC->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
        SMC->SetCollisionResponseToAllChannels(ECR_Overlap);
        if (InMesh) SMC->SetStaticMesh(InMesh);
        if (IsRegistered())
        {
            SMC->SetupAttachment(this);
            SMC->RegisterComponent();
        }
        return SMC;
    };

    if (!IsValid(VisualizerShaft))     VisualizerShaft     = MakeSubMesh(TEXT("VisualizerShaft"),     CylinderMesh);
    if (!IsValid(VisualizerCapTop))    VisualizerCapTop    = MakeSubMesh(TEXT("VisualizerCapTop"),    SphereMesh);
    if (!IsValid(VisualizerCapBottom)) VisualizerCapBottom = MakeSubMesh(TEXT("VisualizerCapBottom"), SphereMesh);

    UpdateCapTransforms();
}

void UMjCapsule::OnRegister()
{
    Super::OnRegister();
    EnsureVisualizerMesh();

    auto RegisterIfNeeded = [this](UStaticMeshComponent* SMC)
    {
        if (IsValid(SMC) && !SMC->IsRegistered())
        {
            SMC->SetupAttachment(this);
            SMC->RegisterComponent();
        }
    };
    RegisterIfNeeded(VisualizerShaft);
    RegisterIfNeeded(VisualizerCapTop);
    RegisterIfNeeded(VisualizerCapBottom);

    UpdateCapTransforms();
}

void UMjCapsule::UpdateCapTransforms()
{
    const FVector ParentScale = GetRelativeScale3D();
    const float   SafeZ       = FMath::Max(FMath::Abs(ParentScale.Z), kMinScaleZ);

    // Caps inherit the parent's X/Y scale (correct sphere diameter), but
    // counter-scale on Z so parent's Z stretch doesn't deform them into
    // ellipsoids. Z-sign preserved to support mirrored (negative-scale) setups.
    const float CapZ = (ParentScale.Z >= 0.0f ? 1.0f : -1.0f) * (ParentScale.X / SafeZ);

    // Cap centres sit at ±HalfLength world-cm = ±(ParentScale.Z * 50 cm).
    // Because Z positions are interpreted in the parent's local (un-scaled)
    // space and then scaled by ParentScale.Z, a constant local ±50 lands at
    // the correct world position as HalfLength varies with ParentScale.Z.
    const FVector CapLocalUp   = FVector(0.0f, 0.0f,  kBaseHalf);
    const FVector CapLocalDown = FVector(0.0f, 0.0f, -kBaseHalf);
    const FVector CapScale     = FVector(1.0f, 1.0f, CapZ);

    // IsValid() checks cover both null and pending-kill (post-reconstruction
    // transient state) on top of the UPROPERTY-nulling guarantee.
    if (IsValid(VisualizerCapTop))
    {
        VisualizerCapTop->SetRelativeLocation(CapLocalUp);
        VisualizerCapTop->SetRelativeScale3D(CapScale);
    }
    if (IsValid(VisualizerCapBottom))
    {
        VisualizerCapBottom->SetRelativeLocation(CapLocalDown);
        VisualizerCapBottom->SetRelativeScale3D(CapScale);
    }
    if (IsValid(VisualizerShaft))
    {
        VisualizerShaft->SetRelativeLocation(FVector::ZeroVector);
        VisualizerShaft->SetRelativeScale3D(FVector(1.0f, 1.0f, 1.0f));
    }
}

void UMjCapsule::ImportFromXml(const FXmlNode* Node)
{
    ImportFromXml(Node, FMjCompilerSettings());
}

void UMjCapsule::ImportFromXml(const FXmlNode* Node, const FMjCompilerSettings& CompilerSettings)
{
    Super::ImportFromXml(Node, CompilerSettings);

    // Super already resolves any `fromto` into pos/quat + Size.Y (half-length).
    // Capsule's MJCF `size` is [radius, halflength] — same layout as cylinder.
    Radius     = Size.X;
    HalfLength = Size.Y;

    // Map (radius, halflength) → parent scale, mirroring UMjCylinder.
    FVector NewScale;
    NewScale.X = NewScale.Y = (Radius     * kCmPerM) / kBaseHalf;
    NewScale.Z              = (HalfLength * kCmPerM) / kBaseHalf;
    SetRelativeScale3D(NewScale);

    UpdateCapTransforms();
}

void UMjCapsule::ExportTo(mjsGeom* geom, mjsDefault* def)
{
    // Derive radius + half-length back out of the Unreal scale. Base cylinder
    // is 100cm diameter (radius 50cm) → scale of 1.0 = 50cm radius = 0.5 m.
    FVector Scale = GetRelativeScale3D();
    Size.X = Scale.X * 0.5f; // Radius (metres)
    Size.Y = Scale.Z * 0.5f; // Half-length (metres)

    // User-authored geoms (bWasImported=false) need the size override flag
    // so the base ExportTo writes both components.
    if (!bWasImported)
    {
        bOverride_Size = true;
    }

    Super::ExportTo(geom, def);
}

void UMjCapsule::SyncUnrealTransformFromMj()
{
    if (m_GeomView.id == -1) return;

    if (bOverride_FromTo)
    {
        // Raw fromto is a two-endpoint description; tearing the composite
        // down and letting the user author manually is the path of least
        // surprise (mirrors UMjCylinder's behaviour).
        auto KillSMC = [](UStaticMeshComponent*& SMC)
        {
            if (IsValid(SMC)) { SMC->DestroyComponent(); }
            SMC = nullptr;
        };
        KillSMC(VisualizerShaft);
        KillSMC(VisualizerCapTop);
        KillSMC(VisualizerCapBottom);
        return;
    }

    if (!bOverride_Size)
    {
        const float RadiusVal     = m_GeomView.size[0];
        const float HalfLengthVal = m_GeomView.size[1];

        const FVector NewScale = FVector(RadiusVal * 2.0f, RadiusVal * 2.0f, HalfLengthVal * 2.0f);
        SetRelativeScale3D(NewScale);

        UE_LOG(LogURLabBind, Log,
            TEXT("[MjCapsule] Syncing Scale for '%s' from MuJoCo radius: %f, half-length: %f -> NewScale: %s"),
            *GetName(), RadiusVal, HalfLengthVal, *NewScale.ToString());
    }

    UpdateCapTransforms();
}

#if WITH_EDITOR
void UMjCapsule::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
    Super::PostEditChangeProperty(PropertyChangedEvent);

    const FName PropertyName       = (PropertyChangedEvent.Property       != nullptr) ? PropertyChangedEvent.Property->GetFName()       : NAME_None;
    const FName MemberPropertyName = (PropertyChangedEvent.MemberProperty != nullptr) ? PropertyChangedEvent.MemberProperty->GetFName() : NAME_None;

    // Enforce uniform radius (X==Y) so end caps stay spherical in X/Y.
    if (PropertyName == FName(TEXT("RelativeScale3D")) || MemberPropertyName == FName(TEXT("RelativeScale3D")))
    {
        FVector Scale = GetRelativeScale3D();
        if (!FMath::IsNearlyEqual(Scale.X, Scale.Y))
        {
            Scale.Y = Scale.X;
            SetRelativeScale3D(Scale);
        }
    }

    UpdateCapTransforms();
}
#endif

void UMjCapsule::SetGeomVisibility(bool bNewVisibility)
{
    auto SetVis = [&](UStaticMeshComponent* SMC)
    {
        if (!IsValid(SMC)) return;
        SMC->SetVisibility(bNewVisibility, false);
        SMC->bHiddenInGame = !bNewVisibility;
#if WITH_EDITOR
        SMC->Modify();
        SMC->MarkRenderStateDirty();
        SMC->RecreateRenderState_Concurrent();
#endif
    };
    SetVis(VisualizerShaft);
    SetVis(VisualizerCapTop);
    SetVis(VisualizerCapBottom);
}
