// Copyright (c) OpenHUTB. All rights reserved.
// 实现见头文件注释。

#include "Vehicles/Rov/RovMuJoCoBridge.h"

#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Components/PrimitiveComponent.h"
#include "Kismet/GameplayStatics.h"

// URLab：MuJoCo 物理
#include "MuJoCo/Core/MjArticulation.h"
#include "MuJoCo/Components/Bodies/MjBody.h"

DEFINE_LOG_CATEGORY_STATIC(LogRovBridge, Log, All);

std::function<bool(FVector&, FQuat&)> URovMuJoCoBridge::ExternalPoseProvider;

URovMuJoCoBridge::URovMuJoCoBridge()
{
    PrimaryComponentTick.bCanEverTick = false; // 按需取值，不需要自己 tick
}

void URovMuJoCoBridge::BeginPlay()
{
    Super::BeginPlay();

    if (!bEnableBridge) {
        UE_LOG(LogRovBridge, Log,
            TEXT("[RovMuJoCoBridge] 已禁用（bEnableBridge=false），AirSim 保持原生物理"));
        return;
    }

    ResolveTargets();

    if (!TargetBody.IsValid()) {
        UE_LOG(LogRovBridge, Warning,
            TEXT("[RovMuJoCoBridge] 未找到后缀为 '%s' 的 MuJoCo body，桥接未启用。"
                 "请确认场景里存在 AMjArticulation 且模型已编译。"),
            *TargetBodyNameSuffix);
        return;
    }

    if (bDisablePawnPhysics) {
        DisablePawnPhysics();
    }

    // 注册全局位姿源。用 TWeakObjectPtr 语义保证组件销毁后不再被调用。
    ExternalPoseProvider = [this](FVector& OutPos, FQuat& OutRot) -> bool {
        return ProvidePose(OutPos, OutRot);
    };

    bBridgeActive = true;

    UE_LOG(LogRovBridge, Log,
        TEXT("[RovMuJoCoBridge] ✅ 桥接已启用，跟随 body '%s'（Articulation='%s'）"),
        *GetResolvedBodyName(),
        Articulation.IsValid() ? *Articulation->GetName() : TEXT("<invalid>"));
}

void URovMuJoCoBridge::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (bBridgeActive) {
        ExternalPoseProvider = nullptr;
        bBridgeActive = false;
        UE_LOG(LogRovBridge, Log, TEXT("[RovMuJoCoBridge] 桥接已解除"));
    }
    Super::EndPlay(EndPlayReason);
}

void URovMuJoCoBridge::ResolveTargets()
{
    UWorld* World = GetWorld();
    if (!World) return;

    Articulation = Cast<AMjArticulation>(
        UGameplayStatics::GetActorOfClass(World, AMjArticulation::StaticClass()));

    if (!Articulation.IsValid()) {
        UE_LOG(LogRovBridge, Warning,
            TEXT("[RovMuJoCoBridge] 场景里没有 AMjArticulation"));
        return;
    }

    // URLab 会给 spec 元素名加蓝图前缀，所以用后缀匹配
    TArray<UMjBody*> Bodies = Articulation->GetBodies();
    for (UMjBody* Body : Bodies) {
        if (!Body) continue;
        const FString Name = Body->GetName();
        if (Name.EndsWith(TargetBodyNameSuffix, ESearchCase::IgnoreCase)) {
            TargetBody = Body;
            UE_LOG(LogRovBridge, Log,
                TEXT("[RovMuJoCoBridge] 匹配到 body: '%s'（后缀 '%s'）"),
                *Name, *TargetBodyNameSuffix);
            return;
        }
    }

    // 没匹配上时，把候选名字打出来方便排查
    UE_LOG(LogRovBridge, Warning,
        TEXT("[RovMuJoCoBridge] 没有 body 以 '%s' 结尾。候选："), *TargetBodyNameSuffix);
    for (UMjBody* Body : Bodies) {
        if (Body) {
            UE_LOG(LogRovBridge, Warning, TEXT("    %s"), *Body->GetName());
        }
    }
}

bool URovMuJoCoBridge::ProvidePose(FVector& OutPosition, FQuat& OutRotation) const
{
    if (!TargetBody.IsValid()) {
        return false;
    }

    // 用绑定状态判断「是否就绪」，而不是靠位姿是否为零——
    // body 真的待在原点时，零位姿是合法值，不能当成未就绪。
    if (!TargetBody->IsBound()) {
        return false;
    }

    // 注意：这两个函数直接读 MuJoCo 的 xpos / xquat 数组，
    // 不依赖 UMjBody::TickComponent 是否已经执行，因此不存在「差一帧」问题。
    OutPosition = TargetBody->GetWorldPosition();
    OutRotation = TargetBody->GetWorldRotation();

    return true;
}

void URovMuJoCoBridge::DisablePawnPhysics()
{
    AActor* Owner = GetOwner();
    if (!Owner) return;

    int32 DisabledPrimitives = 0;

    TArray<UPrimitiveComponent*> Primitives;
    Owner->GetComponents<UPrimitiveComponent>(Primitives);

    for (UPrimitiveComponent* Prim : Primitives) {
        if (!Prim) continue;

        if (Prim->IsSimulatingPhysics()) {
            Prim->SetSimulatePhysics(false);
        }
        // 关掉碰撞：MuJoCo 已经负责碰撞，UE 侧再算一次会双计。
        if (Prim->GetCollisionEnabled() != ECollisionEnabled::NoCollision) {
            Prim->SetCollisionEnabled(ECollisionEnabled::NoCollision);
            ++DisabledPrimitives;
        }
    }

    UE_LOG(LogRovBridge, Log,
        TEXT("[RovMuJoCoBridge] 已关闭 Pawn '%s' 的物理与碰撞（%d 个图元），"
             "避免与 MuJoCo 双计"),
        *Owner->GetName(), DisabledPrimitives);
}

FString URovMuJoCoBridge::GetResolvedBodyName() const
{
    return TargetBody.IsValid() ? TargetBody->GetName() : FString();
}
