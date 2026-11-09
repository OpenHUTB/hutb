// Copyright (c) OpenHUTB. All rights reserved.
// 实现见头文件注释。

#include "Vehicles/Rov/RovMuJoCoBridge.h"

#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameFramework/PlayerController.h"
#include "Components/PrimitiveComponent.h"
#include "Kismet/GameplayStatics.h"
#include "InputCoreTypes.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

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
    if (ContactLogArchive)
    {
        ContactLogArchive->Close();
        delete ContactLogArchive;
        ContactLogArchive = nullptr;
    }
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

// 键盘推进
//
// 推力通过 MuJoCo 的 xfrc_applied 施加：桥接关闭了 ARovPawn 的物理与碰撞，
// AirSim 的 RC 通道没有物理体承接；模型里的 actuator 只有机械臂关节。
// xfrc_applied 不会被 mj_step 自动清零，每帧需先清再写。
// 力的量级按质量缩放，换不同质量的 ROV 手感一致。

void URovMuJoCoBridge::TickComponent(float DeltaTime, ELevelTick TickType,
                                     FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    if (bBridgeActive && bEnableKeyboardControl) {
        ApplyControlForce();
    }
}

void URovMuJoCoBridge::ApplyConfigFile()
{
    if (!bLoadConfigFile)
    {
        UE_LOG(LogRovBridge, Log, TEXT("[配置] bLoadConfigFile=false，跳过配置文件"));
        return;
    }

    FString Path;
    if (!FParse::Value(FCommandLine::Get(), TEXT("RovConfig="), Path))
    {
        Path = FPaths::Combine(FPaths::ProjectConfigDir(), TEXT("RovParams.json"));
    }
    Path = FPaths::ConvertRelativePathToFull(Path);
    if (!FPaths::FileExists(Path))
    {
        UE_LOG(LogRovBridge, Warning, TEXT("[配置] 未找到配置文件 %s，使用类默认值 + 命令行覆盖"), *Path);
        return;
    }

    TMap<FString, float> Kv;
    FString Err;
    if (!RovConfig::LoadKeyValues(Path, Kv, Err))
    {
        UE_LOG(LogRovBridge, Warning, TEXT("[配置] %s"), *Err);
        return;
    }

    auto ApplyF = [&Kv](const TCHAR* Key, float& Target)
    {
        if (const float* V = Kv.Find(Key)) { Target = *V; }
    };
    auto ApplyB = [&Kv](const TCHAR* Key, bool& Target)
    {
        if (const float* V = Kv.Find(Key)) { Target = (*V) != 0.0f; }
    };
    auto ApplyV = [&Kv](const TCHAR* Prefix, FVector& Target)
    {
        const FString P(Prefix);
        if (const float* X = Kv.Find(P + TEXT("X"))) { Target.X = *X; }
        if (const float* Y = Kv.Find(P + TEXT("Y"))) { Target.Y = *Y; }
        if (const float* Z = Kv.Find(P + TEXT("Z"))) { Target.Z = *Z; }
    };

    ApplyF(TEXT("FluidDensity"), FluidDensity);
    ApplyF(TEXT("FluidViscosity"), FluidViscosity);
    ApplyF(TEXT("DisplacedVolume"), DisplacedVolume);
    ApplyF(TEXT("BuoyancyScale"), BuoyancyScale);
    ApplyF(TEXT("BuoyancyArm"), BuoyancyArm);
    ApplyF(TEXT("ArmBuoyancyVolume"), ArmBuoyancyVolume);
    ApplyF(TEXT("ArmBuoyancyScale"), ArmBuoyancyScale);
    ApplyF(TEXT("ArmJointDamping"), ArmJointDamping);
    ApplyF(TEXT("ArmJointStiffness"), ArmJointStiffness);
    ApplyB(TEXT("ArmGravityComp"), bArmGravityComp);
    ApplyF(TEXT("ThrusterMaxRatio"), ThrusterMaxRatio);
    ApplyF(TEXT("WaterSurfaceZ"), WaterSurfaceZ);
    ApplyF(TEXT("AngularDofDamping"), AngularDofDamping);
    ApplyF(TEXT("AngularDampingCoeff"), AngularDampingCoeff);
    ApplyF(TEXT("AttitudeBandwidth"), AttitudeBandwidth);
    ApplyF(TEXT("AttitudeZeta"), AttitudeZeta);
    ApplyF(TEXT("AttitudeIntegralGain"), AttitudeIntegralGain);
    ApplyF(TEXT("LevelBandwidth"), LevelBandwidth);
    ApplyF(TEXT("LevelZeta"), LevelZeta);
    ApplyF(TEXT("YawBandwidth"), YawBandwidth);
    ApplyF(TEXT("YawZeta"), YawZeta);
    ApplyF(TEXT("YawRateDegPerSec"), YawRateDegPerSec);
    ApplyF(TEXT("CurrentDragCoeff"), CurrentDragCoeff);
    ApplyF(TEXT("OceanCurrentMeanSpeed"), OceanCurrentMeanSpeed);
    ApplyB(TEXT("EnableAttitudeHold"), bEnableAttitudeHold);
    ApplyB(TEXT("EnableDepthHold"), bEnableDepthHold);
    ApplyB(TEXT("EnableOceanCurrent"), bEnableOceanCurrent);
    ApplyB(TEXT("UseNativeFluid"), bUseNativeFluid);

    ApplyB(TEXT("UseExplicitDrag"), bUseExplicitDrag);
    ApplyV(TEXT("DragCoeff"), DragCoeff);
    ApplyV(TEXT("DragRefArea"), DragRefArea);
    ApplyB(TEXT("UseAddedMass"), bUseAddedMass);
    ApplyV(TEXT("AddedMass"), AddedMass);

    ApplyB(TEXT("AutoEstimateFluidParams"), bAutoEstimateFluidParams);
    ApplyV(TEXT("HullDims"), HullDims);
    if (const float* S = Kv.Find(TEXT("HullShape")))
    {
        HullShape = static_cast<ERovHullShape>(FMath::Clamp(FMath::RoundToInt(*S), 0, 3));
    }
    if (bAutoEstimateFluidParams)
    {
        const FRovFluidEstimate E = URovFluidLibrary::EstimateFluidParams(HullShape, HullDims, FluidDensity);
        DragCoeff = FVector(E.Cd, E.Cd, E.Cd);
        DragRefArea = FVector(E.RefArea, E.RefArea, E.RefArea);
        AddedMass = FVector(E.AddedMass, E.AddedMass, E.AddedMass);
        bUseExplicitDrag = true;
        bUseAddedMass = true;
        UE_LOG(LogRovBridge, Log,
            TEXT("[配置] 外形估算：Cd=%.2f  迎流面积=%.4f m²  附加质量=%.1f kg  排水体积=%.4f m³"),
            E.Cd, E.RefArea, E.AddedMass, E.Volume);
    }

    ApplyB(TEXT("LogContacts"), bLogContacts);
    if (const float* V = Kv.Find(TEXT("ContactLogInterval")))
    {
        ContactLogInterval = FMath::Max(1, FMath::RoundToInt(*V));
    }

    LoadedConfigPath = Path;
    UE_LOG(LogRovBridge, Log, TEXT("[配置] 已加载 %s，共 %d 个数值项"), *Path, Kv.Num());
}

void URovMuJoCoBridge::LogContacts(mjModel_* Model, mjData_* Data)
{
    if (!bLogContacts || !Model || !Data)
    {
        return;
    }
    if (++ContactLogStep % FMath::Max(1, ContactLogInterval) != 0)
    {
        return;
    }

    if (!ContactLogArchive)
    {
        FString Path = ContactLogPath.IsEmpty()
            ? FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("RovContactLog.csv"))
            : ContactLogPath;
        Path = FPaths::ConvertRelativePathToFull(Path);
        ContactLogArchive = IFileManager::Get().CreateFileWriter(*Path, FILEWRITE_Append);
        if (!ContactLogArchive)
        {
            UE_LOG(LogRovBridge, Warning, TEXT("[接触日志] 无法写入 %s"), *Path);
            bLogContacts = false;
            return;
        }
        UE_LOG(LogRovBridge, Log, TEXT("[接触日志] 写入 %s（每 %d 个物理步一条）"), *Path, ContactLogInterval);
    }

    auto WriteLine = [this](const FString& Line)
    {
        FTCHARToUTF8 Utf8(*Line);
        ContactLogArchive->Serialize(static_cast<void*>(const_cast<ANSICHAR*>(Utf8.Get())), Utf8.Length());
    };

    if (!bContactLogHeaderWritten)
    {
        WriteLine(TEXT("time_s,body1,body2,normal_N,pos_x,pos_y,pos_z\n"));
        bContactLogHeaderWritten = true;
    }

    for (int32 i = 0; i < Data->ncon; ++i)
    {
        const mjContact& C = Data->contact[i];
        mjtNum F6[6] = { 0, 0, 0, 0, 0, 0 };
        mj_contactForce(Model, Data, i, F6);

        const int32 B1 = Model->geom_bodyid[C.geom1];
        const int32 B2 = Model->geom_bodyid[C.geom2];
        const FString N1 = (B1 >= 0 && Model->name_bodyadr[B1] >= 0)
            ? FString(UTF8_TO_TCHAR(Model->names + Model->name_bodyadr[B1])) : FString(TEXT("world"));
        const FString N2 = (B2 >= 0 && Model->name_bodyadr[B2] >= 0)
            ? FString(UTF8_TO_TCHAR(Model->names + Model->name_bodyadr[B2])) : FString(TEXT("world"));

        WriteLine(FString::Printf(TEXT("%.5f,%s,%s,%.4f,%.5f,%.5f,%.5f\n"),
            Data->time, *N1, *N2, F6[0], C.pos[0], C.pos[1], C.pos[2]));
    }

    // 立即落盘，避免进程被强杀时缓冲区内容丢失
    ContactLogArchive->Flush();
}

void URovMuJoCoBridge::GetStatusSnapshot(FRovStatusSnapshot& Out) const
{
    Out.RollDeg = (float)CachedRollDeg.load();
    Out.PitchDeg = (float)CachedPitchDeg.load();
    Out.YawDeg = (float)CachedYawDeg.load();
    Out.DepthM = (float)CachedDepthM.load();
    Out.ThrustN = (float)CachedThrustN.load();
    Out.ContactForceN = (float)CachedContactForce.load();
    Out.VelocityMps = FVector((float)CachedVelocityX.load(), (float)CachedVelocityY.load(), (float)CachedVelocityZ.load());
    Out.ContactPoint = FVector((float)CachedContactPointX.load(), (float)CachedContactPointY.load(), (float)CachedContactPointZ.load());
    Out.ContactForceDir = FVector((float)CachedContactDirX.load(), (float)CachedContactDirY.load(), (float)CachedContactDirZ.load());
    Out.FlowVelocityMps = FVector((float)CachedFlowX.load(), (float)CachedFlowY.load(), (float)CachedFlowZ.load());
    Out.bValid = bBridgeActive && TargetBody.IsValid();
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
        // 配置文件先加载，命令行随后覆盖，保证「命令行 > 配置文件 > 默认值」
        ApplyConfigFile();
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
        if (FParse::Param(FCommandLine::Get(), TEXT("RovContactTest")))
        {
            ContactTestForce = 900.0;
            bLogContacts = true;
            bEnableDepthHold = false;
            UE_LOG(LogRovBridge, Log, TEXT("[接触试验] 施加 %.0f N 下压力，用于产生海床接触"), ContactTestForce);
        }
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

        // 复合惯量：先用本体惯量兜底，等仿真真正跑起来再换成 6×6 质量阵的值。
        // （qM 只有在 mj_forward/mj_step 之后才有效，首帧读会拿到垃圾值；
        //   打包版首帧时序和编辑器不同，曾读到极小值 → 控制器力矩≈0 → ROV 缓慢翻转。）
        for (int32 a = 0; a < 3; ++a) {
            EffInertia[a] = Model->body_inertia[3 * BodyId + a];
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

    // 复合惯量延迟计算：qM 只在 mj_forward/mj_step 之后有效，
    // 因此等仿真时间过 0.2 s 再取，并对量级做下限校验。
    {
        static bool bEffInertiaDone = false;
        if (!bEffInertiaDone && Data->time > 0.2 && Data->qM) {
            bEffInertiaDone = true;
            const int32 Nv = Model->nv;
            bool bOk = (Nv >= 6);
            double Vals[3] = { 0.0, 0.0, 0.0 };
            if (bOk) {
                TArray<mjtNum> M;
                M.SetNumZeroed(Nv * Nv);
                mj_fullM(Model, M.GetData(), Data->qM);
                for (int32 a = 0; a < 3; ++a) {
                    const double V = M[(3 + a) * Nv + (3 + a)];
                    // 这台 ROV 的转动惯量量级是几十 kg·m²，小于 1 一律视为无效
                    if (!FMath::IsFinite(V) || V < 1.0) { bOk = false; break; }
                    Vals[a] = V;
                }
            }
            if (bOk) {
                for (int32 a = 0; a < 3; ++a) { EffInertia[a] = Vals[a]; }
            }
            UE_LOG(LogRovBridge, Log,
                TEXT("[RovMuJoCoBridge][复合惯量] 延迟计算 t=%.2f  "
                     "EffInertia=(%.3f, %.3f, %.3f)  来源=%s"),
                Data->time, EffInertia[0], EffInertia[1], EffInertia[2],
                bOk ? TEXT("6x6 质量阵") : TEXT("body_inertia 回退"));
        }
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
    // 方向按画面定义：MuJoCo→UE 为镜像映射（Y 取反），MuJoCo 的 +yaw 在画面上是 -yaw，
    // 因此 → = 画面右转 = MuJoCo yaw 减小。
    // 按住期间同时给出角速度前馈，避免目标航向跑得比机身快导致误差累积过 180°。
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

    // 航向角速度前馈交给物理线程的姿态环
    CachedYawFFRadPerSec.store(FMath::DegreesToRadians(YawCmdRateDeg));

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

    // 【9】附加角阻尼：τ = -Kd·ω（机体系），与流体模型叠加。
    // Kd = AngularDampingCoeff × 平均主惯量。
    double TorqueX = 0.0, TorqueY = 0.0, TorqueZ = 0.0;
    double WorldTorqueX = 0.0, WorldTorqueY = 0.0, WorldTorqueZ = 0.0;
    if (AngularDampingCoeff > 0.0f && Model->body_jntnum[BodyId] >= 1) {
        const int32 DampJnt = Model->body_jntadr[BodyId];
        if (Model->jnt_type[DampJnt] == mjJNT_FREE) {
            const int32 DampDof = Model->jnt_dofadr[DampJnt];
            // 用复合惯量：body_inertia 只含本体，不含机械臂。
            const double Iavg = (EffInertia[0] + EffInertia[1] + EffInertia[2]) / 3.0;
            const double Kd = (double)AngularDampingCoeff * Iavg;
            TorqueX += -Kd * Data->qvel[DampDof + 3];
            TorqueY += -Kd * Data->qvel[DampDof + 4];
            TorqueZ += -Kd * Data->qvel[DampDof + 5];
        }
    }

    // 旧的四元数 PD 姿态控制器（已停用，保留备查）。
    // 误差 e = 2·sign(qw)·qv，增益按惯量缩放；由下方【10】级联控制器取代。
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

    // 【10】级联姿态控制（已迁移到物理线程，见 ComputeAttitudeTorque）
    //   保留本块仅作参考：它按渲染帧调用，帧率低于约 25 fps 时会与积分项形成
    //   慢振荡直至翻转，因此改为在物理线程按 500 Hz 运行。
    //   外环：姿态误差 -> 期望角速度（机体系）
    //   内环：期望角速度 - 实测角速度（qvel[3:6] 为机体系）-> 力矩
    if (false && bEnableAttitudeHold && !bSpinTest && Model->body_jntnum[BodyId] >= 1) {
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

            // 积分步长必须用「距上次调用实际经过的仿真时间」，不能用固定 timestep：
            // ApplyControlForce 是按渲染帧调用的，帧率不同积分速度就不同
            // （Development 约 60 fps、Shipping 约 120+ fps）。用固定 timestep 会让
            // 打包版的积分快一倍，恒力矩还没抵掉就先过冲，表现为缓慢翻转。
            static double LastIntTime = -1.0;
            double IntDt = 0.0;
            {
                const double NowT = (double)Data->time;
                if (LastIntTime >= 0.0) {
                    IntDt = FMath::Clamp(NowT - LastIntTime, 0.0, 0.05);
                }
                LastIntTime = NowT;
            }

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
                AttInt[Axis] = FMath::Clamp(AttInt[Axis] + E3[Axis] * IntDt, -2.0, 2.0);
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
    CachedControlTime.store((double)Data->time);
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

void URovMuJoCoBridge::ComputeAttitudeTorque(mjModel_* Model, mjData_* Data, int32 BodyId, double Dt, double OutTorque[3])
{
    OutTorque[0] = 0.0;
    OutTorque[1] = 0.0;
    OutTorque[2] = 0.0;

    if (!bEnableAttitudeHold || !Model || !Data || BodyId < 0 || BodyId >= Model->nbody)
    {
        return;
    }
    if (Model->body_jntnum[BodyId] < 1)
    {
        return;
    }
    const int32 J = Model->body_jntadr[BodyId];
    if (Model->jnt_type[J] != mjJNT_FREE)
    {
        return;
    }
    const int32 D = Model->jnt_dofadr[J];

    const double* QQ = &Data->xquat[4 * BodyId];
    const double qw = QQ[0], qx = QQ[1], qy = QQ[2], qz = QQ[3];

    if (!bAttitudeInitialized)
    {
        bAttitudeInitialized = true;
        const double InitYaw = FMath::Atan2(2.0 * (qw * qz + qx * qy),
                                            1.0 - 2.0 * (qy * qy + qz * qz));
        TargetYawDeg = (float)FMath::RadiansToDegrees(InitYaw);
        UE_LOG(LogRovBridge, Log,
            TEXT("[RovBridge][姿态] 物理线程级联控制启用（%.0f Hz），锁定航向 %.1f°"),
            1.0 / FMath::Max(Dt, 1e-6), TargetYawDeg);
    }

    const double HalfYaw = FMath::DegreesToRadians((double)TargetYawDeg) * 0.5;
    const double Qtw = FMath::Cos(HalfYaw), Qtz = FMath::Sin(HalfYaw);

    // q_e = conj(q_c) ⊗ q_t（机体系），取短弧
    const double aw = qw, ax = -qx, ay = -qy, az = -qz;
    double Qew = aw * Qtw - az * Qtz;
    double Qex = ax * Qtw + ay * Qtz;
    double Qey = -ax * Qtz + ay * Qtw;
    double Qez = aw * Qtz + az * Qtw;
    if (Qew < 0.0) { Qew = -Qew; Qex = -Qex; Qey = -Qey; Qez = -Qez; }

    const double E3[3] = { 2.0 * Qex, 2.0 * Qey, 2.0 * Qez };
    const double KpAtt[3] = { 1.5, 1.5, 1.0 };
    const double Wmax[3] = { 0.8, 0.8, 0.6 };
    const double Wn = 5.0;

    const double YawFF = CachedYawFFRadPerSec.load();
    const double WmaxZ = (YawFF != 0.0) ? FMath::Max(Wmax[2], FMath::Abs(YawFF) + 0.2) : Wmax[2];

    for (int32 Axis = 0; Axis < 3; ++Axis)
    {
        const double I = (EffInertia[Axis] > 0.0) ? EffInertia[Axis]
                                                  : Model->body_inertia[3 * BodyId + Axis];
        const double Wlim = (Axis == 2) ? WmaxZ : Wmax[Axis];

        // 大角度时临时抬高角速度上限，给回正留出力矩余量。
        // 稳态扰动力矩约 86 N·m；若上限恒为 0.8 rad/s，最大回正力矩只有约 86 N·m，
        // 一旦翻过 90° 就会停在「控制器力矩 = 重力矩」的平衡点上，再也回不来。
        const double Boost = FMath::Clamp((FMath::Abs(E3[Axis]) - 0.30) * 1.5, 0.0, 2.0);
        const double WlimEff = Wlim + Boost;

        // 积分限幅 + 抗饱和：角速度大时不再累积，并按时间常数快速泄放。
        // 只按角速度判据：若再叠加「误差小才积分」，稳态大倾角就永远消不掉
        // （实测会停在 28° 下不来）。
        const double Omega = (double)Data->qvel[D + 3 + Axis];
        if (FMath::Abs(Omega) < 0.30)
        {
            AttIntState[Axis] = FMath::Clamp(AttIntState[Axis] + E3[Axis] * Dt, -2.0, 2.0);
        }
        else
        {
            AttIntState[Axis] *= FMath::Max(0.0, 1.0 - 3.0 * Dt);
        }

        double Wsp = FMath::Clamp(KpAtt[Axis] * E3[Axis]
                                      + (double)AttitudeIntegralGain * AttIntState[Axis],
                                  -WlimEff, WlimEff);
        if (Axis == 2 && YawFF != 0.0)
        {
            Wsp = FMath::Clamp(Wsp + YawFF, -WlimEff, WlimEff);
        }
        double T = I * Wn * (Wsp - Omega);
        OutTorque[Axis] = FMath::Clamp(T, -1500.0, 1500.0);
    }

    // 1 Hz 诊断：倾角、误差、角速度、力矩、积分量
    {
        static double LastLogT = -1.0;
        if (Data->time - LastLogT > 1.0)
        {
            LastLogT = Data->time;
            const double UpZ = FMath::Clamp((double)Data->xmat[9 * BodyId + 8], -1.0, 1.0);
            const double TiltDeg = FMath::RadiansToDegrees(FMath::Acos(UpZ));
            UE_LOG(LogRovBridge, Log,
                TEXT("[RovBridge][姿态环] t=%.1f 倾角=%.2f E3=(%+.3f,%+.3f,%+.3f) "
                     "w=(%+.3f,%+.3f,%+.3f) T=(%+.1f,%+.1f,%+.1f) 积分=(%+.2f,%+.2f,%+.2f)"),
                Data->time, TiltDeg,
                E3[0], E3[1], E3[2],
                Data->qvel[D + 3], Data->qvel[D + 4], Data->qvel[D + 5],
                OutTorque[0], OutTorque[1], OutTorque[2],
                AttIntState[0], AttIntState[1], AttIntState[2]);
        }
    }
}

void URovMuJoCoBridge::ApplyCachedForce(mjModel_* Model, mjData_* Data)
{
    const int32 BodyId = CachedBodyId.load();
    if (BodyId < 0 || !Model || !Data || BodyId >= Model->nbody) {
        return;
    }

    // 力与力矩统一用 mj_applyFT 施加：force / torque / point 分开传参、全部世界系，
    // 自动补 r×F，避免 xfrc_applied 的元素顺序歧义，也能让浮力作用在浮心。
    if (Model->body_jntnum[BodyId] < 1) {
        return;
    }
    const int32 JntId = Model->body_jntadr[BodyId];
    if (Model->jnt_type[JntId] != mjJNT_FREE) {
        return;
    }
    const int32 DofAdr = Model->jnt_dofadr[JntId];

    // 逐接触点日志（4.2.4）：此处读到的是上一步求解出的接触
    LogContacts(Model, Data);

    // mj_applyFT 是累加语义，先清零本 body 的 6 维广义力
    for (int32 k = 0; k < 6; ++k) {
        Data->qfrc_applied[DofAdr + k] = 0.0;
    }

    // 指令新鲜度：控制量在游戏线程按帧更新，物理线程按 500 Hz 步进。
    // 游戏线程卡顿时旧指令会被持续施加，这里按仿真时间做淡出：
    // 超过 0.04 s 开始衰减，超过 0.08 s 完全交给物理线程的兜底控制器。
    // 阈值取得比 0.1 s 更紧，是因为帧率低于约 25 fps 时游戏线程的姿态环
    // 已经跟不上（每帧只更新一次），会与积分项形成慢振荡直至翻转。
    const double CmdAge = (double)Data->time - CachedControlTime.load();
    const double Fresh = FMath::Clamp(1.0 - (CmdAge - 0.04) / 0.04, 0.0, 1.0);

    const mjtNum CG[3] = {
        Data->xipos[3 * BodyId + 0],
        Data->xipos[3 * BodyId + 1],
        Data->xipos[3 * BodyId + 2]
    };
    const mjtNum ZeroT[3] = { 0.0, 0.0, 0.0 };
    const mjtNum ZeroF[3] = { 0.0, 0.0, 0.0 };

    // 【1】推进器推力 + 洋流拖曳：作用在质心
    mjtNum F1[3] = {
        (mjtNum)(CachedForceX.load() * Fresh),
        (mjtNum)(CachedForceY.load() * Fresh),
        (mjtNum)(CachedForceZ.load() * Fresh - ContactTestForce)
    };
    mj_applyFT(Model, Data, F1, ZeroT, CG, BodyId, Data->qfrc_applied);

    // 【1b】显式二次阻力：沿机体系三轴 F = -0.5·ρ·Cd·A·|v|·v。
    // 用于补偿椭球近似对大钝体 / 细长体的偏差，默认关闭。
    if (bUseExplicitDrag)
    {
        const double QW = Data->xquat[4 * BodyId + 0];
        const double QX = Data->xquat[4 * BodyId + 1];
        const double QY = Data->xquat[4 * BodyId + 2];
        const double QZ = Data->xquat[4 * BodyId + 3];
        double Bx = 0.0, By = 0.0, Bz = 0.0;
        RotByQuat(QW, -QX, -QY, -QZ,
                  Data->qvel[DofAdr + 0], Data->qvel[DofAdr + 1], Data->qvel[DofAdr + 2],
                  Bx, By, Bz);
        const double Half = 0.5 * (double)FluidDensity;
        const double Fbx = -Half * (double)DragCoeff.X * (double)DragRefArea.X * FMath::Abs(Bx) * Bx;
        const double Fby = -Half * (double)DragCoeff.Y * (double)DragRefArea.Y * FMath::Abs(By) * By;
        const double Fbz = -Half * (double)DragCoeff.Z * (double)DragRefArea.Z * FMath::Abs(Bz) * Bz;
        double Fwx = 0.0, Fwy = 0.0, Fwz = 0.0;
        RotByQuat(QW, QX, QY, QZ, Fbx, Fby, Fbz, Fwx, Fwy, Fwz);
        mjtNum F1b[3] = { (mjtNum)Fwx, (mjtNum)Fwy, (mjtNum)Fwz };
        mj_applyFT(Model, Data, F1b, ZeroT, CG, BodyId, Data->qfrc_applied);
    }

    // 【1c】附加质量：F = -m_A·a（机体系三轴），默认关闭。
    // 椭球模型内部已含附加质量，本项用于单独调节而不改模型。
    if (bUseAddedMass)
    {
        const double QW = Data->xquat[4 * BodyId + 0];
        const double QX = Data->xquat[4 * BodyId + 1];
        const double QY = Data->xquat[4 * BodyId + 2];
        const double QZ = Data->xquat[4 * BodyId + 3];
        double Ax = 0.0, Ay = 0.0, Az = 0.0;
        RotByQuat(QW, -QX, -QY, -QZ,
                  Data->qacc[DofAdr + 0], Data->qacc[DofAdr + 1], Data->qacc[DofAdr + 2],
                  Ax, Ay, Az);
        const double Fbx = -(double)AddedMass.X * Ax;
        const double Fby = -(double)AddedMass.Y * Ay;
        const double Fbz = -(double)AddedMass.Z * Az;
        double Fwx = 0.0, Fwy = 0.0, Fwz = 0.0;
        RotByQuat(QW, QX, QY, QZ, Fbx, Fby, Fbz, Fwx, Fwy, Fwz);
        mjtNum F1c[3] = { (mjtNum)Fwx, (mjtNum)Fwy, (mjtNum)Fwz };
        mj_applyFT(Model, Data, F1c, ZeroT, CG, BodyId, Data->qfrc_applied);
    }

    // 【2】浮力：作用在整机浮心 = 整机合成质心 + R·(0,0,BG)。
    // 排水体积属于整个载体，浮心应跟随整机质量分布，而非艇体自身质心。
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
    // 优先使用 MuJoCo 计算的子树质心，保证与重力作用点一致。
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

    // 【2b】机械臂浮力：按排水体积在臂的合成质心处施加向上浮力，
    // 等价于给机械臂绑浮力块。
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

    // 【3】姿态力矩：free joint 的 qfrc_applied[3..5] 就是机体系力矩，直接赋值。
    // 姿态环已移到物理线程（ComputeAttitudeTorque，500 Hz），这里只叠加
    // 游戏线程给的配平力矩与角阻尼，按 Fresh 淡出。
    double Tx = CachedTorqueX.load() * Fresh;
    double Ty = CachedTorqueY.load() * Fresh;
    double Tz = CachedTorqueZ.load() * Fresh;

    double AttT[3] = { 0.0, 0.0, 0.0 };
    ComputeAttitudeTorque(Model, Data, BodyId, (double)Model->opt.timestep, AttT);

    // 自检：-RovFlipTest 在 6~6.8 s 施加 500 N·m 横滚力矩把机身掀翻，用于验证回正能力
    static const bool bFlipTest = FParse::Param(FCommandLine::Get(), TEXT("RovFlipTest"));
    if (bFlipTest && Data->time > 6.0 && Data->time < 6.8)
    {
        AttT[0] += 500.0;
    }
    Tx += AttT[0];
    Ty += AttT[1];
    Tz += AttT[2];

    Data->qfrc_applied[DofAdr + 3] = (mjtNum)Tx;
    Data->qfrc_applied[DofAdr + 4] = (mjtNum)Ty;
    Data->qfrc_applied[DofAdr + 5] = (mjtNum)Tz;

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
    // 【6】实时状态缓存：供 HUD 与外部读取（3.4 / 7.3.1）
    {
        const double* qq = &Data->xquat[4 * BodyId];
        CachedRollDeg.store(FMath::RadiansToDegrees(
            FMath::Atan2(2.0 * (qq[0] * qq[1] + qq[2] * qq[3]),
                         1.0 - 2.0 * (qq[1] * qq[1] + qq[2] * qq[2]))));
        CachedPitchDeg.store(FMath::RadiansToDegrees(
            FMath::Asin(FMath::Clamp(2.0 * (qq[0] * qq[2] - qq[3] * qq[1]), -1.0, 1.0))));
        CachedYawDeg.store(FMath::RadiansToDegrees(
            FMath::Atan2(2.0 * (qq[0] * qq[3] + qq[1] * qq[2]),
                         1.0 - 2.0 * (qq[2] * qq[2] + qq[3] * qq[3]))));
        CachedDepthM.store((double)WaterSurfaceZ - Data->xpos[3 * BodyId + 2]);
        CachedVelocityX.store(Data->qvel[DofAdr + 0]);
        CachedVelocityY.store(Data->qvel[DofAdr + 1]);
        CachedVelocityZ.store(Data->qvel[DofAdr + 2]);
        const double Fx = CachedForceX.load() * Fresh;
        const double Fy = CachedForceY.load() * Fresh;
        const double Fz = CachedForceZ.load() * Fresh;
        CachedThrustN.store(FMath::Sqrt(Fx * Fx + Fy * Fy + Fz * Fz));

        // 接触力：只统计涉及本体的接触，并累加法向力方向
        double ContactSum = 0.0, Px = 0.0, Py = 0.0, Pz = 0.0;
        double Dx = 0.0, Dy = 0.0, Dz = 0.0;
        mjtNum F6[6] = { 0, 0, 0, 0, 0, 0 };
        for (int32 i = 0; i < Data->ncon; ++i)
        {
            const mjContact& C = Data->contact[i];
            const int32 B1 = Model->geom_bodyid[C.geom1];
            const int32 B2 = Model->geom_bodyid[C.geom2];
            bool bTouchesBody = false;
            bool bBody1IsRov = false;
            for (int32 b : { B1, B2 })
            {
                bool bInSubtree = false;
                int32 p = b;
                while (p > 0)
                {
                    if (p == BodyId) { bInSubtree = true; break; }
                    p = Model->body_parentid[p];
                }
                if (b == BodyId) { bInSubtree = true; }
                if (bInSubtree)
                {
                    bTouchesBody = true;
                    if (b == B1) { bBody1IsRov = true; }
                    break;
                }
            }
            if (!bTouchesBody) { continue; }
            mj_contactForce(Model, Data, i, F6);
            ContactSum += FMath::Abs((double)F6[0]);
            // frame[0..2] 是从 geom1 指向 geom2 的法向；作用在本体上的方向取反
            const double Sign = bBody1IsRov ? -1.0 : 1.0;
            Dx += Sign * C.frame[0] * F6[0];
            Dy += Sign * C.frame[1] * F6[0];
            Dz += Sign * C.frame[2] * F6[0];
            Px = C.pos[0]; Py = C.pos[1]; Pz = C.pos[2];
        }
        CachedContactForce.store(ContactSum);
        // 接触点存成「相对机体、UE 轴、厘米」的偏移：HUD 直接用
        // 机体 UE 位置 + 该偏移即可，避免 MuJoCo/UE 两套坐标混用。
        const double BX = Data->xpos[3 * BodyId + 0];
        const double BY = Data->xpos[3 * BodyId + 1];
        const double BZ = Data->xpos[3 * BodyId + 2];
        CachedContactPointX.store((Px - BX) * 100.0);
        CachedContactPointY.store(-(Py - BY) * 100.0);
        CachedContactPointZ.store((Pz - BZ) * 100.0);
        const double DNorm = FMath::Sqrt(Dx * Dx + Dy * Dy + Dz * Dz);
        if (DNorm > 1e-9)
        {
            // 法向同样做 Y 镜像，得到 UE 世界系方向
            CachedContactDirX.store(Dx / DNorm);
            CachedContactDirY.store(-Dy / DNorm);
            CachedContactDirZ.store(Dz / DNorm);
        }
        else
        {
            CachedContactDirX.store(0.0);
            CachedContactDirY.store(0.0);
            CachedContactDirZ.store(0.0);
        }
        CachedFlowX.store(OceanCurrent.LastVelocity.X);
        CachedFlowY.store(-OceanCurrent.LastVelocity.Y);
        CachedFlowZ.store(OceanCurrent.LastVelocity.Z);
    }
}
