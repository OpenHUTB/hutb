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

#include "MuJoCo/Core/MjDebugVisualizer.h"
#include "MuJoCo/Core/AMjManager.h"
#include "MuJoCo/Core/MjPhysicsEngine.h"
#include "MuJoCo/Utils/MjUtils.h"
#include "MuJoCo/Utils/MjColor.h"
#include "MuJoCo/Core/MjArticulation.h"
#include "MuJoCo/Components/Geometry/MjGeom.h"
#include "MuJoCo/Components/QuickConvert/MjQuickConvertComponent.h"
#include "MuJoCo/Components/Sensors/MjCamera.h"
#include "DrawDebugHelpers.h"
#include "Components/SplineMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Utils/URLabLogging.h"

UMjDebugVisualizer::UMjDebugVisualizer()
{
    PrimaryComponentTick.bCanEverTick = true;
}

void UMjDebugVisualizer::BeginPlay()
{
    Super::BeginPlay();
    InitializeOverlayMaterial();
    TendonTubeMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
    if (!TendonTubeMesh)
    {
        UE_LOG(LogURLab, Warning,
            TEXT("Debug viz: could not load /Engine/BasicShapes/Cylinder — tendon tubes will be invisible."));
    }
}

void UMjDebugVisualizer::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    UpdateBodyOverlays();

    if (bGlobalDrawTendons)
    {
        UpdateTendonTubes();
    }
    else
    {
        HideTendonTubes();
    }

    if (!bShowDebug) return;

    FMuJoCoDebugData LocalDebugData;
    {
        FScopeLock Lock(&DebugMutex);
        LocalDebugData = DebugData;
    }

    UWorld* World = GetWorld();
    if (!World) return;

    for (int i = 0; i < LocalDebugData.ContactPoints.Num(); ++i)
    {
        float Force = 0.0f;
        if (i < LocalDebugData.ContactForces.Num()) Force = LocalDebugData.ContactForces[i];

        float ClampedForce = FMath::Min(Force, DebugMaxForce);
        float VisualLength = ClampedForce * DebugForceScale;

        DrawDebugPoint(World, LocalDebugData.ContactPoints[i], DebugContactPointSize, FColor::Red, false, -1.0f);

        if (i < LocalDebugData.ContactNormals.Num())
        {
            float ArrowHeadSize = FMath::Clamp(VisualLength * 0.2f, 2.0f, 15.0f);

            DrawDebugDirectionalArrow(World,
                LocalDebugData.ContactPoints[i],
                LocalDebugData.ContactPoints[i] + LocalDebugData.ContactNormals[i] * VisualLength,
                ArrowHeadSize, FColor::Yellow, false, -1.0f, 0, DebugContactArrowThickness);
        }
    }
}

void UMjDebugVisualizer::CaptureDebugData()
{
    AAMjManager* Manager = Cast<AAMjManager>(GetOwner());
    if (!Manager || !Manager->PhysicsEngine || !Manager->PhysicsEngine->m_model || !Manager->PhysicsEngine->m_data) return;

    mjModel* Model = Manager->PhysicsEngine->m_model;
    mjData* Data = Manager->PhysicsEngine->m_data;

    FScopeLock Lock(&DebugMutex);

    DebugData.ContactPoints.Reset();
    DebugData.ContactNormals.Reset();
    DebugData.ContactForces.Reset();

    for (int i = 0; i < Data->ncon; ++i)
    {
        FVector Pos = MjUtils::MjToUEPosition(Data->contact[i].pos);
        DebugData.ContactPoints.Add(Pos);

        // Normal: first row of contact frame, Y-flipped for UE coordinate convention
        double* f = Data->contact[i].frame;
        FVector NormalArg(f[0], -f[1], f[2]);
        DebugData.ContactNormals.Add(NormalArg);

        mjtNum cforce[6];
        mj_contactForce(Model, Data, i, cforce);
        DebugData.ContactForces.Add((float)cforce[0]);
    }

    DebugData.BodyAwake.SetNumUninitialized(Model->nbody);
    // for (int32 i = 0; i < Model->nbody; ++i)
    // {
    //     DebugData.BodyAwake[i] = Data->body_awake ? Data->body_awake[i] : 1;
    // }

    // Per-body Halton seed, matching MuJoCo's native islandColor algorithm in
    // engine_vis_visualize.c: use the active constraint island's dof-address if
    // available, otherwise fall back to the kinematic tree's dof-address so
    // sleeping bodies keep a stable colour. When asleep, mj_sleepCycle collapses
    // connected sleep-cycle trees to their smallest index so a group of bodies
    // resting on each other share one colour.
    DebugData.BodyIslandSeed.Init(-1, Model->nbody);
    const bool bSleepEnabled = (Model->opt.enableflags & mjENBL_SLEEP) != 0;
    for (int32 b = 0; b < Model->nbody; ++b)
    {
        const int32 WeldId = Model->body_weldid ? Model->body_weldid[b] : b;
        if (WeldId < 0 || WeldId >= Model->nbody) continue;
        if (!Model->body_dofnum || Model->body_dofnum[WeldId] == 0) continue;

        const int32 Dof = Model->body_dofadr ? Model->body_dofadr[WeldId] : -1;
        if (Dof < 0 || Dof >= Model->nv) continue;

        const int32 Island = (Data->nisland > 0 && Data->dof_island) ? Data->dof_island[Dof] : -1;
        int32 H = (Island >= 0 && Data->island_dofadr) ? Data->island_dofadr[Island] : -1;

        // if (H == -1 && bSleepEnabled && Model->dof_treeid && Model->tree_dofadr)
        // {
        //     int32 Tree = Model->dof_treeid[Dof];
        //     // const bool bBodyAwake = Data->body_awake ? (Data->body_awake[b] != 0) : true;
        //     // if (!bBodyAwake && Data->tree_asleep && Model->ntree > 0 &&
        //     //     Tree >= 0 && Tree < Model->ntree)
        //     // {
        //     //     // Reimplementation of MuJoCo's mj_sleepCycle (engine_sleep.c).
        //     //     int32 Smallest = Tree;
        //     //     int32 Current = Tree;
        //     //     // for (int32 Count = 0; Count <= Model->ntree; ++Count)
        //     //     // {
        //     //     //     const int32 Next = Data->tree_asleep[Current];
        //     //     //     if (Next < 0 || Next >= Model->ntree) { Smallest = -1; break; }
        //     //     //     if (Next < Smallest) Smallest = Next;
        //     //     //     Current = Next;
        //     //     //     if (Current == Tree) break;
        //     //     // }
        //     //     Tree = Smallest;
        //     // }
        //     // if (Tree >= 0 && Tree < Model->ntree)
        //     // {
        //     //     H = Model->tree_dofadr[Tree];
        //     // }
        // }

        DebugData.BodyIslandSeed[b] = H;
    }

    // Tendon wrap points — mirror MuJoCo's own renderer (engine_vis_visualize.c
    // addSpatialTendonGeoms). It iterates wrap-point index j from ten_wrapadr[t]
    // to ...+ten_wrapnum[t]-1 and connects wrap_xpos[3*j] -> wrap_xpos[3*j+3],
    // skipping pulleys (wrap_obj == -2). Declared layout is `nwrap x 6` so we
    // snapshot 2*nwrap 3D points.
    const int32 NumWrapPoints = 2 * Model->nwrap;
    DebugData.WrapPointsFlat.SetNumUninitialized(NumWrapPoints);
    if (Data->wrap_xpos && Model->nwrap > 0)
    {
        for (int32 p = 0; p < NumWrapPoints; ++p)
        {
            DebugData.WrapPointsFlat[p] = MjUtils::MjToUEPosition(Data->wrap_xpos + p * 3);
        }
    }

    const int32 NumWrapObj = 2 * Model->nwrap;
    DebugData.WrapObj.SetNumUninitialized(NumWrapObj);
    if (Data->wrap_obj && Model->nwrap > 0)
    {
        for (int32 i = 0; i < NumWrapObj; ++i) DebugData.WrapObj[i] = Data->wrap_obj[i];
    }

    DebugData.TendonWrapAdr.SetNumUninitialized(Model->ntendon);
    DebugData.TendonWrapNum.SetNumUninitialized(Model->ntendon);
    DebugData.TendonLength.SetNumUninitialized(Model->ntendon);
    DebugData.TendonLimited.SetNumUninitialized(Model->ntendon);
    DebugData.TendonRangeLo.SetNumUninitialized(Model->ntendon);
    DebugData.TendonRangeHi.SetNumUninitialized(Model->ntendon);
    DebugData.TendonActivation.Init(-1.0f, Model->ntendon);
    for (int32 t = 0; t < Model->ntendon; ++t)
    {
        DebugData.TendonWrapAdr[t]  = Data->ten_wrapadr ? Data->ten_wrapadr[t] : 0;
        DebugData.TendonWrapNum[t]  = Data->ten_wrapnum ? Data->ten_wrapnum[t] : 0;
        DebugData.TendonLength[t]   = Data->ten_length ? (float)Data->ten_length[t] : 0.0f;
        DebugData.TendonLimited[t]  = Model->tendon_limited ? Model->tendon_limited[t] : 0;
        DebugData.TendonRangeLo[t]  = Model->tendon_range ? (float)Model->tendon_range[t * 2 + 0] : 0.0f;
        DebugData.TendonRangeHi[t]  = Model->tendon_range ? (float)Model->tendon_range[t * 2 + 1] : 0.0f;
    }

    // Resolve muscle activation per tendon by scanning actuators. Muscles drive
    // tendons via trntype == TENDON; act[actadr] holds activation in [0, 1].
    for (int32 a = 0; a < Model->nu; ++a)
    {
        if (Model->actuator_trntype[a] != mjTRN_TENDON) continue;
        if (Model->actuator_dyntype[a] != mjDYN_MUSCLE) continue;
        const int32 TargetTendon = Model->actuator_trnid[a * 2];
        const int32 ActAdr = Model->actuator_actadr ? Model->actuator_actadr[a] : -1;
        if (TargetTendon < 0 || TargetTendon >= Model->ntendon) continue;
        if (ActAdr < 0 || !Data->act) continue;
        DebugData.TendonActivation[TargetTendon] = FMath::Clamp((float)Data->act[ActAdr], 0.0f, 1.0f);
    }

    DebugData.GeomXPos.SetNumUninitialized(Model->ngeom);
    for (int32 g = 0; g < Model->ngeom; ++g)
    {
        DebugData.GeomXPos[g] = Data->geom_xpos
            ? MjUtils::MjToUEPosition(Data->geom_xpos + g * 3)
            : FVector::ZeroVector;
    }
}

void UMjDebugVisualizer::UpdateAllGlobalVisibility()
{
    AAMjManager* Manager = Cast<AAMjManager>(GetOwner());
    if (!Manager) return;

    for (AMjArticulation* Art : Manager->GetAllArticulations())
    {
        if (Art)
        {
            Art->bDrawDebugCollision = bGlobalDrawDebugCollision;
            Art->bDrawDebugJoints = bGlobalDrawDebugJoints;
            Art->bShowGroup3 = bGlobalShowGroup3;
            Art->UpdateGroup3Visibility();
        }
    }

    for (UMjQuickConvertComponent* QC : Manager->GetAllQuickComponents())
    {
        if (QC)
        {
            QC->m_debug_meshes = bGlobalQuickConvertCollision;
        }
    }
}

void UMjDebugVisualizer::ToggleDebugContacts()
{
    bShowDebug = !bShowDebug;
    UE_LOG(LogURLab, Log, TEXT("Debug contacts: %s"), bShowDebug ? TEXT("ON") : TEXT("OFF"));
}

void UMjDebugVisualizer::ToggleArticulationCollisions()
{
    bGlobalDrawDebugCollision = !bGlobalDrawDebugCollision;

    AAMjManager* Manager = Cast<AAMjManager>(GetOwner());
    if (!Manager) return;

    for (AMjArticulation* Art : Manager->GetAllArticulations())
    {
        if (Art)
        {
            Art->bDrawDebugCollision = bGlobalDrawDebugCollision;
        }
    }
    UE_LOG(LogURLab, Log, TEXT("Articulation collisions: %s"), bGlobalDrawDebugCollision ? TEXT("ON") : TEXT("OFF"));
}

void UMjDebugVisualizer::ToggleQuickConvertCollisions()
{
    bGlobalQuickConvertCollision = !bGlobalQuickConvertCollision;

    AAMjManager* Manager = Cast<AAMjManager>(GetOwner());
    if (!Manager) return;

    for (UMjQuickConvertComponent* QC : Manager->GetAllQuickComponents())
    {
        if (QC)
        {
            QC->m_debug_meshes = bGlobalQuickConvertCollision;
        }
    }
    UE_LOG(LogURLab, Log, TEXT("QuickConvert collisions: %s"), bGlobalQuickConvertCollision ? TEXT("ON") : TEXT("OFF"));
}

void UMjDebugVisualizer::ToggleDebugJoints()
{
    bGlobalDrawDebugJoints = !bGlobalDrawDebugJoints;

    AAMjManager* Manager = Cast<AAMjManager>(GetOwner());
    if (!Manager) return;

    for (AMjArticulation* Art : Manager->GetAllArticulations())
    {
        if (Art)
        {
            Art->bDrawDebugJoints = bGlobalDrawDebugJoints;
        }
    }
    UE_LOG(LogURLab, Log, TEXT("Debug joints: %s"), bGlobalDrawDebugJoints ? TEXT("ON") : TEXT("OFF"));
}

void UMjDebugVisualizer::ToggleVisuals()
{
    bVisualsHidden = !bVisualsHidden;

    AAMjManager* Manager = Cast<AAMjManager>(GetOwner());
    if (!Manager) return;

    for (AMjArticulation* Art : Manager->GetAllArticulations())
    {
        if (!Art) continue;
        TArray<UStaticMeshComponent*> MeshComps;
        Art->GetComponents<UStaticMeshComponent>(MeshComps);
        for (UStaticMeshComponent* SMC : MeshComps)
        {
            SMC->SetVisibility(!bVisualsHidden);
        }
    }
    UE_LOG(LogURLab, Log, TEXT("Visuals: %s"), bVisualsHidden ? TEXT("HIDDEN") : TEXT("VISIBLE"));
}

void UMjDebugVisualizer::CycleDebugShaderMode()
{
    const uint8 Count = static_cast<uint8>(EMjDebugShaderMode::SemanticSegmentation) + 1;
    DebugShaderMode = static_cast<EMjDebugShaderMode>((static_cast<uint8>(DebugShaderMode) + 1) % Count);

    const TCHAR* Label = TEXT("Off");
    switch (DebugShaderMode)
    {
        case EMjDebugShaderMode::Island:                Label = TEXT("Island"); break;
        case EMjDebugShaderMode::InstanceSegmentation:  Label = TEXT("Instance Segmentation"); break;
        case EMjDebugShaderMode::SemanticSegmentation:  Label = TEXT("Semantic Segmentation"); break;
        default: break;
    }
    UE_LOG(LogURLab, Log, TEXT("Debug shader mode: %s"), Label);
}

void UMjDebugVisualizer::ToggleTendons()
{
    bGlobalDrawTendons = !bGlobalDrawTendons;
    UE_LOG(LogURLab, Log, TEXT("Tendon rendering: %s"), bGlobalDrawTendons ? TEXT("ON") : TEXT("OFF"));
}

void UMjDebugVisualizer::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    ClearBodyOverlays();
    ClearTendonTubes();
    Super::EndPlay(EndPlayReason);
}

void UMjDebugVisualizer::ClearBodyOverlays()
{
    for (auto& Pair : OriginalMaterials)
    {
        UStaticMeshComponent* Mesh = Pair.Key.Get();
        if (!Mesh) continue;
        Mesh->SetMaterial(0, Pair.Value);

        if (const TMap<int32, UMaterialInterface*>* Extra = OriginalSlotMaterials.Find(Pair.Key))
        {
            for (const auto& SlotPair : *Extra)
            {
                Mesh->SetMaterial(SlotPair.Key, SlotPair.Value);
            }
        }
    }
    OriginalMaterials.Reset();
    OriginalSlotMaterials.Reset();
    ActiveMIDs.Reset();
}

void UMjDebugVisualizer::UpdateBodyOverlays()
{
    if (DebugShaderMode == EMjDebugShaderMode::Off)
    {
        if (OriginalMaterials.Num() > 0) ClearBodyOverlays();
        return;
    }

    if (!OverlayParentMaterial || OverlayColorParam.IsNone()) return;

    AAMjManager* Manager = Cast<AAMjManager>(GetOwner());
    if (!Manager) return;

    TArray<int32> LocalAwake;
    TArray<int32> LocalIslandSeed;
    {
        FScopeLock Lock(&DebugMutex);
        LocalAwake = DebugData.BodyAwake;
        LocalIslandSeed = DebugData.BodyIslandSeed;
    }

    auto ApplyToMesh = [&](UStaticMeshComponent* Mesh, int32 BodyId, uint32 GroupHash)
    {
        if (!Mesh) return;

        const bool bAwake =
            (LocalAwake.IsValidIndex(BodyId) ? LocalAwake[BodyId] != 0 : true);
        const int32 Seed =
            (LocalIslandSeed.IsValidIndex(BodyId) ? LocalIslandSeed[BodyId] : -1);

        TWeakObjectPtr<UStaticMeshComponent> WeakMesh(Mesh);
        const int32 NumSlots = FMath::Max(1, Mesh->GetNumMaterials());

        if (!OriginalMaterials.Contains(WeakMesh))
        {
            OriginalMaterials.Add(WeakMesh, Mesh->GetMaterial(0));
            for (int32 SlotIdx = 1; SlotIdx < NumSlots; ++SlotIdx)
            {
                OriginalSlotMaterials.FindOrAdd(WeakMesh).Add(SlotIdx, Mesh->GetMaterial(SlotIdx));
            }
        }

        const bool bColourAsAwake = bAwake || !bModulateBySleep;
        FLinearColor Color;
        switch (DebugShaderMode)
        {
            case EMjDebugShaderMode::Island:
                Color = MjColor::IslandColor(Seed, bColourAsAwake,
                    SleepValueScale, SleepSaturationScale);
                break;
            case EMjDebugShaderMode::InstanceSegmentation:
                Color = MjColor::InstanceSegmentationColor(GroupHash, BodyId, bColourAsAwake,
                    SleepValueScale, SleepSaturationScale);
                break;
            case EMjDebugShaderMode::SemanticSegmentation:
                Color = MjColor::SemanticSegmentationColor(GroupHash, bColourAsAwake,
                    SleepValueScale, SleepSaturationScale);
                break;
            default:
                return;
        }

        UMaterialInstanceDynamic* MID = nullptr;
        if (UMaterialInstanceDynamic** Existing = ActiveMIDs.Find(WeakMesh))
        {
            MID = *Existing;
        }
        if (!MID)
        {
            MID = UMaterialInstanceDynamic::Create(OverlayParentMaterial, this);
            ActiveMIDs.Add(WeakMesh, MID);
        }

        for (int32 SlotIdx = 0; SlotIdx < NumSlots; ++SlotIdx)
        {
            if (Mesh->GetMaterial(SlotIdx) != MID)
            {
                Mesh->SetMaterial(SlotIdx, MID);
            }
        }

        MID->SetVectorParameterValue(OverlayColorParam, Color);
    };

    for (AMjArticulation* Art : Manager->GetAllArticulations())
    {
        if (!Art) continue;

        // Semantic grouping hashes the Blueprint class so two instances share colour.
        const uint32 ArtHash = GetTypeHash(Art->GetClass()->GetFName());

        TArray<UMjGeom*> Geoms;
        Art->GetRuntimeComponents<UMjGeom>(Geoms);
        for (UMjGeom* Geom : Geoms)
        {
            if (!Geom || !Geom->IsBound()) continue;
            const int32 BodyId = Geom->GetMj().body_id;

            ApplyToMesh(Geom->GetVisualizerMesh(), BodyId, ArtHash);

            TArray<USceneComponent*> Children;
            Geom->GetChildrenComponents(true, Children);
            for (USceneComponent* Child : Children)
            {
                if (UStaticMeshComponent* SMC = Cast<UStaticMeshComponent>(Child))
                {
                    ApplyToMesh(SMC, BodyId, ArtHash);
                }
            }
        }
    }

    for (UMjQuickConvertComponent* QC : Manager->GetAllQuickComponents())
    {
        if (!QC) continue;
        const int32 BodyId = QC->GetMjBodyId();
        if (BodyId < 0) continue;

        AActor* Owner = QC->GetOwner();
        if (!Owner) continue;

        TArray<UStaticMeshComponent*> MeshComps;
        Owner->GetComponents<UStaticMeshComponent>(MeshComps);

        // Semantic grouping hashes the first static mesh so props sharing a mesh read as one "type".
        uint32 GroupHash = GetTypeHash(Owner->GetClass()->GetFName());
        for (UStaticMeshComponent* SMC : MeshComps)
        {
            if (SMC && SMC->GetStaticMesh())
            {
                GroupHash = GetTypeHash(SMC->GetStaticMesh()->GetFName());
                break;
            }
        }

        for (UStaticMeshComponent* SMC : MeshComps)
        {
            ApplyToMesh(SMC, BodyId, GroupHash);
        }
    }
}

// ---------------------------------------------------------------------------
// Per-camera segmentation pool
// ---------------------------------------------------------------------------

TArray<UObject*>* UMjDebugVisualizer::GetSegPoolArray(EMjCameraMode Mode)
{
    switch (Mode)
    {
    case EMjCameraMode::InstanceSegmentation: return &InstanceSegSiblings;
    case EMjCameraMode::SemanticSegmentation: return &SemanticSegSiblings;
    default:                                  return nullptr;
    }
}

TSet<TWeakObjectPtr<UMjCamera>>* UMjDebugVisualizer::GetSegSubscribers(EMjCameraMode Mode)
{
    switch (Mode)
    {
    case EMjCameraMode::InstanceSegmentation: return &InstanceSegSubscribers;
    case EMjCameraMode::SemanticSegmentation: return &SemanticSegSubscribers;
    default:                                  return nullptr;
    }
}

UStaticMeshComponent* UMjDebugVisualizer::SpawnSegSibling(
    UStaticMeshComponent* Original, int32 BodyId, uint32 GroupHash, EMjCameraMode Mode)
{
    if (!Original || !Original->GetStaticMesh()) return nullptr;
    if (!OverlayParentMaterial || OverlayColorParam.IsNone()) return nullptr;

    AActor* Owner = Original->GetOwner();
    if (!Owner) return nullptr;

    UStaticMeshComponent* Sibling = NewObject<UStaticMeshComponent>(Owner);
    Sibling->SetStaticMesh(Original->GetStaticMesh());

    // Attach to the same parent as the original so it inherits body transforms
    // for free — no per-tick sync needed.
    if (USceneComponent* Parent = Original->GetAttachParent())
    {
        Sibling->SetupAttachment(Parent);
    }
    Sibling->SetRelativeTransform(Original->GetRelativeTransform());

    // Isolation: siblings must not contribute indirect lighting, shadows, or
    // reflections to other views. bVisibleInSceneCaptureOnly hides the primitive
    // from the main viewport; the rest prevents secondary lighting/reflection
    // passes from picking it up (source of the "faint tinge" in viewport
    // otherwise). Leave bRenderInMainPass at default true — the seg capture's
    // own rendering uses the main pass.
    // Sibling->bVisibleInSceneCaptureOnly         = true;
    Sibling->SetCastShadow(false);
    Sibling->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Sibling->SetGenerateOverlapEvents(false);
    Sibling->bAffectDynamicIndirectLighting     = false;
    Sibling->bAffectDistanceFieldLighting       = false;
    Sibling->bVisibleInReflectionCaptures       = false;
    Sibling->bVisibleInRealTimeSkyCaptures      = false;
    Sibling->bVisibleInRayTracing               = false;
    Sibling->bReceivesDecals                    = false;

    // Unlit tint material — parented on the same material the viewport overlay uses.
    // Seg cameras set CaptureSource = SCS_BaseColor, which bypasses lighting so the
    // tint value lands in the RT unmodified.
    UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(OverlayParentMaterial, Sibling);
    const FLinearColor Tint = (Mode == EMjCameraMode::SemanticSegmentation)
        ? MjColor::SemanticSegmentationColor(GroupHash, /*bAwake=*/true, /*SleepValueScale=*/1.0f, /*SleepSatScale=*/1.0f)
        : MjColor::InstanceSegmentationColor(GroupHash, BodyId, /*bAwake=*/true, /*SleepValueScale=*/1.0f, /*SleepSatScale=*/1.0f);
    MID->SetVectorParameterValue(OverlayColorParam, Tint);

    const int32 NumSlots = FMath::Max(1, Original->GetNumMaterials());
    for (int32 Slot = 0; Slot < NumSlots; ++Slot)
    {
        Sibling->SetMaterial(Slot, MID);
    }

    Sibling->ComponentTags.Add(Mode == EMjCameraMode::InstanceSegmentation
        ? FName(TEXT("URLab_Seg_Instance"))
        : FName(TEXT("URLab_Seg_Semantic")));

    Sibling->RegisterComponent();
    return Sibling;
}

void UMjDebugVisualizer::BuildSegPool(EMjCameraMode Mode)
{
	TArray<UObject*>* Pool = GetSegPoolArray(Mode);
    if (!Pool) return;

    AAMjManager* Manager = Cast<AAMjManager>(GetOwner());
    if (!Manager) return;

    Pool->Reset();

    auto AddSibling = [&](UStaticMeshComponent* Original, int32 BodyId, uint32 GroupHash)
    {
        if (UStaticMeshComponent* Sib = SpawnSegSibling(Original, BodyId, GroupHash, Mode))
        {
            Pool->Add(Sib);
        }
    };

    // Articulation visual meshes — walk geoms and their static-mesh children.
    for (AMjArticulation* Art : Manager->GetAllArticulations())
    {
        if (!Art) continue;
        const uint32 ArtHash = GetTypeHash(Art->GetClass()->GetFName());

        TArray<UMjGeom*> Geoms;
        Art->GetRuntimeComponents<UMjGeom>(Geoms);
        for (UMjGeom* Geom : Geoms)
        {
            if (!Geom || !Geom->IsBound()) continue;
            const int32 BodyId = Geom->GetMj().body_id;

            AddSibling(Geom->GetVisualizerMesh(), BodyId, ArtHash);

            TArray<USceneComponent*> Children;
            Geom->GetChildrenComponents(true, Children);
            for (USceneComponent* Child : Children)
            {
                if (UStaticMeshComponent* SMC = Cast<UStaticMeshComponent>(Child))
                {
                    AddSibling(SMC, BodyId, ArtHash);
                }
            }
        }
    }

    // Quick-Convert primitives — group hash keyed off the first static mesh.
    for (UMjQuickConvertComponent* QC : Manager->GetAllQuickComponents())
    {
        if (!QC) continue;
        const int32 BodyId = QC->GetMjBodyId();
        if (BodyId < 0) continue;

        AActor* Owner = QC->GetOwner();
        if (!Owner) continue;

        TArray<UStaticMeshComponent*> MeshComps;
        Owner->GetComponents<UStaticMeshComponent>(MeshComps);

        uint32 GroupHash = GetTypeHash(Owner->GetClass()->GetFName());
        for (UStaticMeshComponent* SMC : MeshComps)
        {
            if (SMC && SMC->GetStaticMesh())
            {
                GroupHash = GetTypeHash(SMC->GetStaticMesh()->GetFName());
                break;
            }
        }

        for (UStaticMeshComponent* SMC : MeshComps)
        {
            AddSibling(SMC, BodyId, GroupHash);
        }
    }

    UE_LOG(LogURLab, Log,
        TEXT("[MjDebugVisualizer] Built seg pool mode=%s size=%d"),
        *UEnum::GetValueAsString(Mode), Pool->Num());
}

void UMjDebugVisualizer::DestroySegPool(EMjCameraMode Mode)
{
	TArray<UObject*>* Pool = GetSegPoolArray(Mode);
    if (!Pool) return;

    // for (const UObject*& Sib : *Pool)
    // {
    //     // if (Sib) Sib->DestroyComponent();
    // }
    Pool->Reset();
}

void UMjDebugVisualizer::AcquireSegPool(EMjCameraMode Mode, UMjCamera* Camera,
                                        TArray<UPrimitiveComponent*>& OutSiblings)
{
    OutSiblings.Reset();

    TArray<UObject*>* Pool = GetSegPoolArray(Mode);
    TSet<TWeakObjectPtr<UMjCamera>>*          Subs = GetSegSubscribers(Mode);
    if (!Pool || !Subs) return;

    const bool bFirstSubscriber = Subs->Num() == 0;
    Subs->Add(Camera);

    if (bFirstSubscriber)
    {
        BuildSegPool(Mode);
    }

    OutSiblings.Reserve(Pool->Num());
	// for (const UObject* Sib : *Pool)
    // {
    //     if (Sib) OutSiblings.Add(Sib);
    // }
}

void UMjDebugVisualizer::ReleaseSegPool(EMjCameraMode Mode, UMjCamera* Camera)
{
    TSet<TWeakObjectPtr<UMjCamera>>* Subs = GetSegSubscribers(Mode);
    if (!Subs) return;

    Subs->Remove(Camera);
    // Also drop any stale weak pointers so refcount reflects reality.
    for (auto It = Subs->CreateIterator(); It; ++It)
    {
        if (!It->IsValid()) It.RemoveCurrent();
    }

    if (Subs->Num() == 0)
    {
        DestroySegPool(Mode);
    }
}

void UMjDebugVisualizer::GetSegPoolSiblings(EMjCameraMode Mode,
                                            TArray<UPrimitiveComponent*>& OutSiblings) const
{
    OutSiblings.Reset();
    // const TArray* Pool = nullptr;
    // switch (Mode)
    // {
    // case EMjCameraMode::InstanceSegmentation: Pool = &InstanceSegSiblings; break;
    // case EMjCameraMode::SemanticSegmentation: Pool = &SemanticSegSiblings; break;
    // default:                                  return;
    // }

    // OutSiblings.Reserve(Pool->Num());
    // for (const TObjectPtr<UStaticMeshComponent>& Sib : *Pool)
    // {
    //     if (Sib) OutSiblings.Add(Sib);
    // }
}

// ---------------------------------------------------------------------------
// Tendon rendering
// ---------------------------------------------------------------------------

void UMjDebugVisualizer::HideTendonTubes()
{
    for (USplineMeshComponent* Seg : TendonSegmentPool)
    {
        if (Seg) Seg->SetVisibility(false);
    }
}

void UMjDebugVisualizer::ClearTendonTubes()
{
    for (USplineMeshComponent* Seg : TendonSegmentPool)
    {
        if (Seg) Seg->DestroyComponent();
    }
    TendonSegmentPool.Reset();
    TendonSegmentMIDs.Reset();
}

namespace
{
    // `/Engine/BasicShapes/Cylinder.Cylinder` ships as a 100cm-tall unit cylinder
    // with 50cm base radius. USplineMeshComponent::SetStartScale multiplies the
    // cross-section (X/Y) by its arg, so to get a visible radius in cm we divide
    // by this constant. Kept local since we're the only caller.
    constexpr float kBasicCylinderBaseRadiusCm = 50.0f;

    FVector ComputeJoinTangent(const FVector& PrevDir, float PrevLen,
                               const FVector& NextDir, float NextLen)
    {
        const FVector Avg = (PrevDir + NextDir).GetSafeNormal();
        const float Scale = 0.5f * (PrevLen + NextLen);
        return Avg * Scale;
    }

    // Spherical lerp about a shared origin — exact for sphere wraps, good enough
    // for cylinder wraps where both wrap points share the same perpendicular
    // distance from the cylinder axis.
    FVector SlerpAboutCentre(const FVector& Centre, const FVector& VA, const FVector& VB, float T)
    {
        // const float LenA = VA.Length();
	    const float LenA = FMath::Sqrt(VA.X * VA.X + VA.Y * VA.Y + VA.Z * VA.Z);
        // const float LenB = VB.Length();
	    const float LenB = FMath::Sqrt(VB.X * VB.X + VB.Y * VB.Y + VB.Z * VB.Z);
        if (LenA < KINDA_SMALL_NUMBER || LenB < KINDA_SMALL_NUMBER)
        {
            return Centre + FMath::Lerp(VA, VB, T);
        }
        const FVector UA = VA / LenA;
        const FVector UB = VB / LenB;
        const float CosTheta = FMath::Clamp(FVector::DotProduct(UA, UB), -1.0f, 1.0f);
        const float Theta = FMath::Acos(CosTheta);
        if (Theta < 1e-3f)
        {
            return Centre + FMath::Lerp(VA, VB, T);
        }
        const float Radius = FMath::Lerp(LenA, LenB, T);
        const float SinTheta = FMath::Sin(Theta);
        const float WA = FMath::Sin((1.0f - T) * Theta) / SinTheta;
        const float WB = FMath::Sin(T * Theta) / SinTheta;
        return Centre + (UA * WA + UB * WB) * Radius;
    }
}

void UMjDebugVisualizer::UpdateTendonTubes()
{
    AActor* Owner = GetOwner();
    if (!Owner || !TendonTubeMesh || !OverlayParentMaterial || OverlayColorParam.IsNone())
    {
        HideTendonTubes();
        return;
    }

    TArray<FVector> LocalPoints;
    TArray<int32> LocalWrapObj, LocalAdr, LocalNum;
    TArray<float> LocalLengths, LocalLo, LocalHi, LocalActivation;
    TArray<uint8> LocalLimited;
    TArray<FVector> LocalGeomPos;
    {
        FScopeLock Lock(&DebugMutex);
        LocalPoints = DebugData.WrapPointsFlat;
        LocalWrapObj = DebugData.WrapObj;
        LocalAdr = DebugData.TendonWrapAdr;
        LocalNum = DebugData.TendonWrapNum;
        LocalLengths = DebugData.TendonLength;
        LocalLimited = DebugData.TendonLimited;
        LocalLo = DebugData.TendonRangeLo;
        LocalHi = DebugData.TendonRangeHi;
        LocalActivation = DebugData.TendonActivation;
        LocalGeomPos = DebugData.GeomXPos;
    }

    struct FTubeSeg
    {
        FVector Start;
        FVector End;
        FVector StartTangent;
        FVector EndTangent;
        FLinearColor Colour;
        float Radius;     // visible radius in cm
    };
    TArray<FTubeSeg> Segs;
    Segs.Reserve(LocalPoints.Num() * 2);

    const int32 NumTendons = LocalAdr.Num();
    for (int32 t = 0; t < NumTendons; ++t)
    {
        // Intensity: activation for muscles, length-vs-range stretch for limited tendons, else neutral.
        float Intensity = 0.5f;
        const float Act = LocalActivation.IsValidIndex(t) ? LocalActivation[t] : -1.0f;
        if (Act >= 0.0f)
        {
            Intensity = Act;
        }
        else if (LocalLimited.IsValidIndex(t) && LocalLimited[t])
        {
            const float Lo = LocalLo[t];
            const float Hi = LocalHi[t];
            if (Hi > Lo + KINDA_SMALL_NUMBER)
            {
                Intensity = FMath::Clamp((LocalLengths[t] - Lo) / (Hi - Lo), 0.0f, 1.0f);
            }
        }
        const FLinearColor Colour = FLinearColor::LerpUsingHSV(
            FLinearColor(0.05f, 0.05f, 0.6f), FLinearColor(1.0f, 0.15f, 0.05f), Intensity);
        // Swell more dramatically — 0.5× relaxed, 2× fully contracted (4× range).
        const float Radius = TendonTubeRadius * (0.5f + 1.5f * Intensity);

        // Build ordered path with optional arc subdivision across geom wraps.
        TArray<FVector> Path;
        Path.Reserve(LocalNum[t] * (TendonArcSubdivisions + 2));
        const int32 Adr = LocalAdr[t];
        const int32 Num = LocalNum[t];
        for (int32 j = Adr; j + 1 < Adr + Num; ++j)
        {
            if (LocalWrapObj.IsValidIndex(j)     && LocalWrapObj[j]     == -2) continue;
            if (LocalWrapObj.IsValidIndex(j + 1) && LocalWrapObj[j + 1] == -2) continue;
            if (!LocalPoints.IsValidIndex(j + 1)) continue;

            const FVector& A = LocalPoints[j];
            const FVector& B = LocalPoints[j + 1];
            if (Path.Num() == 0) Path.Add(A);

            const int32 ObjJ  = LocalWrapObj.IsValidIndex(j)     ? LocalWrapObj[j]     : -1;
            const int32 ObjJ1 = LocalWrapObj.IsValidIndex(j + 1) ? LocalWrapObj[j + 1] : -1;
            if (ObjJ >= 0 && ObjJ == ObjJ1 && LocalGeomPos.IsValidIndex(ObjJ) && TendonArcSubdivisions > 0)
            {
                const FVector& Centre = LocalGeomPos[ObjJ];
                const FVector VA = A - Centre;
                const FVector VB = B - Centre;
                for (int32 k = 1; k <= TendonArcSubdivisions; ++k)
                {
                    const float T = (float)k / (float)(TendonArcSubdivisions + 1);
                    Path.Add(SlerpAboutCentre(Centre, VA, VB, T));
                }
            }
            Path.Add(B);
        }

        if (Path.Num() < 2) continue;

        const int32 SegCount = Path.Num() - 1;
        TArray<FVector> Dirs;
        TArray<float> Lens;
        Dirs.SetNumUninitialized(SegCount);
        Lens.SetNumUninitialized(SegCount);
        for (int32 s = 0; s < SegCount; ++s)
        {
            const FVector D = Path[s + 1] - Path[s];
            // Lens[s] = D.Length();
			Lens[s] = FMath::Sqrt(D.X * D.X + D.Y * D.Y + D.Z * D.Z);
            Dirs[s] = Lens[s] > KINDA_SMALL_NUMBER ? (D / Lens[s]) : FVector::UpVector;
        }

        for (int32 s = 0; s < SegCount; ++s)
        {
            if (Lens[s] < 0.5f) continue;

            const FVector StartTangent = (s > 0)
                ? ComputeJoinTangent(Dirs[s - 1], Lens[s - 1], Dirs[s], Lens[s])
                : Dirs[s] * Lens[s];
            const FVector EndTangent = (s + 1 < SegCount)
                ? ComputeJoinTangent(Dirs[s], Lens[s], Dirs[s + 1], Lens[s + 1])
                : Dirs[s] * Lens[s];

            FTubeSeg Seg;
            Seg.Start = Path[s];
            Seg.End = Path[s + 1];
            Seg.StartTangent = StartTangent;
            Seg.EndTangent = EndTangent;
            Seg.Colour = Colour;
            Seg.Radius = Radius;
            Segs.Add(Seg);
        }
    }

    while (TendonSegmentPool.Num() < Segs.Num())
    {
        USplineMeshComponent* New = NewObject<USplineMeshComponent>(Owner, NAME_None, RF_Transient);
        if (!New) break;

        New->SetMobility(EComponentMobility::Movable);
        New->SetStaticMesh(TendonTubeMesh);
        New->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        New->SetCastShadow(false);
        New->bSelectable = false;
        New->bUseAttachParentBound = true;
        New->SetForwardAxis(ESplineMeshAxis::Z);   // /Engine/BasicShapes/Cylinder extends along Z
        New->RegisterComponent();
        New->AttachToComponent(Owner->GetRootComponent(), FAttachmentTransformRules::KeepWorldTransform);

        // Standard idiom: let the component create + own the MID so it's wired
        // correctly through the render proxy. Creating via UMaterialInstanceDynamic::Create
        // with the visualizer as outer resulted in the MID not routing to the mesh.
        UMaterialInstanceDynamic* MID = New->CreateDynamicMaterialInstance(0, OverlayParentMaterial);

        TendonSegmentPool.Add(New);
        TendonSegmentMIDs.Add(MID);
    }

    for (int32 i = 0; i < TendonSegmentPool.Num(); ++i)
    {
        USplineMeshComponent* Seg = TendonSegmentPool[i];
        if (!Seg) continue;
        if (i < Segs.Num())
        {
            const FTubeSeg& S = Segs[i];
            // Convert desired visible radius in cm to the scale factor applied
            // to the base cylinder mesh (50cm radius → divide by 50).
            const float Scale = S.Radius / kBasicCylinderBaseRadiusCm;
            Seg->SetStartScale(FVector2D(Scale, Scale), false);
            Seg->SetEndScale(FVector2D(Scale, Scale), false);
            Seg->SetStartAndEnd(S.Start, S.StartTangent, S.End, S.EndTangent, true);
            if (TendonSegmentMIDs.IsValidIndex(i) && TendonSegmentMIDs[i])
            {
                TendonSegmentMIDs[i]->SetVectorParameterValue(OverlayColorParam, S.Colour);
            }
            Seg->SetVisibility(true);
        }
        else
        {
            Seg->SetVisibility(false);
        }
    }
}

void UMjDebugVisualizer::InitializeOverlayMaterial()
{
    if (OverlayParentMaterial) return;

    UMaterialInterface* Parent = LoadObject<UMaterialInterface>(
        nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));

    if (!Parent)
    {
        UE_LOG(LogURLab, Warning,
            TEXT("Debug viz: failed to load /Engine/BasicShapes/BasicShapeMaterial — overlays disabled"));
        return;
    }

    TArray<FMaterialParameterInfo> VecInfos;
    TArray<FGuid> Guids;
    Parent->GetAllVectorParameterInfo(VecInfos, Guids);

    if (VecInfos.Num() == 0)
    {
        UE_LOG(LogURLab, Warning,
            TEXT("Debug viz: BasicShapeMaterial exposes no vector params — overlays disabled"));
        return;
    }

    // Prefer well-known colour-param names so we don't accidentally drive an emissive tint etc.
    static const FName PreferredNames[] = {
        TEXT("Color"), TEXT("BaseColor"), TEXT("Tint"), TEXT("TintColor"), TEXT("DiffuseColor")
    };
    FName Chosen = NAME_None;
    for (const FName& Pref : PreferredNames)
    {
        for (const FMaterialParameterInfo& Info : VecInfos)
        {
            if (Info.Name == Pref) { Chosen = Pref; break; }
        }
        if (!Chosen.IsNone()) break;
    }
    if (Chosen.IsNone()) Chosen = VecInfos[0].Name;

    OverlayParentMaterial = Parent;
    OverlayColorParam = Chosen;
}
