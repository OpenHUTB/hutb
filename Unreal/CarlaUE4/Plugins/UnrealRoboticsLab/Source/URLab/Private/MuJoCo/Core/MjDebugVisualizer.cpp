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

        // 正常：接触框的第一行，针对UE坐标习惯进行了Y轴翻转
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

    // 每个物体的 Halton 种子，匹配 MuJoCo 在 engine_vis_visualize.c 中的原生 islandColor 算法：
    // 如果可用的话，使用活动约束岛屿的自由度地址，否则回退到运动学树的自由度地址，这样静止的物体可以保持稳定的颜色。
    // 休眠时，mj_sleepCycle 会将连接的休眠循环树折叠到最小索引，这样一组彼此依靠的物体会共享同一种颜色。
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

    // 肌腱包裹点——镜像 MuJoCo 自带的渲染器（engine_vis_visualize.c 中的 addSpatialTendonGeoms）。
    // 它会迭代绕点索引 j，从 ten_wrapadr[t] 到… ten_wrapnum[t]-1，
    // 并连接 wrap_xpos[3*j] -> wrap_xpos[3*j+3]，跳过滑轮（wrap_obj == -2）。
    // 声明的布局是 `nwrap x 6`，所以我们会快照 2*nwrap 个三维点。
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

    // 通过扫描执行器来解析每条肌腱的肌肉激活。
    // 肌肉通过 trntype == TENDON 驱动肌腱；act[actadr] 的激活值在 [0, 1] 之间。
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

        // 语义分组会对蓝图类进行哈希处理，这样两个实例就可以共享颜色。
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

        // 语义分组会对第一个静态网格进行哈希处理，所以共享同一网格的道具会被视作同一“类型”。
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
// 每个相机的分割池
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


// 为给定的原始 UStaticMeshComponent 创建一个“兄弟”静态网格组件（sibling），用于分割相机（instance/semantic segmentation）渲染。
// 这个兄弟组件使用单一无光照的动态材质（带指定色彩），并附加到与原件相同的父组件以自动继承变换，从而无需每帧同步位置。
UStaticMeshComponent* UMjDebugVisualizer::SpawnSegSibling(
    UStaticMeshComponent* Original, int32 BodyId, uint32 GroupHash, EMjCameraMode Mode)
{
    if (!Original || !Original->GetStaticMesh()) return nullptr;
    if (!OverlayParentMaterial || OverlayColorParam.IsNone()) return nullptr;

    AActor* Owner = Original->GetOwner();
    if (!Owner) return nullptr;

    UStaticMeshComponent* Sibling = NewObject<UStaticMeshComponent>(Owner);
    Sibling->SetStaticMesh(Original->GetStaticMesh());

    // 附加到与原始对象相同的父对象，这样它就能随意继承身体变换——不需要每帧同步。
    if (USceneComponent* Parent = Original->GetAttachParent())
    {
        Sibling->SetupAttachment(Parent);
    }
    Sibling->SetRelativeTransform(Original->GetRelativeTransform());

    // 隔离：兄弟节点不能向其他视图贡献间接光照、阴影或反射。
    // bVisibleInSceneCaptureOnly 会把这个原件从主视口隐藏；
    // 其他设置则防止次要光照/反射通道捕捉它（否则会在视口里出现“淡淡的色调”）。
    // 保持 bRenderInMainPass 默认 true ——分割捕捉自己的渲染会使用主通道。
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

    // 未点亮的色调材质 —— 归属于视口叠加使用的同一个材质。
    // 分割摄像机将 CaptureSource 设置为 SCS_BaseColor，这样可以绕过光照，让色调值未经过修改就直接进入渲染目标。
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


// 为指定的分割相机模式（实例或语义分割）构建一组“兄弟”静态网格组件池（seg pool）。
// 这些兄弟组件是原始可视网格的轻量替代，仅用于分割相机渲染（单色编码），避免每帧单独同步变换。
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

    // 铰链可视网格——行走几何体及其静态网格子级。
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

    // 快速转换原语——基于第一个静态网格的分组哈希。
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
    // 同时丢掉任何过时的弱指针，这样引用计数才反映现实。
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
// 肌腱渲染
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
    // `/Engine/BasicShapes/Cylinder.Cylinder` 出厂时是一个高 100 厘米、底面半径 50 厘米的单位圆柱。
    // USplineMeshComponent::SetStartScale 会将横截面（X/Y）乘以它的参数，所以要得到以厘米为单位的可见半径，我们需要除以这个常数。
    // 保留为本地变量，因为只有我们一个调用者。
    constexpr float kBasicCylinderBaseRadiusCm = 50.0f;

    FVector ComputeJoinTangent(const FVector& PrevDir, float PrevLen,
                               const FVector& NextDir, float NextLen)
    {
        const FVector Avg = (PrevDir + NextDir).GetSafeNormal();
        const float Scale = 0.5f * (PrevLen + NextLen);
        return Avg * Scale;
    }

    // 围绕共同原点的球面插值——对于球面包裹是精确的，对于圆柱包裹也足够好，只要两个包裹点与圆柱轴的垂直距离相同。
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


// 根据 CaptureDebugData() 从 MuJoCo 快照得到的包裹点、激活和长度数据，
// 构建/更新一组 USplineMeshComponent，以在 Unreal 中以带颜色与可变半径的“管状”方式可视化肌腱（tendon）路径。
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
        float Radius;     // 可见半径（厘米）
    };
    TArray<FTubeSeg> Segs;
    Segs.Reserve(LocalPoints.Num() * 2);

    const int32 NumTendons = LocalAdr.Num();
    for (int32 t = 0; t < NumTendons; ++t)
    {
        // 强度：肌肉激活，有限肌腱时做 长度-范围 拉伸，其它保持中性。
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
        // 膨胀得更明显——放松时为 0.5×，完全收缩时为 2×（范围 4×）。
        const float Radius = TendonTubeRadius * (0.5f + 1.5f * Intensity);

        // 在几何包裹中构建有序路径，并可选择弧分割。
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
        New->SetForwardAxis(ESplineMeshAxis::Z);   // /Engine/BasicShapes/Cylinder 沿 Z 轴延伸
        New->RegisterComponent();
        New->AttachToComponent(Owner->GetRootComponent(), FAttachmentTransformRules::KeepWorldTransform);

        // 标准习语：让组件自己创建“动态材质实例”（Material Instance Dynamic, MID），这样它才会通过渲染代理正确连接。
        // 通过 UMaterialInstanceDynamic::Create 用可视化器作为外部创建，会导致 MID 没有正确路由到网格上。
        // 补充：MID是基于某个父材质（UMaterial 或 UMaterialInstance）在运行时创建的材质实例，允许通过代码实时修改材质参数（颜色、标量、贴图等）
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
            // 将想要的可见半径（厘米）转换为应用于基础圆柱网格的缩放因子（50厘米半径 → 除以50）。
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


// 初始化用于调试覆盖（overlay）和分割渲染的父材质与颜色参数名。
// 它确保后续创建的动态材质实例（MID）有一个已知的父材质和可写的向量参数（用于设置颜色）。
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

    // 尽量使用知名的颜色参数名字，这样我们就不会意外地弄出自发光色调之类的东西。
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
