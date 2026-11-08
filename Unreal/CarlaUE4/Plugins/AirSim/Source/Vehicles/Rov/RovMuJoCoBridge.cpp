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

namespace {
/** 用 MuJoCo 四元数 (w,x,y,z) 旋转向量：v' = v + 2*(w*c + q×c)，c = q×v */
void RotByQuat(double qw, double qx, double qy, double qz,
               double vx, double vy, double vz,
               double& ox, double& oy, double& oz)
{
    const double cx = qy * vz - qz * vy;
    const double cy = qz * vx - qx * vz;
    const double cz = qx * vy - qy * vx;
    const double dx = qy * cz - qz * cy;
    const double dy = qz * cx - qx * cz;
    const double dz = qx * cy - qy * cx;
    ox = vx + 2.0 * (qw * cx + dx);
    oy = vy + 2.0 * (qw * cy + dy);
    oz = vz + 2.0 * (qw * cz + dz);
}
}

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
        BuoyancyArm      = FloatParam(TEXT("RovBuoyancyArm="), BuoyancyArm);
        ArmBuoyancyVolume = FloatParam(TEXT("RovArmBuoyVolume="), ArmBuoyancyVolume);
        ArmBuoyancyScale  = FloatParam(TEXT("RovArmBuoyScale="), ArmBuoyancyScale);
        AngularDampingCoeff = FloatParam(TEXT("RovAngularDamping="), AngularDampingCoeff);
        ArmJointDamping = FloatParam(TEXT("RovArmDamping="), ArmJointDamping);
        ThrusterMaxRatio = FloatParam(TEXT("RovThrustRatio="), ThrusterMaxRatio);
        WaterSurfaceZ    = FloatParam(TEXT("RovWaterSurfaceZ="), WaterSurfaceZ);
        if (FParse::Param(FCommandLine::Get(), TEXT("RovNoAttitude"))) { bEnableAttitudeHold = false; }
        UE_LOG(LogRovBridge, Log,
            TEXT("[RovMuJoCoBridge][调参] 排水体积=%.4f m³  浮力倍率=%.2f  臂排水=%.2f L×%.2f  "
                 "推力比=%.2f  水面Z=%.2f m"),
            DisplacedVolume, BuoyancyScale, ArmBuoyancyVolume, ArmBuoyancyScale,
            ThrusterMaxRatio, WaterSurfaceZ);

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
        UE_LOG(LogRovBridge, Log,
            TEXT("[RovMuJoCoBridge][惯量] body_inertia=(%.5f, %.5f, %.5f) kg·m²  "
                 "（角阻尼 Kd = %.1f × 平均惯量 %.5f = %.3f N·m·s/rad）"),
            Model->body_inertia[3 * BodyId + 0],
            Model->body_inertia[3 * BodyId + 1],
            Model->body_inertia[3 * BodyId + 2],
            AngularDampingCoeff,
            (Model->body_inertia[3 * BodyId + 0] + Model->body_inertia[3 * BodyId + 1]
             + Model->body_inertia[3 * BodyId + 2]) / 3.0,
            AngularDampingCoeff * (Model->body_inertia[3 * BodyId + 0] + Model->body_inertia[3 * BodyId + 1]
             + Model->body_inertia[3 * BodyId + 2]) / 3.0);
        for (int32 i = 1; i < Model->nbody; ++i) {
            if (Model->body_mass[i] > 0.5) {
                const char* Name = (Model->names && Model->name_bodyadr[i] >= 0)
                    ? (Model->names + Model->name_bodyadr[i]) : "?";
                UE_LOG(LogRovBridge, Log,
                    TEXT("[RovMuJoCoBridge][质量]   body[%2d] %-24s mass=%.3f  subtreemass=%.3f"),
                    i, ANSI_TO_TCHAR(Name), Model->body_mass[i], Model->body_subtreemass[i]);
            }
        }

        // 机械臂关节阻尼 + 刚度：模型的机械臂关节没有 ctrl 输入，
        // 会自由摆动，反作用力矩把 ROV 本体晃得不停翻滚。
        // 阻尼抵抗运动、刚度把它锁在参考角度，两者合起来相当于位置伺服。
        {
            int32 DampedDofs = 0;
            int32 StiffJoints = 0;
            for (int32 j = 0; j < Model->njnt; ++j) {
                if (Model->jnt_type[j] == mjJNT_FREE) {
                    continue;
                }
                const int32 Adr = Model->jnt_dofadr[j];
                const int32 Num = (Model->jnt_type[j] == mjJNT_BALL) ? 3 : 1;
                for (int32 k = 0; k < Num; ++k) {
                    Model->dof_damping[Adr + k] = ArmJointDamping;
                    ++DampedDofs;
                }
                Model->jnt_stiffness[j] = (mjtNum)ArmJointStiffness;
                ++StiffJoints;
            }
            UE_LOG(LogRovBridge, Log,
                TEXT("[RovMuJoCoBridge][机械臂] %d 个关节：阻尼 %.1f N·m·s/rad、刚度 %.1f N·m/rad（%d 个自由度）"),
                StiffJoints, ArmJointDamping, ArmJointStiffness, DampedDofs);
        }

        // 机械臂重力补偿：抵消其自重，消除对艇体的恒定倾覆力矩
        if (bArmGravityComp) {
            int32 CompBodies = 0;
            for (int32 b = 1; b < Model->nbody; ++b) {
                if (b == BodyId) {
                    continue;   // ROV 本体不补偿
                }
                Model->body_gravcomp[b] = 1.0;
                ++CompBodies;
            }
            UE_LOG(LogRovBridge, Log,
                TEXT("[RovMuJoCoBridge][机械臂] 已对 %d 个 body 做重力补偿（gravcomp=1.0）"),
                CompBodies);
        }

        // 复合惯量：从自由关节的 6×6 质量阵取转动对角元。
        // body_inertia 只有 ROV 本体，不含机械臂，用它算增益会严重偏小 ——
        // 这正是消融实验里"纯刚体 + PD 也不收敛"的原因。
        //
        // 安全性：qM 只有在 mj_forward/mj_step 之后才有效。打包版首帧的时序
        // 可能与编辑器不同，若此时 qM 未就绪会读到垃圾值 → 增益变 NaN →
        // 力矩爆炸。所以这里校验：非空、有限、为正，否则退回 body_inertia。
        {
            const int32 Nv = Model->nv;
            bool bOk = (Nv >= 6) && (Data->qM != nullptr);
            if (bOk) {
                TArray<mjtNum> M;
                M.SetNumZeroed(Nv * Nv);
                mj_fullM(Model, M.GetData(), Data->qM);
                for (int32 a = 0; a < 3; ++a) {
                    const double V = M[(3 + a) * Nv + (3 + a)];
                    if (!FMath::IsFinite(V) || V <= 0.0) {
                        bOk = false;
                        break;
                    }
                    EffInertia[a] = V;
                }
            }
            if (!bOk) {
                for (int32 a = 0; a < 3; ++a) {
                    EffInertia[a] = Model->body_inertia[3 * BodyId + a];
                }
                UE_LOG(LogRovBridge, Warning,
                    TEXT("[RovMuJoCoBridge][复合惯量] qM 未就绪，回退到 body_inertia"));
            }
            UE_LOG(LogRovBridge, Log,
                TEXT("[RovMuJoCoBridge][复合惯量] 转动对角元 = (%.3f, %.3f, %.3f) kg·m²  "
                     "（body_inertia = %.3f/%.3f/%.3f，来源=%s）"),
                EffInertia[0], EffInertia[1], EffInertia[2],
                Model->body_inertia[3 * BodyId + 0],
                Model->body_inertia[3 * BodyId + 1],
                Model->body_inertia[3 * BodyId + 2],
                bOk ? TEXT("6x6 质量阵") : TEXT("body_inertia 回退"));
        }

        // 洋流模型初始化（三层级联）
        if (FParse::Param(FCommandLine::Get(), TEXT("RovOceanCurrent"))) {
            bEnableOceanCurrent = true;   // 命令行强制开启，便于测试
        }
        if (FParse::Param(FCommandLine::Get(), TEXT("RovNoAttitude"))) {
            bEnableAttitudeHold = false;  // 自检时关掉姿态控制器，避免干扰
        }
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

    // 【航向调整】← / → 连续改变目标航向（姿态保持开启时生效）
    //
    // 方向定义：MuJoCo→UE 的坐标映射是 X→X、Y→-Y、Z→Z（MjUtils::MjToUERotation），
    // 属于**镜像变换**，所以 MuJoCo 里的 +yaw 在画面上表现为 -yaw。这里按操作者
    // 看到的画面定义按键方向：→ = 画面右转 = MuJoCo yaw 减小，← 反之。
    //
    // 为什么要给前馈：以前只把目标航向按 60°/s 往前推，而内环最大角速度只有
    // 0.6 rad/s（34°/s），目标跑得比机身快，误差会一路累积；一旦超过 180°，
    // 四元数短弧归一化会把误差翻号，机身就反着转 —— 表现就是"转满一圈后回弹"。
    // 现在按住期间直接给期望角速度（前馈），误差只反映跟踪滞后，永远不会绕圈。
    double YawCmdRateDeg = 0.0;   // 本帧航向角速度指令（deg/s，MuJoCo 符号）
    if (bEnableAttitudeHold && PC) {
        const bool bYawLeft  = PC->IsInputKeyDown(EKeys::Left);
        const bool bYawRight = PC->IsInputKeyDown(EKeys::Right);
        if (bYawLeft != bYawRight) {
            YawCmdRateDeg = bYawLeft ? +(double)YawRateDegPerSec
                                     : -(double)YawRateDegPerSec;
        }
    }

    // 命令行自检：-RovYawTest 在 3~11 秒模拟"一直按住 →"（画面右转 = MuJoCo yaw 减小），
    // 转过的总角度超过 360°，用来验证方向、以及转满一圈后不回弹。
    static const bool bYawTest = FParse::Param(FCommandLine::Get(), TEXT("RovYawTest"));
    if (bYawTest && bEnableAttitudeHold && YawCmdRateDeg == 0.0) {
        const double TestT = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
        if (TestT > 3.0 && TestT < 11.0) {
            YawCmdRateDeg = -(double)YawRateDegPerSec;
        }
    }

    if (YawCmdRateDeg != 0.0) {
        TargetYawDeg += (float)(YawCmdRateDeg * FrameDt);
        while (TargetYawDeg >  180.0f) { TargetYawDeg -= 360.0f; }
        while (TargetYawDeg < -180.0f) { TargetYawDeg += 360.0f; }
        static int32 YawLogCounter = 0;
        if ((++YawLogCounter % 30) == 0) {
            UE_LOG(LogRovBridge, Log,
                TEXT("[RovMuJoCoBridge][航向] 目标航向 = %.1f°  指令角速度 = %+.0f°/s（←/→）"),
                TargetYawDeg, YawCmdRateDeg);
        }
    }

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

    // 出生自动锁定深度：避免浮力略小于重力时一路沉到海底，保证演示时停在水面附近
    static bool bAutoDepthInit = false;
    if (bEnableDepthHold && !bAutoDepthInit) {
        bAutoDepthInit = true;
        bDepthHoldActive = true;
        DepthPID.Reset();
        DepthHoldTargetZ = RovZ;
        UE_LOG(LogRovBridge, Log, TEXT("[RovMuJoCoBridge][定深] 出生自动锁定 Z=%.3f m"), DepthHoldTargetZ);
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
                // 放到水中（Z=-5m）并清零线速度，避免海底接触和浮力突变污染测量。
                // 注意：姿态控制在自检期间会被跳过，否则闭环会把脉冲立刻压掉。
                const int32 QPosAdr = Model->jnt_qposadr[SpinJnt];
                Data->qpos[QPosAdr + 0] = 0.0;
                Data->qpos[QPosAdr + 1] = 0.0;
                Data->qpos[QPosAdr + 2] = -5.0;
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
                DepthHoldTargetZ = -5.0;   // 定深跟着走，别把 ROV 拉回出生高度
                // 关掉浮心高度：BG≠0 会形成"单摆"回复力矩，角速度会来回摆，
                // 没法拟合成指数衰减。自检只要纯阻尼，浮力大小不变（仍然中性附近）。
                BuoyancyArm = 0.0f;
                bDepthHoldActive = false;
                UE_LOG(LogRovBridge, Log,
                    TEXT("[RovMuJoCoBridge][旋转自检] 移至水中 Z=-5m，施加 3 rad/s 三轴角速度脉冲"
                         "（dof_damping=%.2f）"),
                    (double)Model->dof_damping[SpinDof + 3]);
            }
            if (bSpinApplied) {
                const int32 Tick = (int32)(Now * 4.0);
                if (Tick != SpinLogTick) {
                    SpinLogTick = Tick;
                    const double Wx = Data->qvel[SpinDof + 3];
                    const double Wy = Data->qvel[SpinDof + 4];
                    const double Wz = Data->qvel[SpinDof + 5];
                    UE_LOG(LogRovBridge, Log,
                        TEXT("[RovMuJoCoBridge][旋转自检] t=%.2fs  wx=%.4f wy=%.4f wz=%.4f  |w|=%.4f rad/s"),
                        Now - 2.0, Wx, Wy, Wz,
                        FMath::Sqrt(Wx * Wx + Wy * Wy + Wz * Wz));
                }
            }
        }
    }

    // 命令行自检：-RovTiltTest 在 2 秒时把 ROV 倾斜 20° 后释放，
    // 验证被动稳定（浮心高于重心产生的回复力矩）能否把它扶正。
    static const bool bTiltTest = FParse::Param(FCommandLine::Get(), TEXT("RovTiltTest"));
    static bool bTiltApplied = false;
    static int32 TiltLogTick = -1;
    if (bTiltTest && Model->body_jntnum[BodyId] >= 1) {
        const int32 TJnt = Model->body_jntadr[BodyId];
        if (Model->jnt_type[TJnt] == mjJNT_FREE) {
            const int32 TDof = Model->jnt_dofadr[TJnt];
            const int32 TQPos = Model->jnt_qposadr[TJnt];
            const double Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;

            if (!bTiltApplied && Now > 2.0) {
                bTiltApplied = true;
                const double HalfAngle = 10.0 * PI / 180.0;   // 20° 的一半
                Data->qpos[TQPos + 2] = 4.0;                  // 抬到水中，避免接触干扰
                Data->qpos[TQPos + 3] = FMath::Cos(HalfAngle); // w
                Data->qpos[TQPos + 4] = FMath::Sin(HalfAngle); // x（绕 X 轴倾斜）
                Data->qpos[TQPos + 5] = 0.0;
                Data->qpos[TQPos + 6] = 0.0;
                for (int32 k = 0; k < 6; ++k) {
                    Data->qvel[TDof + k] = 0.0;
                }
                UE_LOG(LogRovBridge, Log,
                    TEXT("[RovMuJoCoBridge][倾斜自检] 已倾斜 20° 并释放（BG=%.3f m）"), BuoyancyArm);
            }

            if (bTiltApplied) {
                const int32 Tick = (int32)(Now * 4.0);
                if (Tick != TiltLogTick) {
                    TiltLogTick = Tick;
                    // 当前倾角：机体系 +Z 转到世界后与竖直方向的夹角
                    const double Qw2 = Data->qpos[TQPos + 3];
                    const double Qx2 = Data->qpos[TQPos + 4];
                    const double Qy2 = Data->qpos[TQPos + 5];
                    const double Qz2 = Data->qpos[TQPos + 6];
                    // 旋转 (0,0,1)：结果 z 分量 = 1 - 2*(x² + y²)
                    const double UpZ = 1.0 - 2.0 * (Qx2 * Qx2 + Qy2 * Qy2);
                    const double TiltDeg = FMath::RadiansToDegrees(
                        FMath::Acos(FMath::Clamp(UpZ, -1.0, 1.0)));
                    UE_LOG(LogRovBridge, Log,
                        TEXT("[RovMuJoCoBridge][倾斜自检] t=%.2fs  倾角=%.2f°  ω=%.4f rad/s"),
                        Now - 2.0, TiltDeg, Data->qvel[TDof + 4]);
                }
            }
        }
    }

    // 命令行自检：-RovTorqueAxis=0/1/2 只给该轴一个 100 N·m 力矩，
    // 用来确定 qfrc_applied 角向分量到底在机体系还是世界系。
    static const bool bTorqueAxisTest = FParse::Param(FCommandLine::Get(), TEXT("RovTorqueAxisTest"));
    static int32 TorqueTestAxis = -1;
    if (bTorqueAxisTest && TorqueTestAxis < 0) {
        int32 V = 0;
        if (FParse::Value(FCommandLine::Get(), TEXT("RovTorqueAxis="), V)) {
            TorqueTestAxis = V;
        } else {
            TorqueTestAxis = 0;
        }
        UE_LOG(LogRovBridge, Log,
            TEXT("[RovMuJoCoBridge][力矩轴自检] 将只给轴 %d 施加 100 N·m"), TorqueTestAxis);
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

        // 洋流诊断：每秒打印一次流速与拖曳力，确认量级
        if (bEnableOceanCurrent) {
            static int32 OceanLogTick = -1;
            const int32 Tick = (int32)(GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0);
            if (Tick != OceanLogTick) {
                OceanLogTick = Tick;
                UE_LOG(LogRovBridge, Log,
                    TEXT("[RovMuJoCoBridge][洋流诊断] V_current=(%.4f,%.4f,%.4f) |Vc|=%.4f  "
                         "V_robot=(%.4f,%.4f,%.4f) |Vr|=%.4f  F=(%.1f,%.1f,%.1f) N"),
                    VCurrent.X, VCurrent.Y, VCurrent.Z, VCurrent.Size(),
                    Vrx, Vry, Vrz, FMath::Sqrt(Vrx * Vrx + Vry * Vry + Vrz * Vrz),
                    CurrentX, CurrentY, CurrentZ);
            }
        }
    }

    // 【7】水动力阻力/升力：由 MuJoCo 椭球流体模型自行计算，这里不重复施加

    const double Fx = ThrusterX + CurrentX;
    const double Fy = ThrusterY + CurrentY;
    const double Fz = ThrusterZ + CurrentZ;   // 浮力单独走 mj_applyFT（作用在浮心）

    // 【8】浮力的回复力矩由 mj_applyFT 在浮心处施加时自动产生，这里不再手算。

    // 【9】附加角阻尼：以**力矩**形式施加（τ = -Kd·ω，机体系）。
    // MuJoCo 椭球流体模型的旋转阻力偏弱，真实 ROV 的导流罩/鳍会额外提供阻力。
    // Kd = AngularDampingCoeff × 平均主惯量，保持和之前"每秒衰减 1/s"的语义一致。
    double TorqueX = 0.0, TorqueY = 0.0, TorqueZ = 0.0;
    double WorldTorqueX = 0.0, WorldTorqueY = 0.0, WorldTorqueZ = 0.0;
    if (AngularDampingCoeff > 0.0f && Model->body_jntnum[BodyId] >= 1) {
        const int32 DampJnt = Model->body_jntadr[BodyId];
        if (Model->jnt_type[DampJnt] == mjJNT_FREE) {
            const int32 DampDof = Model->jnt_dofadr[DampJnt];
            // 同样要用**复合惯量**：body_inertia 只有本体，实测复合惯量约为其 4 倍，
            // 按本体算会让角阻尼衰减率慢 4 倍（P7 退步的原因）。
            const double Iavg = (EffInertia[0] + EffInertia[1] + EffInertia[2]) / 3.0;
            const double Kd = (double)AngularDampingCoeff * Iavg;
            TorqueX += -Kd * Data->qvel[DampDof + 3];
            TorqueY += -Kd * Data->qvel[DampDof + 4];
            TorqueZ += -Kd * Data->qvel[DampDof + 5];
        }
    }

    // 【10】姿态保持控制器（四元数 PD + 积分）
    //
    // 误差用四元数旋转矢量 e = 2·sign(qw)·qv，而不是欧拉角差：
    //   - 无万向节死锁（ROV 做大俯仰时欧拉角会奇异）
    //   - 小角度下 e 就是旋转矢量，|e| ≤ 2 天然限幅
    //   - sign(qw) 取短弧，避免命令转 350° 而不是 10°
    //
    // 增益按惯量缩放（Kp = I·ω²、Kd = 2I·ζ·ω），换模型不用重调。
    // 积分项是必需的：机械臂挂在一侧产生恒定扰动力矩，
    // 纯 PD 只能把它平衡在某个固定倾角上，消不掉稳态误差。
    // 旧的四元数三轴控制器存在符号/轴向问题，已停用；改用下方世界系解耦控制
    if (false && bEnableAttitudeHold && Model->body_jntnum[BodyId] >= 1) {
        const int32 AttJnt = Model->body_jntadr[BodyId];
        if (Model->jnt_type[AttJnt] == mjJNT_FREE) {
            const int32 AttDof = Model->jnt_dofadr[AttJnt];

            // 首次运行捕获当前航向作为目标
            if (!bAttitudeInitialized) {
                bAttitudeInitialized = true;
                const double InitYaw = FMath::Atan2(2.0 * (Qw * Qz + Qx * Qy),
                                                    1.0 - 2.0 * (Qy * Qy + Qz * Qz));
                TargetYawDeg = FMath::RadiansToDegrees(InitYaw);
                UE_LOG(LogRovBridge, Log,
                    TEXT("[RovMuJoCoBridge][姿态] 已锁定目标航向 %.1f°，带宽 %.2f rad/s，阻尼比 %.2f"),
                    TargetYawDeg, AttitudeBandwidth, AttitudeZeta);
            }

            const double HalfYaw = FMath::DegreesToRadians(TargetYawDeg) * 0.5;
            const double Qtw = FMath::Cos(HalfYaw);
            const double Qtz = FMath::Sin(HalfYaw);

            // q_e = conj(q_c) ⊗ q_t  （机体系误差）
            auto QMul = [](double aw, double ax, double ay, double az,
                           double bw, double bx, double by, double bz,
                           double& ow, double& ox, double& oy, double& oz) {
                ow = aw * bw - ax * bx - ay * by - az * bz;
                ox = aw * bx + ax * bw + ay * bz - az * by;
                oy = aw * by - ax * bz + ay * bw + az * bx;
                oz = aw * bz + ax * by - ay * bx + az * bw;
            };
            double Qew = 0.0, Qex = 0.0, Qey = 0.0, Qez = 0.0;
            QMul(Qw, -Qx, -Qy, -Qz, Qtw, 0.0, 0.0, Qtz, Qew, Qex, Qey, Qez);
            if (Qew < 0.0) {          // 取短弧
                Qew = -Qew; Qex = -Qex; Qey = -Qey; Qez = -Qez;
            }

            const double E[3] = { 2.0 * Qex, 2.0 * Qey, 2.0 * Qez };

            // 积分（限幅防饱和）
            double IntArr[3] = { (double)AttitudeIntegral.X,
                                 (double)AttitudeIntegral.Y,
                                 (double)AttitudeIntegral.Z };
            for (int32 Axis = 0; Axis < 3; ++Axis) {
                IntArr[Axis] += E[Axis] * FrameDt;
                IntArr[Axis] = FMath::Clamp(IntArr[Axis], -2.0, 2.0);
            }
            AttitudeIntegral = FVector(IntArr[0], IntArr[1], IntArr[2]);

            const double W = (double)AttitudeBandwidth;
            const double Zeta = (double)AttitudeZeta;
            const double KiF = (double)AttitudeIntegralFactor;

            // body_inertia 是**主轴系**下的主值，body_iquat 给出主轴相对机体的旋转。
            // 直接用 body_inertia[3*BodyId+Axis] 当 X/Y/Z 惯量是错的（除非 iquat 是单位四元数）。
            // 正确做法：把机体单位轴转到主轴系，算 vᵀ·diag(I)·v 得到该轴的等效惯量。
            const mjtNum* IQ = &Model->body_iquat[4 * BodyId];
            double Iaxis[3] = { 0.0, 0.0, 0.0 };
            for (int32 a = 0; a < 3; ++a) {
                double vx = (a == 0) ? 1.0 : 0.0;
                double vy = (a == 1) ? 1.0 : 0.0;
                double vz = (a == 2) ? 1.0 : 0.0;
                double px = 0.0, py = 0.0, pz = 0.0;
                RotByQuat(IQ[0], -IQ[1], -IQ[2], -IQ[3], vx, vy, vz, px, py, pz);
                Iaxis[a] = Model->body_inertia[3 * BodyId + 0] * px * px
                         + Model->body_inertia[3 * BodyId + 1] * py * py
                         + Model->body_inertia[3 * BodyId + 2] * pz * pz;
            }

            static bool bLoggedIquat = false;
            if (!bLoggedIquat) {
                bLoggedIquat = true;
                UE_LOG(LogRovBridge, Log,
                    TEXT("[RovMuJoCoBridge][惯量诊断] body_iquat=(%.4f, %.4f, %.4f, %.4f)  "
                         "主轴主值=(%.3f, %.3f, %.3f)  机体轴等效惯量=(%.3f, %.3f, %.3f)"),
                    IQ[0], IQ[1], IQ[2], IQ[3],
                    Model->body_inertia[3 * BodyId + 0],
                    Model->body_inertia[3 * BodyId + 1],
                    Model->body_inertia[3 * BodyId + 2],
                    Iaxis[0], Iaxis[1], Iaxis[2]);
            }

            // 姿态诊断：每秒打印一次欧拉角、目标、误差、角速度和力矩
            {
                static int32 AttLogTick = -1;
                const int32 TickNow = (int32)(GetWorld()->GetTimeSeconds());
                if (TickNow != AttLogTick) {
                    AttLogTick = TickNow;
                    const double RollD  = FMath::RadiansToDegrees(FMath::Atan2(2.0*(Qw*Qx + Qy*Qz), 1.0 - 2.0*(Qx*Qx + Qy*Qy)));
                    const double PitchD = FMath::RadiansToDegrees(FMath::Asin(FMath::Clamp(2.0*(Qw*Qy - Qz*Qx), -1.0, 1.0)));
                    const double YawD   = FMath::RadiansToDegrees(FMath::Atan2(2.0*(Qw*Qz + Qx*Qy), 1.0 - 2.0*(Qy*Qy + Qz*Qz)));
                    UE_LOG(LogRovBridge, Log,
                        TEXT("[%s][姿态诊断] roll=%.1f pitch=%.1f yaw=%.1f targetYaw=%.1f |E|=(%.3f,%.3f,%.3f) omega=(%.2f,%.2f,%.2f)")
                        , TEXT("RovMuJoCoBridge"), RollD, PitchD, YawD, TargetYawDeg, E[0], E[1], E[2],
                        Data->qvel[AttDof+3], Data->qvel[AttDof+4], Data->qvel[AttDof+5]);
                }
            }

            for (int32 Axis = 0; Axis < 3; ++Axis) {
                // 优先用复合惯量（含机械臂）；取不到时退回本体的等效惯量
                const double I = (EffInertia[Axis] > 0.0) ? EffInertia[Axis] : Iaxis[Axis];
                const double Kp = I * W * W;
                const double Kd = 2.0 * I * Zeta * W;
                const double Ki = I * W * W * W * KiF;
                const double Omega = Data->qvel[AttDof + 3 + Axis];
                double T = Kp * E[Axis] + Ki * IntArr[Axis] - Kd * Omega;
                T = FMath::Clamp(T, -1500.0, 1500.0);   // 力矩限幅，防止饱和发散
                if (Axis == 0) { TorqueX += T; }
                else if (Axis == 1) { TorqueY += T; }
                else { TorqueZ += T; }
            }
        }
    }

    // 【10】级联姿态控制（按调研报告 C1-C4）
    //   外环：姿态误差 -> 期望角速度（机体系）
    //   内环：期望角速度 - 实测角速度（qvel[3:6] 即机体系）-> 力矩
    //   力矩直接写 qfrc_applied 后三维（机体系），不再经 RotByQuat/mj_applyFT
    if (bEnableAttitudeHold && !bSpinTest && Model->body_jntnum[BodyId] >= 1) {
        const int32 NJ = Model->body_jntadr[BodyId];
        if (Model->jnt_type[NJ] == mjJNT_FREE) {
            const int32 ND = Model->jnt_dofadr[NJ];
            const double* QQ = &Data->xquat[4 * BodyId];
            const double qw = QQ[0], qx = QQ[1], qy = QQ[2], qz = QQ[3];

            if (!bAttitudeInitialized) {
                bAttitudeInitialized = true;
                const double InitYaw = FMath::Atan2(2.0 * (qw * qz + qx * qy),
                                                    1.0 - 2.0 * (qy * qy + qz * qz));
                TargetYawDeg = FMath::RadiansToDegrees(InitYaw);
                UE_LOG(LogRovBridge, Log,
                    TEXT("[RovMuJoCoBridge][姿态] 级联控制启用，锁定航向 %.1f°"), TargetYawDeg);
            }

            const double HalfYaw = FMath::DegreesToRadians(TargetYawDeg) * 0.5;
            const double Qtw = FMath::Cos(HalfYaw);
            const double Qtz = FMath::Sin(HalfYaw);

            // q_e = conj(q_c) ⊗ q_t （机体系），取短弧
            const double aw = qw, ax = -qx, ay = -qy, az = -qz;
            double Qew = aw * Qtw - ax * 0.0 - ay * 0.0 - az * Qtz;
            double Qex = aw * 0.0 + ax * Qtw + ay * Qtz - az * 0.0;
            double Qey = aw * 0.0 - ax * Qtz + ay * Qtw + az * 0.0;
            double Qez = aw * Qtz + ax * 0.0 - ay * 0.0 + az * Qtw;
            if (Qew < 0.0) { Qew = -Qew; Qex = -Qex; Qey = -Qey; Qez = -Qez; }

            const double E3[3] = { 2.0 * Qex, 2.0 * Qey, 2.0 * Qez };
            const double KpAtt[3] = { 1.5, 1.5, 1.0 };
            const double Wmax[3]  = { 0.8, 0.8, 0.6 };
            const double Wn = 5.0;
            static double AttInt[3] = { 0.0, 0.0, 0.0 };

            // 航向前馈（见【航向调整】）：按住 ←/→ 时直接给期望角速度。
            // 上限也要一起放大，否则前馈会被 Wmax 截掉，目标航向又会跑飞。
            const double YawFF = FMath::DegreesToRadians(YawCmdRateDeg);
            const double WmaxZ = (YawFF != 0.0)
                ? FMath::Max(Wmax[2], FMath::Abs(YawFF) + 0.2)
                : Wmax[2];

            for (int32 Axis = 0; Axis < 3; ++Axis) {
                const double I = (EffInertia[Axis] > 0.0) ? EffInertia[Axis]
                                                          : Model->body_inertia[3 * BodyId + Axis];
                const double Wlim = (Axis == 2) ? WmaxZ : Wmax[Axis];
                // 积分项：抵消模型里无法消除的恒定力矩（质心/浮心不重合、
                // 机械臂残余重量等），否则纯 P 会停在一个固定倾角上。
                AttInt[Axis] = FMath::Clamp(
                    AttInt[Axis] + E3[Axis] * (double)Model->opt.timestep, -2.0, 2.0);
                double Wsp = FMath::Clamp(KpAtt[Axis] * E3[Axis] + AttitudeIntegralGain * AttInt[Axis],
                                          -Wlim, Wlim);
                if (Axis == 2 && YawFF != 0.0) {
                    Wsp = FMath::Clamp(Wsp + YawFF, -WmaxZ, WmaxZ);
                }
                double T = I * Wn * (Wsp - Data->qvel[ND + 3 + Axis]);
                T = FMath::Clamp(T, -1500.0, 1500.0);
                if (Axis == 0) { TorqueX += T; }
                else if (Axis == 1) { TorqueY += T; }
                else { TorqueZ += T; }
            }

            // 航向诊断（每秒一次）：目标 / 实测 / 误差 / 机体 Z 角速度 / 前馈
            {
                static double LastYawLogT = -1.0;
                if (Data->time - LastYawLogT > 1.0) {
                    LastYawLogT = Data->time;
                    const double CurYawDeg = FMath::RadiansToDegrees(
                        FMath::Atan2(2.0 * (qw * qz + qx * qy),
                                     1.0 - 2.0 * (qy * qy + qz * qz)));
                    double YawErrDeg = (double)TargetYawDeg - CurYawDeg;
                    while (YawErrDeg >  180.0) { YawErrDeg -= 360.0; }
                    while (YawErrDeg < -180.0) { YawErrDeg += 360.0; }
                    // 真实倾角：机体 Z 轴与世界 Z 的夹角，不受欧拉角约定影响
                    const double UpZ = FMath::Clamp((double)Data->xmat[9 * BodyId + 8], -1.0, 1.0);
                    const double TiltDeg = FMath::RadiansToDegrees(FMath::Acos(UpZ));
                    UE_LOG(LogRovBridge, Log,
                        TEXT("[%s][姿态诊断] t=%.1f 倾角=%.1f° 目标=%.1f° 实测=%.1f° 误差=%.1f° "
                             "E3=(%.3f, %.3f, %.3f) w=(%.3f, %.3f, %.3f) 前馈=%+.0f°/s"),
                        TEXT("RovMuJoCoBridge"), Data->time, TiltDeg,
                        TargetYawDeg, CurYawDeg, YawErrDeg,
                        E3[0], E3[1], E3[2],
                        Data->qvel[ND + 3], Data->qvel[ND + 4], Data->qvel[ND + 5],
                        YawCmdRateDeg);
                }
            }
        }
    }
    // 【11】机械臂静力矩配平：常值前馈，把控制器硬扛的那部分稳态力矩抵掉
    if (bEnableAttitudeHold && !bSpinTest) {
        float TrimPitch = TrimTorquePitch;
        float TrimRoll  = TrimTorqueRoll;
        FParse::Value(FCommandLine::Get(), TEXT("RovTrimPitch="), TrimPitch);
        FParse::Value(FCommandLine::Get(), TEXT("RovTrimRoll="), TrimRoll);
        TorqueX += TrimRoll;
        TorqueY += TrimPitch;
    }

    CachedBodyId.store(BodyId);
    CachedForceX.store(Fx);
    CachedForceY.store(Fy);
    CachedForceZ.store(Fz);

    // 力矩轴自检：覆盖掉所有力矩项，只给指定轴 100 N·m
    if (bTorqueAxisTest && TorqueTestAxis >= 0) {
        TorqueX = (TorqueTestAxis == 0) ? 100.0 : 0.0;
        TorqueY = (TorqueTestAxis == 1) ? 100.0 : 0.0;
        TorqueZ = (TorqueTestAxis == 2) ? 100.0 : 0.0;

        static int32 TorqueLogTick = -1;
        const int32 Tick = (int32)(GetWorld()->GetTimeSeconds() * 4.0);
        if (Tick != TorqueLogTick && Model->body_jntnum[BodyId] >= 1) {
            TorqueLogTick = Tick;
            const int32 J = Model->body_jntadr[BodyId];
            if (Model->jnt_type[J] == mjJNT_FREE) {
                const int32 D = Model->jnt_dofadr[J];
                UE_LOG(LogRovBridge, Log,
                    TEXT("[RovMuJoCoBridge][力矩轴自检] 给轴%d 100N·m  ->  omega=(%.4f, %.4f, %.4f)"),
                    TorqueTestAxis, Data->qvel[D + 3], Data->qvel[D + 4], Data->qvel[D + 5]);
            }
        }
    }

    // 力矩就是机体系，直接缓存；ApplyCachedForce 里直写 qfrc_applied[3..5]
    CachedTorqueX.store(TorqueX);
    CachedTorqueY.store(TorqueY);
    CachedTorqueZ.store(TorqueZ);
    CachedBuoyancy.store(Buoyancy);

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
    if (BodyId < 0 || !Model || !Data || BodyId >= Model->nbody) {
        return;
    }

    // =========================================================================
    // 通道：mj_applyFT 直接累加进 qfrc_applied。
    //
    // 为什么不用其它两种写法：
    //   - xfrc_applied：元素顺序有两套说法（官方文档力在前，URLab 的
    //     MjBind.h:574 注释写力矩在前），写反会让"向前"变成"俯仰"。
    //   - 直接写 qfrc_applied 后三维：实测轴系对不上 —— 只给"轴 0"
    //     100 N·m，结果 omega 主要绕 Y 转（-1.03 对 -0.24）。
    //     姿态控制器的力矩因此打到了错误轴上，积分也在错误轴上卷。
    //
    // mj_applyFT 把 force / torque / point 分开传参、**全部世界系**，
    // 自动补 r×F，没有布局和轴系歧义，还能让浮力正确地作用在浮心。
    // =========================================================================
    if (Model->body_jntnum[BodyId] < 1) {
        return;
    }
    const int32 JntId = Model->body_jntadr[BodyId];
    if (Model->jnt_type[JntId] != mjJNT_FREE) {
        return;
    }
    const int32 DofAdr = Model->jnt_dofadr[JntId];

    // mj_applyFT 是累加语义，先清零本 body 的 6 维广义力
    for (int32 k = 0; k < 6; ++k) {
        Data->qfrc_applied[DofAdr + k] = 0.0;
    }

    const mjtNum CG[3] = {
        Data->xipos[3 * BodyId + 0],
        Data->xipos[3 * BodyId + 1],
        Data->xipos[3 * BodyId + 2]
    };
    const mjtNum ZeroT[3] = { 0.0, 0.0, 0.0 };
    const mjtNum ZeroF[3] = { 0.0, 0.0, 0.0 };

    // 【1】推进器推力 + 洋流拖曳：作用在质心
    mjtNum F1[3] = {
        (mjtNum)CachedForceX.load(),
        (mjtNum)CachedForceY.load(),
        (mjtNum)CachedForceZ.load()
    };
    mj_applyFT(Model, Data, F1, ZeroT, CG, BodyId, Data->qfrc_applied);

    // 【2】浮力：作用在**整机浮心** = 整机合成质心 + R·(0,0,BG)
    //
    // 浮力必须加在「整机（艇体 + 机械臂）的合成质心」上，而不是艇体自身的质心：
    // 排水体积属于整个载体，浮心跟着整机质量分布走。之前加在艇体质心上，
    // 臂的 22 kg 把整机质心拉偏了约 18 mm，4518 N × 18 mm ≈ 81 N·m 的恒定力矩，
    // 姿态控制器只能靠 28° 的倾斜去平衡（实测 roll = −27.9°）。
    const double Buoy = CachedBuoyancy.load();

    double TotalMass = 0.0, Cx = 0.0, Cy = 0.0, Cz = 0.0;
    for (int32 b = 0; b < Model->nbody; ++b) {
        bool bInSubtree = (b == BodyId);
        if (!bInSubtree) {
            int32 p = Model->body_parentid[b];
            while (p > 0) {
                if (p == BodyId) { bInSubtree = true; break; }
                p = Model->body_parentid[p];
            }
        }
        if (!bInSubtree) { continue; }
        const double m = Model->body_mass[b];
        TotalMass += m;
        Cx += m * Data->xipos[3 * b + 0];
        Cy += m * Data->xipos[3 * b + 1];
        Cz += m * Data->xipos[3 * b + 2];
    }
    if (TotalMass > 1e-6) {
        Cx /= TotalMass; Cy /= TotalMass; Cz /= TotalMass;
    } else {
        Cx = CG[0]; Cy = CG[1]; Cz = CG[2];
    }
    // 用 MuJoCo 自己算的子树质心做校验：两者必须一致，否则说明重力力矩和浮力
    // 作用点不重合，会留下一个恒定的姿态误差（曾经差 17 mm ≈ 78 N·m）。
    if (Data->subtree_com) {
        Cx = Data->subtree_com[3 * BodyId + 0];
        Cy = Data->subtree_com[3 * BodyId + 1];
        Cz = Data->subtree_com[3 * BodyId + 2];
    }

    if (Buoy > 0.0) {
        double Rx = 0.0, Ry = 0.0, Rz = 0.0;
        RotByQuat(Data->xquat[4 * BodyId + 0], Data->xquat[4 * BodyId + 1],
                  Data->xquat[4 * BodyId + 2], Data->xquat[4 * BodyId + 3],
                  0.0, 0.0, (double)BuoyancyArm, Rx, Ry, Rz);
        mjtNum F2[3] = { 0.0, 0.0, (mjtNum)Buoy };
        mjtNum CBw[3] = {
            (mjtNum)(Cx + Rx),
            (mjtNum)(Cy + Ry),
            (mjtNum)(Cz + Rz)
        };
        mj_applyFT(Model, Data, F2, ZeroT, CBw, BodyId, Data->qfrc_applied);
    }

    // 【2b】机械臂浮力：按真实排水体积，在臂的合成质心处施加向上浮力
    //
    // 为什么必须给臂加浮力：UR5e 是空气中的工业臂，密度远大于水。
    // 整条臂 22.0 kg、排水只有约 12 L，在水里净重约 98 N，挂在艇体前方 0.13 m 处，
    // 会持续产生约 8 N·m 的俯仰力矩。真实 ROV 的做法就是给臂绑浮力块（泡沫/浮筒），
    // 这里按「等排水体积」把它补上，而不是用常值力矩硬顶。
    if (ArmBuoyancyVolume > 0.0f) {
        double ArmMass = 0.0, Cx = 0.0, Cy = 0.0, Cz = 0.0;
        for (int32 b = 0; b < Model->nbody; ++b) {
            int32 p = Model->body_parentid[b];
            bool bDescendant = false;
            while (p > 0) {
                if (p == BodyId) { bDescendant = true; break; }
                p = Model->body_parentid[p];
            }
            if (!bDescendant) { continue; }
            const double m = Model->body_mass[b];
            ArmMass += m;
            Cx += m * Data->xipos[3 * b + 0];
            Cy += m * Data->xipos[3 * b + 1];
            Cz += m * Data->xipos[3 * b + 2];
        }
        if (ArmMass > 1e-6) {
            const double VolM3 = (double)ArmBuoyancyVolume * 0.001 * (double)ArmBuoyancyScale;
            const double FArm = (double)FluidDensity * 9.81 * VolM3;
            mjtNum F3[3] = { 0.0, 0.0, (mjtNum)FArm };
            mjtNum CoB[3] = {
                (mjtNum)(Cx / ArmMass),
                (mjtNum)(Cy / ArmMass),
                (mjtNum)(Cz / ArmMass)
            };
            mj_applyFT(Model, Data, F3, ZeroT, CoB, BodyId, Data->qfrc_applied);
        }
    }

    // 【3】姿态力矩：free joint 的 qfrc_applied[3..5] 就是机体系力矩，直接赋值
    Data->qfrc_applied[DofAdr + 3] = (mjtNum)CachedTorqueX.load();
    Data->qfrc_applied[DofAdr + 4] = (mjtNum)CachedTorqueY.load();
    Data->qfrc_applied[DofAdr + 5] = (mjtNum)CachedTorqueZ.load();

    // 【4】一次性初始化：implicit 积分器 + 转动 dof 阻尼
    static bool bCtlInitDone = false;
    if (!bCtlInitDone) {
        bCtlInitDone = true;
        Model->opt.integrator = mjINT_IMPLICIT;
        const double Iavg = 0.5 * (Model->body_inertia[3 * BodyId + 0]
                                 + Model->body_inertia[3 * BodyId + 1]
                                 + Model->body_inertia[3 * BodyId + 2]) / 3.0;
        // 阻尼取值优先级：命令行清零（做流体基线对照）> 属性 > 0.5×平均惯量兜底
        static const bool bNoDofDamp = FParse::Param(FCommandLine::Get(), TEXT("RovNoDofDamp"));
        double Idamp = (AngularDofDamping > 0.0f) ? (double)AngularDofDamping : Iavg;
        if (bNoDofDamp) { Idamp = 0.0; }
        Model->dof_damping[DofAdr + 3] = Idamp;
        Model->dof_damping[DofAdr + 4] = Idamp;
        Model->dof_damping[DofAdr + 5] = Idamp;
        UE_LOG(LogRovBridge, Log, TEXT("[RovBridge][版本] mj_versionString=%s"), UTF8_TO_TCHAR(mj_versionString()));
        UE_LOG(LogRovBridge, Log,
            TEXT("[RovBridge][惯量] body_inertia=(%.3f, %.3f, %.3f)  EffInertia=(%.3f, %.3f, %.3f)  body_mass=%.3f"),
            Model->body_inertia[3 * BodyId + 0], Model->body_inertia[3 * BodyId + 1],
            Model->body_inertia[3 * BodyId + 2],
            EffInertia[0], EffInertia[1], EffInertia[2], Model->body_mass[BodyId]);
        UE_LOG(LogRovBridge, Log, TEXT("[RovBridge][控制] integrator=implicit  dof_damping=%.2f N.m.s/rad"), Idamp);

        // 机械臂质量分布诊断：-RovArmDiag
        // 目的：确认「臂子树 181 kg」是怎么来的（menagerie 原版 UR5e + 2F85 只有 ~21.9 kg），
        //       顺便把每个连杆的质量/位置/碰撞体尺寸打出来，判断是不是整体被放大了。
        static const bool bArmDiag = FParse::Param(FCommandLine::Get(), TEXT("RovArmDiag"));
        if (bArmDiag) {
            const double* RovCg = &Data->xipos[3 * BodyId];
            double ArmMass = 0.0, Mx = 0.0, My = 0.0, Mz = 0.0;
            UE_LOG(LogRovBridge, Log,
                TEXT("[RovBridge][臂诊断] ROV body[%d]='%s' mass=%.3f subtree=%.3f  惯量=(%.3f, %.3f, %.3f)"),
                BodyId, UTF8_TO_TCHAR(Model->names + Model->name_bodyadr[BodyId]),
                Model->body_mass[BodyId], Model->body_subtreemass[BodyId],
                Model->body_inertia[3 * BodyId + 0], Model->body_inertia[3 * BodyId + 1],
                Model->body_inertia[3 * BodyId + 2]);
            for (int32 b = 0; b < Model->nbody; ++b) {
                int32 p = Model->body_parentid[b];
                bool bDescendant = false;
                while (p > 0) {
                    if (p == BodyId) { bDescendant = true; break; }
                    p = Model->body_parentid[p];
                }
                if (!bDescendant) { continue; }

                const double m = Model->body_mass[b];
                const double* x = &Data->xipos[3 * b];
                const double rx = x[0] - RovCg[0], ry = x[1] - RovCg[1], rz = x[2] - RovCg[2];
                ArmMass += m;
                Mx += m * rx; My += m * ry; Mz += m * rz;
                UE_LOG(LogRovBridge, Log,
                    TEXT("  [臂] body[%d]='%s' mass=%.4f  相对ROV重心=(%.4f, %.4f, %.4f) m  惯量=(%.5f, %.5f, %.5f)"),
                    b, UTF8_TO_TCHAR(Model->names + Model->name_bodyadr[b]), m, rx, ry, rz,
                    Model->body_inertia[3 * b + 0], Model->body_inertia[3 * b + 1],
                    Model->body_inertia[3 * b + 2]);

                // 该 body 的碰撞几何尺寸，用来判断几何有没有被整体放大
                for (int32 g = 0; g < Model->ngeom; ++g) {
                    if (Model->geom_bodyid[g] != b) { continue; }
                    UE_LOG(LogRovBridge, Log,
                        TEXT("      [geom] '%s' type=%d size=(%.4f, %.4f, %.4f) mass=%.4f"),
                        UTF8_TO_TCHAR(Model->names + Model->name_geomadr[g]),
                        (int32)Model->geom_type[g],
                        Model->geom_size[3 * g + 0], Model->geom_size[3 * g + 1],
                        Model->geom_size[3 * g + 2], Model->body_mass[b]);
                }
            }
            if (ArmMass > 1e-6) {
                const double G = 9.81;
                // 重力矩 = Σ r × (0,0,-mg) = (-g·Σm·ry, +g·Σm·rx, 0)
                UE_LOG(LogRovBridge, Log,
                    TEXT("[RovBridge][臂诊断] 合计 mass=%.3f  合成质心相对ROV=(%.4f, %.4f, %.4f) m  "
                         "重力矩=(%.1f, %.1f, %.1f) N·m  重量=%.1f N"),
                    ArmMass, Mx / ArmMass, My / ArmMass, Mz / ArmMass,
                    -G * My, G * Mx, 0.0, ArmMass * G);
            }
        }
    }

    // 【5】qfrc_applied 累加监测（每秒一次）
    {
        static double LastQfrcLogT = -1.0;
        if (Data->time - LastQfrcLogT > 1.0) {
            LastQfrcLogT = Data->time;
            const double* qq = &Data->xquat[4 * BodyId];
            const double RollDeg = FMath::RadiansToDegrees(
                FMath::Atan2(2.0 * (qq[0] * qq[1] + qq[2] * qq[3]),
                             1.0 - 2.0 * (qq[1] * qq[1] + qq[2] * qq[2])));
            const double PitchDeg = FMath::RadiansToDegrees(
                FMath::Asin(FMath::Clamp(2.0 * (qq[0] * qq[2] - qq[3] * qq[1]), -1.0, 1.0)));
            UE_LOG(LogRovBridge, Log,
                TEXT("[RovBridge][qfrc] t=%.2f applied=(%.1f, %.1f, %.1f, %.1f, %.1f, %.1f)  "
                     "roll=%.1f° pitch=%.1f°  Z=%.3f  ncon=%d  bias=(%.1f, %.1f, %.1f)  "
                     "constr=(%.1f, %.1f, %.1f)"),
                Data->time,
                Data->qfrc_applied[DofAdr + 0], Data->qfrc_applied[DofAdr + 1],
                Data->qfrc_applied[DofAdr + 2], Data->qfrc_applied[DofAdr + 3],
                Data->qfrc_applied[DofAdr + 4], Data->qfrc_applied[DofAdr + 5],
                RollDeg, PitchDeg, Data->xpos[3 * BodyId + 2], (int32)Data->ncon,
                Data->qfrc_bias[DofAdr + 3], Data->qfrc_bias[DofAdr + 4], Data->qfrc_bias[DofAdr + 5],
                Data->qfrc_constraint[DofAdr + 3], Data->qfrc_constraint[DofAdr + 4],
                Data->qfrc_constraint[DofAdr + 5]);
        }
    }
    // 诊断
    static int32 ApplyLogCounter = 0;
    if (++ApplyLogCounter % 500 == 0) {
        UE_LOG(LogRovBridge, Log,
            TEXT("[RovMuJoCoBridge][通道] mj_applyFT dof=%d  F=(%.1f,%.1f,%.1f) N  "
                 "浮力=%.1f N  T=(%.1f,%.1f,%.1f) N·m  Z=%.3f"),
            DofAdr,
            Data->qfrc_applied[DofAdr + 0], Data->qfrc_applied[DofAdr + 1], Data->qfrc_applied[DofAdr + 2],
            Buoy, Data->qfrc_applied[DofAdr + 3], Data->qfrc_applied[DofAdr + 4], Data->qfrc_applied[DofAdr + 5],
            Data->xpos[3 * BodyId + 2]);
    }
}
