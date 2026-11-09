// Copyright (c) OpenHUTB. All rights reserved.
//
// ROV 位姿跟随桥接。
//
// MuJoCo 是唯一的位姿真值源，本组件每帧把 ROV 根 body 的世界位姿提供给
// RovPawnSimApi，后者据此驱动 Pawn。AirSim 自身的 ROV 物理与 UE 侧碰撞全部关闭，
// 避免同一台 ROV 被两套物理重复积分。
//
// 位姿直接读 MuJoCo 的 xpos / xquat 数组，不依赖 UMjBody 的 Tick 顺序，
// 因此不需要 AddTickPrerequisiteComponent。

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Vehicles/Rov/RovUnderwaterPhysics.h"
#include <atomic>
#include "RovMuJoCoBridge.generated.h"

class AMjArticulation;
class UMjBody;

// MuJoCo 的 mjModel / mjData 是 struct mjModel_ / struct mjData_ 的 typedef。
// 这里只前向声明，避免把 mujoco.h 拉进 AirSim 的公共头。
struct mjModel_;
struct mjData_;

/**
 * 让 AirSim ROV Pawn 跟随 MuJoCo 模型的桥接组件。
 *
 * 挂在 ARovPawn 上（构造函数里自动创建）。BeginPlay 时：
 *   1. 找到场景里的 AMjArticulation
 *   2. 按后缀匹配找到目标 body（默认 "rov"）
 *   3. 关闭 Pawn 自身的物理模拟与碰撞
 *   4. 注册全局位姿源，供 RovPawnSimApi 每帧查询
 */
UCLASS(ClassGroup = (AirSim), meta = (BlueprintSpawnableComponent))
class AIRSIM_API URovMuJoCoBridge : public UActorComponent
{
    GENERATED_BODY()

public:
    URovMuJoCoBridge();

    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual void TickComponent(float DeltaTime, ELevelTick TickType,
                               FActorComponentTickFunction* ThisTickFunction) override;

    /** 总开关。关掉则完全退回 AirSim 原生物理（改造前的行为）。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ROV|MuJoCo")
    bool bEnableBridge = true;

    /**
     * 目标 body 名的后缀。
     * URLab 会给 spec 元素名加蓝图前缀（如 ImportTestArt_xxx_C_0_rov），
     * 所以用后缀匹配而不是全等匹配。
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ROV|MuJoCo")
    FString TargetBodyNameSuffix = TEXT("rov");

    /** 是否在 BeginPlay 关闭 Pawn 的物理模拟与碰撞（避免与 MuJoCo 双计）。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ROV|MuJoCo")
    bool bDisablePawnPhysics = true;

    /**
     * 是否自动启动 MuJoCo 仿真。
     * UMjPhysicsEngine 默认暂停，仿真不跑 Pawn 就不会动，因此桥接激活时自动取消暂停。
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ROV|MuJoCo")
    bool bAutoStartSimulation = true;

    /**
     * MuJoCo 世界坐标到 UE 世界坐标的平移偏移（cm）。
     * Pawn 位姿每帧由 MuJoCo 覆盖，摆放位置需要通过该偏移调整。
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ROV|MuJoCo")
    FVector WorldOffset = FVector::ZeroVector;

    /** 键盘推进：W/S 前后、A/D 左右、Q/E 上浮下潜。键盘只给出推力比例。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ROV|MuJoCo|Control")
    bool bEnableKeyboardControl = true;

    /** 推进器最大推力占自重的比例。0.5 = 最大推力只有一半重力。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ROV|MuJoCo|Control")
    float ThrusterMaxRatio = 0.5f;

    /**
     * 水面高度（米，MuJoCo 模型单位）。
     * MJCF 为米制，桥接输出到 UE 时 ×100 转厘米。
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ROV|MuJoCo|Control")
    float WaterSurfaceZ = -2.0f;   // 地图海面 Z = -200 cm

    // ---------------------------- 流体 ----------------------------

    /** 是否启用 MuJoCo 原生椭球流体模型（阻力/升力交给引擎算）。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ROV|MuJoCo|Fluid")
    bool bUseNativeFluid = true;

    /** 流体密度（kg/m³）。1000 = 水。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ROV|MuJoCo|Fluid")
    float FluidDensity = 1000.0f;

    /** 流体动力粘度（Pa·s）。水约 0.00089。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ROV|MuJoCo|Fluid")
    float FluidViscosity = 0.00089f;

    // ---------------------------- 浮力 ----------------------------

    /** 排水体积（m³），用于 Fb = ρ·g·V。默认 0.192 与 mujoco_plugin 的 config 一致。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ROV|MuJoCo|Fluid")
    float DisplacedVolume = 0.192f;

    /**
     * 浮力倍率，1.0 = 严格按 ρ·g·V。
     * 按 0.192 m³ 排水、整机 460.5 kg 配平，中性约需 2.34。
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ROV|MuJoCo|Fluid")
    float BuoyancyScale = 2.34f;

    /**
     * 机械臂排水体积（升）。
     * UR5e 连杆 11.73 L + 2F85 约 0.35 L，取 12.1 L；浮力作用在臂的合成质心。
     * 设为 0 可关闭。
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ROV|MuJoCo|Fluid")
    float ArmBuoyancyVolume = 12.1f;

    /** 机械臂浮力系数，1.0 = 完全按真实排水体积；0.9 表示臂仍有 10% 净重。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ROV|MuJoCo|Fluid")
    float ArmBuoyancyScale = 1.0f;

    /**
     * 浮心相对重心的高度（m），沿机体系 +Z。
     * 倾斜时浮力产生回复力矩 τ = r_CB × F_buoy，回复刚度 ≈ B × BG。
     * 设为 0 则浮力加在质心，无被动稳定。
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ROV|MuJoCo|Fluid")
    float BuoyancyArm = 0.02f;

    /**
     * 机械臂关节阻尼（N·m·s/rad），施加到所有非自由关节。
     * 关节无 ctrl 输入时会自由摆动，反作用力矩会持续激励艇体。
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ROV|MuJoCo")
    float ArmJointDamping = 20000.0f;

    /**
     * 机械臂关节刚度（N·m/rad），相当于在每个关节上加位置伺服弹簧，
     * 把关节锁在 MJCF 的 qpos0 上。只加阻尼时机械臂仍会在重力下缓慢下垂，
     * 偏置姿态产生恒定扰动力矩。
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ROV|MuJoCo")
    float ArmJointStiffness = 0.0f;

    /**
     * 机械臂重力补偿：把机械臂各 body 的 gravcomp 设为 1.0，抵消其自重。
     * 保留惯量与碰撞。
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ROV|MuJoCo")
    bool bArmGravityComp = false;

    // ---------------------------- 姿态控制 ----------------------------

    /**
     * 姿态保持：roll/pitch 回水平 + yaw 锁航向。
     *
     * 分工按真实 ROV：被动稳定（浮心高于重心）负责 roll/pitch 的刚度，
     * 本控制器只补阻尼和积分 —— 积分是必需的，因为机械臂挂在一侧会产生
     * 恒定扰动力矩，纯 PD 只能平衡在某个固定倾角上。
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ROV|MuJoCo|Attitude")
    bool bEnableAttitudeHold = true;

    /** 姿态闭环带宽（rad/s）。增益按 I·ω² 自动缩放，换惯量不用重调。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ROV|MuJoCo|Attitude")
    float AttitudeBandwidth = 1.0f;

    /** 阻尼比。1.0 = 临界阻尼，不超调。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ROV|MuJoCo|Attitude")
    float AttitudeZeta = 0.8f;

    /**
     * 姿态积分增益（rad/s per rad·s）。消除恒定力矩造成的固定倾角，
     * 积分值限幅 ±2.0 rad/s；设 0 退回纯 P。
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ROV|MuJoCo|Attitude")
    float AttitudeIntegralGain = 0.5f;

    /** 积分增益系数（相对 I·ω³）。用于消除恒定扰动造成的稳态误差。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ROV|MuJoCo|Attitude")
    float AttitudeIntegralFactor = 0.0f;

    /** 目标航向（度）。启动时自动捕获当前航向。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ROV|MuJoCo|Attitude")
    float TargetYawDeg = 0.0f;

    /** 航向调整速率（度/秒）。按住 ← / → 连续改变目标航向。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ROV|MuJoCo|Attitude")
    float YawRateDegPerSec = 60.0f;

    /**
     * 机械臂静力矩配平（机体系，N·m），常值前馈。
     * 臂的重力矩随俯仰角变化，常值前馈无法完全抵消，默认 0。
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ROV|MuJoCo|Attitude")
    float TrimTorquePitch = 0.0f;

    /** 横滚方向配平力矩（N·m），用法同 TrimTorquePitch。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ROV|MuJoCo|Attitude")
    float TrimTorqueRoll = 0.0f;
    /** 水平（roll/pitch）回调带宽 rad/s */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ROV|MuJoCo|Attitude")
    float LevelBandwidth = 1.5f;

    /** 水平阻尼比 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ROV|MuJoCo|Attitude")
    float LevelZeta = 0.8f;

    /** 航向回调带宽 rad/s */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ROV|MuJoCo|Attitude")
    float YawBandwidth = 1.0f;

    /** 航向阻尼比 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ROV|MuJoCo|Attitude")
    float YawZeta = 1.0f;

    // ---------------------------- 洋流 ----------------------------

    /** 洋流拖曳系数（kg/s）。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ROV|MuJoCo|Current")
    float CurrentDragCoeff = 10.0f;

    /**
     * 附加角阻尼（1/s），与流体模型叠加，作为独立受力项。
     * 默认 0，旋转阻尼由 AngularDofDamping 提供。
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ROV|MuJoCo|Fluid")
    float AngularDampingCoeff = 0.0f;

    /**
     * 转动自由度阻尼（N·m·s/rad），写入 MuJoCo 的 dof_damping[3..5]。
     * 由 spin-down 实验辨识得到；<= 0 时退回「0.5 × 平均惯量」。
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ROV|MuJoCo|Fluid")
    float AngularDofDamping = 13.0f;

    /** 是否启用洋流（三层级联：Gauss-Markov + 分层 + 湍流）。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ROV|MuJoCo|Current")
    bool bEnableOceanCurrent = true;

    /** 洋流平均流速（m/s）。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ROV|MuJoCo|Current")
    float OceanCurrentMeanSpeed = 0.15f;

    // ---------------------------- 定深 ----------------------------

    /** PID 定深是否可用（按 Z 键切换）。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ROV|MuJoCo|Depth")
    bool bEnableDepthHold = true;

    /** 桥接是否已生效。 */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "ROV|MuJoCo")
    bool IsBridgeActive() const { return bBridgeActive; }

    /** 当前跟随的 body 名（调试用）。 */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "ROV|MuJoCo")
    FString GetResolvedBodyName() const;

    /**
     * 全局位姿源。RovPawnSimApi 每帧调用它取位姿。
     * 给出的是 UE 世界坐标；AirSim 侧的 NED 转换由 PawnSimApi::toPoseFromUnreal 完成。
     * 为空表示没有外部源，RovPawnSimApi 退回读取自身物理体。
     */
    static std::function<bool(FVector&, FQuat&)> ExternalPoseProvider;

private:
    /** 供 ExternalPoseProvider 调用的实际取值函数。 */
    bool ProvidePose(FVector& OutPosition, FQuat& OutRotation) const;

    /**
     * 解析 AMjArticulation 与目标 UMjBody。
     * 模型通常在 Pawn 生成之后才编译完成，因此声明为 const + mutable，
     * 由 ProvidePose 在运行中惰性重试。
     */
    void ResolveTargets() const;

    /** 关闭 Pawn 物理与碰撞，避免与 MuJoCo 重复计算。 */
    void DisablePawnPhysics();

    /** 每帧把键盘输入换算成推力与力矩，写入缓存。 */
    void ApplyControlForce();

    /**
     * 物理线程内施力：从缓存读出推力/力矩并写入 MuJoCo。
     * 通过 UMjPhysicsEngine 的 PreStepCallbacks 在 mj_step 之前执行，
     * 与物理步同锁，避免被 mj_step 覆盖。
     */
    void ApplyCachedForce(mjModel_* Model, mjData_* Data);

    /** 期望推力（N），游戏线程写、物理线程读。 */
    std::atomic<double> CachedForceX{0.0};
    std::atomic<double> CachedForceY{0.0};
    std::atomic<double> CachedForceZ{0.0};

    /** 期望力矩（N·m），**世界系**（mj_applyFT 的入参就是世界系）。 */
    std::atomic<double> CachedTorqueX{0.0};
    std::atomic<double> CachedTorqueY{0.0};
    std::atomic<double> CachedTorqueZ{0.0};

    /** 浮力大小（N）。单独存，因为要作用在浮心而不是质心。 */
    std::atomic<double> CachedBuoyancy{0.0};

    std::atomic<int32> CachedBodyId{-1};

    /**
     * 控制指令时间戳（MuJoCo 仿真时间）。
     * 指令在游戏线程按帧更新、物理在独立线程按 500 Hz 步进；
     * 物理线程据此判断指令是否过期，过期时把推力与力矩归零。
     */
    std::atomic<double> CachedControlTime{0.0};

    /** pre-step 回调只注册一次。 */
    bool bControlCallbackRegistered = false;

    /** 洋流模型（三层级联）。 */
    FRovOceanCurrent OceanCurrent;

    /** PID 定深控制器。 */
    FRovPIDDepthController DepthPID;

    /** 是否正在定深。 */
    bool bDepthHoldActive = false;

    /** 定深目标（MuJoCo 世界系 Z，m）。 */
    double DepthHoldTargetZ = 0.0;

    /** Z 键上一帧状态，用于边沿检测。 */
    bool bPrevDepthKeyDown = false;

    /** 姿态控制器积分项（机体系，rad·s）。 */
    FVector AttitudeIntegral = FVector::ZeroVector;

    /** 姿态控制器是否已初始化（首次捕获目标航向）。 */
    bool bAttitudeInitialized = false;

    /**
     * 自由关节 6×6 质量阵的转动对角元（kg·m²），即 ROV+机械臂的**复合惯量**。
     * body_inertia 只有 ROV 本体，不含机械臂；用它算增益会严重偏小。
     */
    double EffInertia[3] = { 0.0, 0.0, 0.0 };

    mutable TWeakObjectPtr<AMjArticulation> Articulation;
    mutable TWeakObjectPtr<UMjBody> TargetBody;

    /** 只在第一次解析失败时打印候选列表，避免每帧刷屏。 */
    mutable bool bResolveWarned = false;

    bool bBridgeActive = false;
};
