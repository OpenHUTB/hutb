#pragma once

#include "CoreMinimal.h"

#include "PawnSimApi.h"
#include "vehicles/rov/RovPhysicsBody.hpp"
#include "vehicles/rov/RovBodyParams.hpp"
#include "common/Common.hpp"
#include "common/CommonStructs.hpp"
#include "common/common_utils/UniqueValueMap.hpp"
#include "RovPawnEvents.h"
#include <future>
#include <functional>

class RovPawnSimApi : public PawnSimApi
{
public:
    typedef msr::airlib::real_T real_T;
    typedef msr::airlib::Utils Utils;
    typedef msr::airlib::RovPhysicsBody RovPhysicsBody;
    typedef msr::airlib::StateReporter StateReporter;
    typedef msr::airlib::UpdatableObject UpdatableObject;
    typedef msr::airlib::Pose Pose;

    typedef RovPawnEvents::RotorTiltableInfo RotorTiltableInfo;

public:
    virtual void initialize() override;

    virtual ~RovPawnSimApi() = default;

    //VehicleSimApiBase interface
    //implements game interface to update pawn
    RovPawnSimApi(const Params& params);
    virtual void updateRenderedState(float dt) override;
    virtual void updateRendering(float dt) override;

    //PhysicsBody interface
    //this just wrapped around RovPhysicsBody physics body
    virtual void resetImplementation() override;
    virtual void update() override;
    virtual void reportState(StateReporter& reporter) override;
    virtual UpdatableObject* getPhysicsBody() override;

    virtual void setPose(const Pose& pose, bool ignore_collision) override;
    virtual void setKinematics(const Kinematics::State& state, bool ignore_collision) override;
    virtual void pawnTick(float dt) override;

    virtual void setPoseCustom(const Pose& pose, const vector<float>& tilt_angles, bool ignore_collision, bool spin_props) override;
    virtual void updateRotors();

    msr::airlib::RovApiBase* getVehicleApi() const
    {
        return vehicle_api_.get();
    }

    virtual msr::airlib::VehicleApiBase* getVehicleApiBase() const override
    {
        return vehicle_api_.get();
    }

    void onKeyboardPwm(const std::vector<float>& pwms);

    // ---------------------------------------------------------------------
    // OpenHUTB 追加（2026-11-05）：外部位姿源
    //
    // 设计依据：docs/decisions/2026-11-05-ROV物理视觉统一改动方案.md
    //
    // AirSim 的 rov_physics_body_ 与 MuJoCo 的 AMjArticulation 会同时积分同一台
    // ROV 的位姿，构成「双所有权」。被调研的 16 个开源仿真系统里没有一个允许
    // 这种配置（docs/research/开源仿真器物理与视觉统一架构调研.html §7 第 3 条）。
    //
    // 这里把 RovPawnSimApi 降级为「纯跟随者」：设置了外部位姿源后，
    // updateRenderedState() 优先用它，不再读 rov_physics_body_。
    //
    // provider 返回 true 表示本帧有有效位姿；返回 false 则回退原逻辑
    // （例如 MuJoCo 模型尚未编译完成）。
    //
    // 注意：provider 给出的是 **UE 世界坐标**（FVector + FQuat），
    // 由基类的 toPoseFromUnreal() 统一转成 AirSim 的 NED Pose，
    // 保证坐标转换只在一个地方发生。
    // ---------------------------------------------------------------------
    void setExternalPoseProvider(std::function<bool(FVector&, FQuat&)> provider)
    {
        external_pose_provider_ = std::move(provider);
    }
    void clearExternalPoseProvider() { external_pose_provider_ = nullptr; }
    bool hasExternalPoseProvider() const { return static_cast<bool>(external_pose_provider_); }

private:
    std::function<bool(FVector&, FQuat&)> external_pose_provider_;

    std::unique_ptr<msr::airlib::RovApiBase> vehicle_api_;
    std::unique_ptr<msr::airlib::RovBodyParams> vehicle_params_;

    std::unique_ptr<RovPhysicsBody> rov_physics_body_;
    unsigned int rotor_count_;
    std::vector<RotorTiltableInfo> rotor_infos_;

    //show info on collision response from physics engine
    CollisionResponse collision_response;

    RovPawnEvents* pawn_events_;

    //when pose needs to set from non-physics thread, we set it as pending
    bool pending_pose_collisions_;
    enum class PendingPoseStatus
    {
        NonePending,
        RenderPending
    } pending_pose_status_;
    Pose pending_phys_pose_; //force new pose through API

    //reset must happen while World is locked so its async task initiated from API thread
    bool reset_pending_;
    bool did_reset_;
    std::packaged_task<void()> reset_task_;

    Pose last_phys_pose_; //for trace lines showing vehicle path
    std::vector<std::string> vehicle_api_messages_;
    RotorTiltableStates rotor_states_;
};
