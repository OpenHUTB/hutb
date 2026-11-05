// Copyright (c) OpenHUTB. All rights reserved.
//
// 把 AirSim 的 ROV Pawn 改造成 MuJoCo 的「纯跟随者」。
//
// 设计依据：docs/decisions/2026-11-05-ROV物理视觉统一改动方案.md
//
// 背景
// ----
// 改造前，同一台 ROV 的位姿有两个写入者：
//   - AirSim 的 rov_physics_body_（在 RovPawnSimApi 里积分）
//   - MuJoCo 的 AMjArticulation（在 URLab 里积分）
// 被调研的 16 个开源仿真系统里没有一个允许这种「双所有权」配置
// （docs/research/开源仿真器物理与视觉统一架构调研.html §7 第 3 条）。
//
// 改造后
// ------
//   MuJoCo 是唯一真值源；本组件每帧把 ROV 根 body 的世界位姿提供给
//   RovPawnSimApi，后者据此驱动 Pawn。AirSim 自身的 ROV 物理与 UE 侧碰撞
//   全部关闭，避免「同一个力被算两次」（调研报告 §6 第 1 条）。
//
// 关于 Tick 顺序
// --------------
// 不需要 AddTickPrerequisiteComponent：UMjBody::GetWorldPosition() /
// GetWorldRotation() 直接读 MuJoCo 的 m_BodyView.xpos / xquat 数组，
// 不依赖组件自身的 TickComponent 是否已经跑过，所以拿到的永远是最新物理状态。

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "RovMuJoCoBridge.generated.h"

class AMjArticulation;
class UMjBody;

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
     *
     * UMjPhysicsEngine 的 bIsPaused 默认是 true——URLab 的设计是让用户在
     * MjSimulate 控件里手动点「播放」。但 AirSim 的 Pawn 一旦跟随 MuJoCo，
     * 仿真不跑 Pawn 就不会动，所以桥接激活时默认自动取消暂停。
     * 需要在编辑器里手动控制时把它关掉。
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ROV|MuJoCo")
    bool bAutoStartSimulation = true;

    /**
     * MuJoCo 世界坐标到 UE 世界坐标的平移偏移（cm）。
     *
     * MuJoCo 的模型是一个自包含的水下世界（自带 seafloor / reef / wellhead），
     * 它的原点与 UE 地图没有关系。把 ROV 摆到城区的某个位置时，
     * 不能靠挪编辑器里的 Actor——Pawn 的位姿每帧被 MuJoCo 覆盖，
     * 必须在桥接这一层加偏移。
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ROV|MuJoCo")
    FVector WorldOffset = FVector::ZeroVector;

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
     * 找到 AMjArticulation 与目标 UMjBody。
     *
     * 声明为 const + mutable 成员：MuJoCo 模型通常在 Pawn 生成之后才编译完成，
     * 所以 ProvidePose（const）需要在运行中惰性重试解析。
     */
    void ResolveTargets() const;

    /** 关闭 Pawn 物理与碰撞，消除双计。 */
    void DisablePawnPhysics();

    mutable TWeakObjectPtr<AMjArticulation> Articulation;
    mutable TWeakObjectPtr<UMjBody> TargetBody;

    /** 只在第一次解析失败时打印候选列表，避免每帧刷屏。 */
    mutable bool bResolveWarned = false;

    bool bBridgeActive = false;
};
