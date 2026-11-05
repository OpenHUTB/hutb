// Copyright (c) OpenHUTB. All rights reserved.
// 实现见头文件注释。

#include "Vehicles/Rov/RovMuJoCoBridge.h"

#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Components/PrimitiveComponent.h"
#include "Kismet/GameplayStatics.h"

// URLab：MuJoCo 物理
#include "MuJoCo/Core/AMjManager.h"
#include "MuJoCo/Core/MjPhysicsEngine.h"
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

    // 先尝试解析一次。注意：此时 MuJoCo 模型很可能还没编译完成，
    // GetBodies() 会是空的——没关系，ProvidePose 里会惰性重试。
    ResolveTargets();

    if (bDisablePawnPhysics) {
        DisablePawnPhysics();
    }

    // 无论是否解析成功都注册 provider：
    // 模型就绪后 ProvidePose 会自动接上，不需要重启 Pawn。
    ExternalPoseProvider = [this](FVector& OutPos, FQuat& OutRot) -> bool {
        return ProvidePose(OutPos, OutRot);
    };

    bBridgeActive = true;

    if (TargetBody.IsValid()) {
        UE_LOG(LogRovBridge, Log,
            TEXT("[RovMuJoCoBridge] ✅ 桥接已启用，跟随 body '%s'（Articulation='%s'）"),
            *GetResolvedBodyName(),
            Articulation.IsValid() ? *Articulation->GetName() : TEXT("<invalid>"));
    }
    else {
        UE_LOG(LogRovBridge, Log,
            TEXT("[RovMuJoCoBridge] 位姿源已注册，但 MuJoCo 模型尚未就绪"
                 "（body 后缀 '%s'）——将在运行中自动重试"),
            *TargetBodyNameSuffix);
    }
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

void URovMuJoCoBridge::ResolveTargets() const
{
    UWorld* World = GetWorld();
    if (!World) return;

    Articulation = Cast<AMjArticulation>(
        UGameplayStatics::GetActorOfClass(World, AMjArticulation::StaticClass()));

    if (!Articulation.IsValid()) {
        return; // 场景里还没有 articulation，静默等待
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

    // 有 articulation 但没匹配到 body：只在第一次打印候选，避免刷屏
    if (!bResolveWarned && Bodies.Num() > 0) {
        bResolveWarned = true;
        UE_LOG(LogRovBridge, Warning,
            TEXT("[RovMuJoCoBridge] 没有 body 以 '%s' 结尾。候选（%d 个）："),
            *TargetBodyNameSuffix, Bodies.Num());
        for (UMjBody* Body : Bodies) {
            if (Body) {
                UE_LOG(LogRovBridge, Warning, TEXT("    %s"), *Body->GetName());
            }
        }
    }
}

bool URovMuJoCoBridge::ProvidePose(FVector& OutPosition, FQuat& OutRotation) const
{
    // 惰性解析：MuJoCo 模型通常在 Pawn 生成之后才编译完成，
    // 所以这里每帧检查一次，一旦 body 出现就自动接上。
    if (!TargetBody.IsValid()) {
        ResolveTargets();
    }

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

    // 一次性物理诊断：确认 MuJoCo 到底有没有在步进。
    // 如果这里 IsRunning=0 或仿真时间一直是 0，说明物理循环没跑起来。
    static bool bLoggedPhysicsDiag = false;
    if (!bLoggedPhysicsDiag) {
        bLoggedPhysicsDiag = true;
        if (AAMjManager* Mgr = AAMjManager::GetManager()) {
            const double Timestep = (Mgr->PhysicsEngine && Mgr->PhysicsEngine->m_model)
                ? Mgr->PhysicsEngine->m_model->opt.timestep
                : -1.0;

            // UMjPhysicsEngine 的 bIsPaused 默认是 true（URLab 让用户在
            // MjSimulate 控件里手动点播放）。Pawn 要跟随就必须让仿真跑起来。
            if (bAutoStartSimulation && !Mgr->IsRunning()) {
                Mgr->SetPaused(false);
                UE_LOG(LogRovBridge, Log,
                    TEXT("[RovMuJoCoBridge] MuJoCo 仿真原本处于暂停状态，已自动启动"));
            }

            UE_LOG(LogRovBridge, Log,
                TEXT("[RovMuJoCoBridge] 物理诊断: IsRunning=%d  IsInitialized=%d  "
                     "SimTime=%.4fs  timestep=%.4fs  SimSpeed=%.0f%%"),
                Mgr->IsRunning() ? 1 : 0,
                Mgr->IsInitialized() ? 1 : 0,
                Mgr->GetSimTime(),
                Timestep,
                Mgr->PhysicsEngine ? Mgr->PhysicsEngine->SimSpeedPercent : -1.0f);
        }
    }

    // 节流调试日志：每 60 次调用打印一次 MuJoCo 位姿 vs Pawn 实际位姿，
    // 用来确认「Pawn 真的跟着 MuJoCo 走」。设 LogRovBridge 为 Log 级别即可看到。
    static int32 CallCounter = 0;
    if (++CallCounter % 60 == 0) {
        const AActor* Owner = GetOwner();
        const FVector PawnPos = Owner ? Owner->GetActorLocation() : FVector::ZeroVector;
        AAMjManager* Mgr = AAMjManager::GetManager();
        const float SimTime = Mgr ? Mgr->GetSimTime() : -1.0f;
        UE_LOG(LogRovBridge, Log,
            TEXT("[RovMuJoCoBridge] 跟随中 #%d  仿真时间=%.3fs  MuJoCo=(%s)  Pawn=(%s)  差值=%.2f cm"),
            CallCounter,
            SimTime,
            *OutPosition.ToString(),
            *PawnPos.ToString(),
            FVector::Dist(OutPosition, PawnPos));
    }

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
