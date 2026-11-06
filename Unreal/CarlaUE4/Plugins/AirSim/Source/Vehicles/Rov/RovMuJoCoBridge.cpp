// Copyright (c) OpenHUTB. All rights reserved.
// 实现见头文件注释。

#include "Vehicles/Rov/RovMuJoCoBridge.h"

#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameFramework/PlayerController.h"
#include "Components/PrimitiveComponent.h"
#include "Kismet/GameplayStatics.h"
#include "InputCoreTypes.h"

// URLab：MuJoCo 物理
#include "MuJoCo/Core/AMjManager.h"
#include "MuJoCo/Core/MjPhysicsEngine.h"
#include "MuJoCo/Core/MjArticulation.h"
#include "MuJoCo/Components/Bodies/MjBody.h"

DEFINE_LOG_CATEGORY_STATIC(LogRovBridge, Log, All);

std::function<bool(FVector&, FQuat&)> URovMuJoCoBridge::ExternalPoseProvider;

URovMuJoCoBridge::URovMuJoCoBridge()
{
    // 需要 tick：键盘推进要每帧把力写进 mjData::xfrc_applied
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.bStartWithTickEnabled = true;
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
    OutPosition = TargetBody->GetWorldPosition() + WorldOffset;
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

// =============================================================================
// 键盘推进
// =============================================================================
//
// 为什么走 xfrc_applied 而不是 AirSim 的 RC 通道：
//   1. 桥接把 ARovPawn 的物理和碰撞全关了，AirSim 算出的推力没有物理体承接；
//   2. MuJoCo 模型里 7 个 actuator 全是机械臂关节，推进器只是视觉几何体，
//      没有对应的 <actuator>，没法用 ctrl 驱动。
//
// xfrc_applied 是 MuJoCo 给每个 body 预留的 6 维外力（力 xyz + 力矩 xyz），
// mj_step 会把它叠加进动力学，且**不会自动清零**，所以每帧必须先清再写。
//
// 力的量级按质量缩放（力 = 质量 × 加速度），这样换一台不同质量的 ROV 手感一致。

void URovMuJoCoBridge::TickComponent(float DeltaTime, ELevelTick TickType,
                                     FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    if (bBridgeActive && bEnableKeyboardControl) {
        ApplyControlForce();
    }
}

void URovMuJoCoBridge::ApplyControlForce()
{
    if (!TargetBody.IsValid() || !TargetBody->IsBound()) {
        return;
    }

    AAMjManager* Mgr = AAMjManager::GetManager();
    if (!Mgr || !Mgr->PhysicsEngine) {
        return;
    }

    mjModel* Model = Mgr->PhysicsEngine->GetModel();
    mjData* Data = Mgr->PhysicsEngine->GetData();
    if (!Model || !Data) {
        return;
    }

    const int32 BodyId = TargetBody->GetMjID();
    if (BodyId < 0 || BodyId >= Model->nbody) {
        return;
    }

    const double Gravity = FMath::Abs((double)Model->opt.gravity[2]);

    // ---------------- 一次性初始化：打开原生流体模型 + 受力诊断 ----------------
    static bool bFluidInitialized = false;
    if (!bFluidInitialized) {
        bFluidInitialized = true;

        // 命令行覆盖，便于现场调参（不用重编）
        auto FloatParam = [](const TCHAR* Key, float Default) -> float {
            float V = Default;
            return FParse::Value(FCommandLine::Get(), Key, V) ? V : Default;
        };
        DisplacedVolume  = FloatParam(TEXT("RovDisplacedVolume="), DisplacedVolume);
        BuoyancyScale    = FloatParam(TEXT("RovBuoyancyScale="), BuoyancyScale);
        ThrusterMaxRatio = FloatParam(TEXT("RovThrustRatio="), ThrusterMaxRatio);
        WaterSurfaceZ    = FloatParam(TEXT("RovWaterSurfaceZ="), WaterSurfaceZ);
        UE_LOG(LogRovBridge, Log,
            TEXT("[RovMuJoCoBridge][调参] 排水体积=%.4f m³  浮力倍率=%.2f  推力比=%.2f  水面Z=%.2f m"),
            DisplacedVolume, BuoyancyScale, ThrusterMaxRatio, WaterSurfaceZ);

        if (bUseNativeFluid) {
            Model->opt.density = FluidDensity;
            Model->opt.viscosity = FluidViscosity;
            UE_LOG(LogRovBridge, Log,
                TEXT("[RovMuJoCoBridge][流体] 已启用 MuJoCo 椭球流体模型  density=%.1f kg/m³  viscosity=%.6f Pa·s"),
                Model->opt.density, Model->opt.viscosity);
        }
        else {
            Model->opt.density = 0.0;
            Model->opt.viscosity = 0.0;
            UE_LOG(LogRovBridge, Log, TEXT("[RovMuJoCoBridge][流体] 已关闭（density=viscosity=0）"));
        }

        // 质量诊断：确认 UE 里这个模型到底多重
        const double TotalMass = Model->body_subtreemass[BodyId];
        UE_LOG(LogRovBridge, Log,
            TEXT("[RovMuJoCoBridge][质量] 目标 body id=%d  body_mass=%.3f kg  body_subtreemass=%.3f kg  nbody=%d"),
            BodyId, Model->body_mass[BodyId], TotalMass, Model->nbody);
        for (int32 i = 1; i < Model->nbody; ++i) {
            if (Model->body_mass[i] > 0.5) {
                const char* Name = (Model->names && Model->name_bodyadr[i] >= 0)
                    ? (Model->names + Model->name_bodyadr[i]) : "?";
                UE_LOG(LogRovBridge, Log,
                    TEXT("[RovMuJoCoBridge][质量]   body[%2d] %-24s mass=%.3f  subtreemass=%.3f"),
                    i, ANSI_TO_TCHAR(Name), Model->body_mass[i], Model->body_subtreemass[i]);
            }
        }

        // 洋流模型初始化（三层级联）
        OceanCurrent.bEnabled = bEnableOceanCurrent;
        OceanCurrent.MeanSpeed = OceanCurrentMeanSpeed;
        OceanCurrent.Seed = 42;
        OceanCurrent.Initialize();
        UE_LOG(LogRovBridge, Log,
            TEXT("[RovMuJoCoBridge][洋流] %s  平均流速=%.2f m/s  分层=%d 层  湍流强度=%.2f"),
            bEnableOceanCurrent ? TEXT("已启用") : TEXT("已关闭"),
            OceanCurrentMeanSpeed, OceanCurrent.Layers.Num(), OceanCurrent.TurbIntensity);

        // 单位诊断：确认 MuJoCo 模型到底是米还是厘米
        {
            int32 RovGeomId = -1;
            for (int32 g = 0; g < Model->ngeom; ++g) {
                const char* GName = (Model->names && Model->name_geomadr[g] >= 0)
                    ? (Model->names + Model->name_geomadr[g]) : "";
                if (FString(ANSI_TO_TCHAR(GName)).EndsWith(TEXT("rov_body"))) {
                    RovGeomId = g;
                    break;
                }
            }
            UE_LOG(LogRovBridge, Log,
                TEXT("[RovMuJoCoBridge][单位] gravity=%.3f  timestep=%.5f  stat.extent=%.3f  "
                     "模型包围盒中心=(%.2f,%.2f,%.2f)"),
                Model->opt.gravity[2], Model->opt.timestep, Model->stat.extent,
                Model->stat.center[0], Model->stat.center[1], Model->stat.center[2]);
            if (RovGeomId >= 0) {
                UE_LOG(LogRovBridge, Log,
                    TEXT("[RovMuJoCoBridge][单位] rov_body geom size=(%.4f, %.4f, %.4f)  "
                         "（若≈0.4/0.3/0.2 则模型是米；若≈40/30/20 则是厘米）"),
                    Model->geom_size[3 * RovGeomId + 0],
                    Model->geom_size[3 * RovGeomId + 1],
                    Model->geom_size[3 * RovGeomId + 2]);
            }
            if (AActor* Owner = GetOwner()) {
                UE_LOG(LogRovBridge, Log,
                    TEXT("[RovMuJoCoBridge][单位] Pawn 世界坐标=(%.2f, %.2f, %.2f) cm  "
                         "MuJoCo xpos=(%.2f, %.2f, %.2f)"),
                    Owner->GetActorLocation().X, Owner->GetActorLocation().Y, Owner->GetActorLocation().Z,
                    Data->xpos[3 * BodyId + 0], Data->xpos[3 * BodyId + 1], Data->xpos[3 * BodyId + 2]);
            }
        }

        // 受力诊断：三个数摆出来，一眼能看出浮沉
        const double Weight = TotalMass * Gravity;
        const double Buoyancy = FluidDensity * Gravity * DisplacedVolume * BuoyancyScale;
        UE_LOG(LogRovBridge, Log,
            TEXT("[RovMuJoCoBridge][受力] 重力=%.1f N  浮力=%.1f N  净力=%+.1f N（正=上浮，负=下沉）  "
                 "排水体积=%.4f m³  浮力倍率=%.2f"),
            Weight, Buoyancy, Buoyancy - Weight, DisplacedVolume, BuoyancyScale);
    }

    APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
    if (!PC) {
        return;
    }

    // 本体坐标系下的期望方向
    // 注意：A/D 的符号和直觉相反——这个模型的 Y 轴朝左，所以 A 用 +1、D 用 -1。
    // 垂直用 Q/E（PageUp/PageDown 在笔记本上常常没有，保留作别名）。
    double LocalX = 0.0, LocalY = 0.0, LocalZ = 0.0;
    if (PC->IsInputKeyDown(EKeys::W)) { LocalX += 1.0; }
    if (PC->IsInputKeyDown(EKeys::S)) { LocalX -= 1.0; }
    if (PC->IsInputKeyDown(EKeys::A)) { LocalY += 1.0; }
    if (PC->IsInputKeyDown(EKeys::D)) { LocalY -= 1.0; }
    if (PC->IsInputKeyDown(EKeys::Q) || PC->IsInputKeyDown(EKeys::PageUp)) { LocalZ += 1.0; }
    if (PC->IsInputKeyDown(EKeys::E) || PC->IsInputKeyDown(EKeys::PageDown)) { LocalZ -= 1.0; }

    // 按键诊断：状态一变就记一条（不节流），用来确认输入到底有没有进到游戏
    {
        // 同时读两条通道：PlayerController 和 PlayerInput，任一为真即算按下。
        // 有些 build 里 IsInputKeyDown 取不到状态，PlayerInput 可以兜底。
        auto IsDown = [PC](const FKey& Key) -> bool {
            bool b = PC->IsInputKeyDown(Key);
            if (!b && PC->PlayerInput) {
                b = PC->PlayerInput->IsPressed(Key);
            }
            return b;
        };

        static int32 PrevKeyMask = 0;
        const int32 KeyMask =
            (IsDown(EKeys::W) ? 1 : 0) |
            (IsDown(EKeys::S) ? 2 : 0) |
            (IsDown(EKeys::A) ? 4 : 0) |
            (IsDown(EKeys::D) ? 8 : 0) |
            (IsDown(EKeys::Q) ? 16 : 0) |
            (IsDown(EKeys::E) ? 32 : 0) |
            (IsDown(EKeys::Z) ? 64 : 0);
        if (KeyMask != PrevKeyMask) {
            UE_LOG(LogRovBridge, Log,
                TEXT("[RovMuJoCoBridge][按键] 状态 %d -> %d  (W=1 S=2 A=4 D=8 Q=16 E=32 Z=64)"),
                PrevKeyMask, KeyMask);
            PrevKeyMask = KeyMask;
        }

        // 每秒打印一次焦点状态，确认游戏窗口到底有没有拿到输入焦点
        static int32 FocusLogTick = -1;
        const double NowSec = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
        const int32 CurTick = (int32)NowSec;
        if (CurTick != FocusLogTick) {
            FocusLogTick = CurTick;
            UE_LOG(LogRovBridge, Log,
                TEXT("[RovMuJoCoBridge][焦点] IsLocalController=%d  PlayerInput=%s  "
                     "bShowMouseCursor=%d  bIgnoreInput=%d  按键掩码=%d"),
                PC->IsLocalController() ? 1 : 0,
                PC->PlayerInput ? TEXT("valid") : TEXT("NULL"),
                PC->bShowMouseCursor ? 1 : 0,
                PC->IsInputKeyDown(EKeys::LeftMouseButton) ? 1 : 0,
                KeyMask);
        }
    }

    // 命令行自检：-RovThrustTest 会在启动后持续上浮 2 秒并打印 Z
    static const bool bAutoThrustTest =
        FParse::Param(FCommandLine::Get(), TEXT("RovThrustTest"));
    static double AutoTestStartTime = -1.0;
    static int32 AutoTestLastTick = -1;
    if (bAutoThrustTest) {
        UWorld* W = GetWorld();
        const double Now = W ? W->GetTimeSeconds() : 0.0;
        if (AutoTestStartTime < 0.0) {
            AutoTestStartTime = Now;
            UE_LOG(LogRovBridge, Log,
                TEXT("[RovMuJoCoBridge][自检] 开始：上浮 2 秒，起始 Z=%.3f"), Data->xpos[3 * BodyId + 2]);
        }
        if (Now - AutoTestStartTime < 10.0) {
            LocalZ = 1.0;
        }
        const int32 Tick = (int32)((Now - AutoTestStartTime) * 2.0);
        if (Tick != AutoTestLastTick) {
            AutoTestLastTick = Tick;
            UE_LOG(LogRovBridge, Log,
                TEXT("[RovMuJoCoBridge][自检] t=%.1fs  Z=%.3f  总Fz=%.1f N  vz=%.3f"),
                Now - AutoTestStartTime, Data->xpos[3 * BodyId + 2],
                CachedForceZ.load(), Data->cvel[6 * BodyId + 5]);
        }
    }

    const double Mass = Model->body_subtreemass[BodyId];
    if (Mass <= 0.0) {
        return;
    }

    const mjtNum* Q = &Data->xquat[4 * BodyId];   // (w, x, y, z)
    const double Qw = Q[0], Qx = Q[1], Qy = Q[2], Qz = Q[3];

    // 只取偏航角，忽略俯仰和横滚 —— 否则 ROV 一翻滚推力方向就乱
    const double Yaw = FMath::Atan2(2.0 * (Qw * Qz + Qx * Qy),
                                    1.0 - 2.0 * (Qy * Qy + Qz * Qz));
    const double Cy = FMath::Cos(Yaw);
    const double Sy = FMath::Sin(Yaw);

    // ------------------------------------------------------------------
    // 受力分解：每一项独立计算，最后相加。不直接设定合力。
    // ------------------------------------------------------------------

    // 【1】推进器推力：键盘只给比例，力 = 比例 × 最大推力
    const double ThrusterMax = Mass * Gravity * ThrusterMaxRatio;
    double ThrusterX = ThrusterMax * (LocalX * Cy - LocalY * Sy);
    double ThrusterY = ThrusterMax * (LocalX * Sy + LocalY * Cy);
    double ThrusterZ = ThrusterMax * LocalZ;

    const double RovZ = Data->xpos[3 * BodyId + 2];
    const FVector RovPos(Data->xpos[3 * BodyId + 0],
                         Data->xpos[3 * BodyId + 1],
                         Data->xpos[3 * BodyId + 2]);

    const double FrameDt = GetWorld() ? FMath::Max(GetWorld()->GetDeltaSeconds(), 1e-4f) : 0.016;

    // 【2】洋流：三层级联求当前流速
    const FVector VCurrent = OceanCurrent.GetVelocity(RovPos, (float)FrameDt);

    // 【3】定深（PID）：按 Z 键锁定当前高度，再按解除
    if (bEnableDepthHold && PC) {
        const bool bDepthKeyDown = PC->IsInputKeyDown(EKeys::Z);
        if (bDepthKeyDown && !bPrevDepthKeyDown) {
            bDepthHoldActive = !bDepthHoldActive;
            DepthPID.Reset();
            if (bDepthHoldActive) {
                DepthHoldTargetZ = RovZ;
            }
            UE_LOG(LogRovBridge, Log,
                TEXT("[RovMuJoCoBridge][定深] %s（目标 Z=%.3f m）"),
                bDepthHoldActive ? TEXT("已锁定") : TEXT("已解除"), DepthHoldTargetZ);
        }
        bPrevDepthKeyDown = bDepthKeyDown;
    }

    // 命令行自检：-RovDepthHoldTest 在 3 秒后自动锁定当前深度并打印误差，
    // 用来验证 PID 定深，不依赖键盘。
    static const bool bDepthHoldTest =
        FParse::Param(FCommandLine::Get(), TEXT("RovDepthHoldTest"));
    static int32 DepthTestLastTick = -1;
    if (bDepthHoldTest) {
        const double Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
        if (Now > 1.0 && !bDepthHoldActive) {
            bDepthHoldActive = true;
            DepthPID.Reset();
            DepthHoldTargetZ = RovZ;
            UE_LOG(LogRovBridge, Log,
                TEXT("[RovMuJoCoBridge][定深自检] 锁定 Z=%.3f m"), DepthHoldTargetZ);
        }
    }

    // 命令行自检：-RovSpinTest 在 2 秒时给一个 3 rad/s 的三轴角速度脉冲，
    // 然后打印角速度衰减，验证流体模型的旋转阻力（验收项 P7）。
    static const bool bSpinTest = FParse::Param(FCommandLine::Get(), TEXT("RovSpinTest"));
    static bool bSpinApplied = false;
    static int32 SpinLogTick = -1;
    if (bSpinTest && Model->body_jntnum[BodyId] >= 1) {
        const int32 SpinJnt = Model->body_jntadr[BodyId];
        if (Model->jnt_type[SpinJnt] == mjJNT_FREE) {
            const int32 SpinDof = Model->jnt_dofadr[SpinJnt];
            const double Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
            if (!bSpinApplied && Now > 2.0) {
                bSpinApplied = true;
                // 先抬到水中（Z=4m）并清零线速度，避免海底接触力矩污染测量
                const int32 QPosAdr = Model->jnt_qposadr[SpinJnt];
                Data->qpos[QPosAdr + 0] = 0.0;
                Data->qpos[QPosAdr + 1] = 0.0;
                Data->qpos[QPosAdr + 2] = 4.0;
                Data->qpos[QPosAdr + 3] = 1.0;   // quat w
                Data->qpos[QPosAdr + 4] = 0.0;
                Data->qpos[QPosAdr + 5] = 0.0;
                Data->qpos[QPosAdr + 6] = 0.0;
                Data->qvel[SpinDof + 0] = 0.0;
                Data->qvel[SpinDof + 1] = 0.0;
                Data->qvel[SpinDof + 2] = 0.0;
                Data->qvel[SpinDof + 3] = 3.0;
                Data->qvel[SpinDof + 4] = 3.0;
                Data->qvel[SpinDof + 5] = 3.0;
                UE_LOG(LogRovBridge, Log,
                    TEXT("[RovMuJoCoBridge][旋转自检] 移至水中 Z=4m，施加 3 rad/s 三轴角速度脉冲"));
            }
            if (bSpinApplied) {
                const int32 Tick = (int32)(Now * 4.0);
                if (Tick != SpinLogTick) {
                    SpinLogTick = Tick;
                    const double Wx = Data->qvel[SpinDof + 3];
                    const double Wy = Data->qvel[SpinDof + 4];
                    const double Wz = Data->qvel[SpinDof + 5];
                    UE_LOG(LogRovBridge, Log,
                        TEXT("[RovMuJoCoBridge][旋转自检] t=%.2fs  |ω|=%.4f rad/s"),
                        Now - 2.0, FMath::Sqrt(Wx * Wx + Wy * Wy + Wz * Wz));
                }
            }
        }
    }

    if (bDepthHoldActive) {
        DepthPID.MaxThrust = (float)ThrusterMax;
        const float PidThrust = DepthPID.Compute(
            (float)DepthHoldTargetZ, (float)RovZ, (float)FrameDt, (float)VCurrent.Z);
        ThrusterZ = FMath::Clamp((double)PidThrust, -ThrusterMax, ThrusterMax);

        // 定深自检日志（放在 PID 之后，才能看到真实的 PID 输出）
        if (bDepthHoldTest) {
            const double Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
            const int32 Tick = (int32)(Now * 2.0);
            if (Tick != DepthTestLastTick) {
                DepthTestLastTick = Tick;
                UE_LOG(LogRovBridge, Log,
                    TEXT("[RovMuJoCoBridge][定深自检] t=%.1fs  目标=%.3f  当前=%.3f  误差=%+.3f m  "
                         "PID推力=%.1f N  洋流Vz=%.3f m/s"),
                    Now, DepthHoldTargetZ, RovZ, DepthHoldTargetZ - RovZ, ThrusterZ, VCurrent.Z);
            }
        }
    }

    // 【4】水面钳制：ROV 已在水面以上时，不允许推进器继续向上推
    if (RovZ >= WaterSurfaceZ && ThrusterZ > 0.0) {
        ThrusterZ = 0.0;
    }

    // 【5】浮力：Fb = ρ·g·V，永远向上。
    // 但**水面之上没有水**，所以浮力为零 —— 这也是 ROV 不会飞出水的物理原因：
    // 一旦越过水面，只剩重力把它拉回来。
    double Buoyancy = 0.0;
    if (RovZ < WaterSurfaceZ) {
        Buoyancy = FluidDensity * Gravity * DisplacedVolume * BuoyancyScale;
    }

    // 【6】洋流拖曳：Fc = -k·(V_robot - V_current)
    // 用相对流速，而不是只用洋流速度 —— 机器人自己游动时也应该受到阻力。
    double CurrentX = 0.0, CurrentY = 0.0, CurrentZ = 0.0;
    {
        double Vrx = 0.0, Vry = 0.0, Vrz = 0.0;
        if (Model->body_jntnum[BodyId] >= 1) {
            const int32 JntId = Model->body_jntadr[BodyId];
            if (Model->jnt_type[JntId] == mjJNT_FREE) {
                const int32 DofAdr = Model->jnt_dofadr[JntId];
                Vrx = Data->qvel[DofAdr + 0];
                Vry = Data->qvel[DofAdr + 1];
                Vrz = Data->qvel[DofAdr + 2];
            }
        }
        CurrentX = -CurrentDragCoeff * (Vrx - VCurrent.X);
        CurrentY = -CurrentDragCoeff * (Vry - VCurrent.Y);
        CurrentZ = -CurrentDragCoeff * (Vrz - VCurrent.Z);
    }

    // 【7】水动力阻力/升力：由 MuJoCo 椭球流体模型自行计算，这里不重复施加

    const double Fx = ThrusterX + CurrentX;
    const double Fy = ThrusterY + CurrentY;
    const double Fz = ThrusterZ + Buoyancy + CurrentZ;

    CachedBodyId.store(BodyId);
    CachedForceX.store(Fx);
    CachedForceY.store(Fy);
    CachedForceZ.store(Fz);

    if (!bControlCallbackRegistered) {
        TWeakObjectPtr<URovMuJoCoBridge> WeakThis(this);
        Mgr->PhysicsEngine->RegisterPreStepCallback(
            [WeakThis](mjModel* M, mjData* D) {
                if (URovMuJoCoBridge* Self = WeakThis.Get()) {
                    Self->ApplyCachedForce(M, D);
                }
            });
        bControlCallbackRegistered = true;
        UE_LOG(LogRovBridge, Log,
            TEXT("[RovMuJoCoBridge] 已注册物理线程施力回调（PreStepCallbacks）"));
    }

    // 节流日志
    static int32 CtrlLogCounter = 0;
    if ((LocalX != 0.0 || LocalY != 0.0 || LocalZ != 0.0) && (++CtrlLogCounter % 30 == 0)) {
        UE_LOG(LogRovBridge, Log,
            TEXT("[RovMuJoCoBridge][受力] 输入=(%.0f,%.0f,%.0f)  推力=(%.1f,%.1f,%.1f) N  "
                 "浮力=%.1f N  洋流拖曳=(%.1f,%.1f,%.1f) N  合计Fz=%.1f N  "
                 "洋流=(%.2f,%.2f,%.2f) m/s  定深=%d  Z=%.3f"),
            LocalX, LocalY, LocalZ, ThrusterX, ThrusterY, ThrusterZ,
            Buoyancy, CurrentX, CurrentY, CurrentZ, Fz,
            VCurrent.X, VCurrent.Y, VCurrent.Z, bDepthHoldActive ? 1 : 0, RovZ);
    }
}

void URovMuJoCoBridge::ApplyCachedForce(mjModel_* Model, mjData_* Data)
{
    const int32 BodyId = CachedBodyId.load();
    if (BodyId < 0 || !Model || !Data) {
        return;
    }
    if (BodyId >= Model->nbody) {
        return;
    }

    // xfrc_applied 布局：[torque_x, torque_y, torque_z, force_x, force_y, force_z]
    // 力在 3/4/5。写错槽位会变成加扭矩。
    mjtNum* F = &Data->xfrc_applied[6 * BodyId];
    F[0] = 0.0;
    F[1] = 0.0;
    F[2] = 0.0;
    F[3] = CachedForceX.load();
    F[4] = CachedForceY.load();
    F[5] = CachedForceZ.load();

    // 同时用冲量写自由关节速度。
    //
    // 为什么两个都写：xfrc_applied 是 MuJoCo 的标准外力入口，语义正确；
    // 但在本工程里实测它不生效（AMjManager 可能装了 CustomStepHandler /
    // Replay，绕过了 mj_step 的外力通道）。冲量路径已验证可靠，
    // 作为兜底同时施加，保证 ROV 真的会动。
    //
    // 注意：两条路径同时生效会双倍施力，所以 xfrc 写入的量按 0 处理，
    // 仅保留语义占位；真正驱动的是下面的冲量。
    F[3] = 0.0;
    F[4] = 0.0;
    F[5] = 0.0;

    if (Model->body_jntnum[BodyId] < 1) {
        return;
    }
    const int32 JntId = Model->body_jntadr[BodyId];
    if (Model->jnt_type[JntId] != mjJNT_FREE) {
        return;
    }

    const int32 DofAdr = Model->jnt_dofadr[JntId];
    const double Mass = Model->body_subtreemass[BodyId];
    const double Dt = Model->opt.timestep;
    if (Mass <= 0.0 || Dt <= 0.0) {
        return;
    }

    // 自由关节 qvel 前三个 = 世界系线速度 → 力 × dt / m = 速度增量
    Data->qvel[DofAdr + 0] += CachedForceX.load() / Mass * Dt;
    Data->qvel[DofAdr + 1] += CachedForceY.load() / Mass * Dt;
    Data->qvel[DofAdr + 2] += CachedForceZ.load() / Mass * Dt;

    // 附加角阻尼（独立受力项）：补足 MuJoCo 椭球模型偏弱的旋转阻力。
    // 后三个 qvel 是 body 系角速度，直接按指数衰减。
    if (AngularDampingCoeff > 0.0f) {
        const double AngFactor = FMath::Clamp(1.0 - (double)AngularDampingCoeff * Dt, 0.0, 1.0);
        Data->qvel[DofAdr + 3] *= AngFactor;
        Data->qvel[DofAdr + 4] *= AngFactor;
        Data->qvel[DofAdr + 5] *= AngFactor;
    }
}
