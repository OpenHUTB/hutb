// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#ifndef msr_airlib_RovRpcLibClient_hpp
#define msr_airlib_RovRpcLibClient_hpp

#include "common/Common.hpp"
#include <functional>
#include "common/CommonStructs.hpp"
#include "common/ImageCaptureBase.hpp"
#include "vehicles/rov/api/RovApiBase.hpp"
#include "api/RpcLibClientBase.hpp"
#include "vehicles/rov/api/RovCommon.hpp"

namespace msr
{
namespace airlib
{

    class RovRpcLibClient : public RpcLibClientBase
    {
    public:
        RovRpcLibClient(const string& ip_address = "localhost", uint16_t port = RpcLibPort, float timeout_sec = 60);

        RovRpcLibClient* takeoffAsync(float timeout_sec = 20, const std::string& vehicle_name = "");
        RovRpcLibClient* landAsync(float timeout_sec = 60, const std::string& vehicle_name = "");
        RovRpcLibClient* goHomeAsync(float timeout_sec = Utils::max<float>(), const std::string& vehicle_name = "");
        RovRpcLibClient* moveToGPSAsync(float latitude, float longitude, float altitude, float velocity, float timeout_sec = Utils::max<float>(),
                                               DrivetrainType drivetrain = DrivetrainType::MaxDegreeOfFreedom, const YawMode& yaw_mode = YawMode(),
                                               float lookahead = -1, float adaptive_lookahead = 1, const std::string& vehicle_name = "");
        RovRpcLibClient* moveByVelocityBodyFrameAsync(float vx, float vy, float vz, float duration,
                                                            DrivetrainType drivetrain = DrivetrainType::MaxDegreeOfFreedom, const YawMode& yaw_mode = YawMode(), const std::string& vehicle_name = "");
        RovRpcLibClient* moveByVelocityZBodyFrameAsync(float vx, float vy, float z, float duration,
                                                             DrivetrainType drivetrain = DrivetrainType::MaxDegreeOfFreedom, const YawMode& yaw_mode = YawMode(), const std::string& vehicle_name = "");
        RovRpcLibClient* moveByRollPitchYawZAsync(float roll, float pitch, float yaw, float z, float duration, const std::string& vehicle_name = "");
        RovRpcLibClient* moveByRollPitchYawThrottleAsync(float roll, float pitch, float yaw, float throttle, float duration, const std::string& vehicle_name = "");
        RovRpcLibClient* moveByRollPitchYawrateThrottleAsync(float roll, float pitch, float yaw_rate, float throttle, float duration, const std::string& vehicle_name = "");
        RovRpcLibClient* moveByRollPitchYawrateZAsync(float roll, float pitch, float yaw_rate, float z, float duration, const std::string& vehicle_name = "");
        RovRpcLibClient* moveByAngleRatesZAsync(float roll_rate, float pitch_rate, float yaw_rate, float z, float duration, const std::string& vehicle_name = "");
        RovRpcLibClient* moveByAngleRatesThrottleAsync(float roll_rate, float pitch_rate, float yaw_rate, float throttle, float duration, const std::string& vehicle_name = "");
        RovRpcLibClient* moveByVelocityAsync(float vx, float vy, float vz, float duration,
                                                   DrivetrainType drivetrain = DrivetrainType::MaxDegreeOfFreedom, const YawMode& yaw_mode = YawMode(), const std::string& vehicle_name = "");
        RovRpcLibClient* moveByVelocityZAsync(float vx, float vy, float z, float duration,
                                                    DrivetrainType drivetrain = DrivetrainType::MaxDegreeOfFreedom, const YawMode& yaw_mode = YawMode(), const std::string& vehicle_name = "");
        RovRpcLibClient* moveOnPathAsync(const vector<Vector3r>& path, float velocity, float timeout_sec = Utils::max<float>(),
                                               DrivetrainType drivetrain = DrivetrainType::MaxDegreeOfFreedom, const YawMode& yaw_mode = YawMode(),
                                               float lookahead = -1, float adaptive_lookahead = 1, const std::string& vehicle_name = "");
        RovRpcLibClient* moveToPositionAsync(float x, float y, float z, float velocity, float timeout_sec = Utils::max<float>(),
                                                   DrivetrainType drivetrain = DrivetrainType::MaxDegreeOfFreedom, const YawMode& yaw_mode = YawMode(),
                                                   float lookahead = -1, float adaptive_lookahead = 1, const std::string& vehicle_name = "");
        RovRpcLibClient* moveToZAsync(float z, float velocity, float timeout_sec = Utils::max<float>(),
                                            const YawMode& yaw_mode = YawMode(), float lookahead = -1, float adaptive_lookahead = 1, const std::string& vehicle_name = "");
        RovRpcLibClient* moveByManualAsync(float vx_max, float vy_max, float z_min, float duration,
                                                 DrivetrainType drivetrain = DrivetrainType::MaxDegreeOfFreedom, const YawMode& yaw_mode = YawMode(), const std::string& vehicle_name = "");
        RovRpcLibClient* rotateToYawAsync(float yaw, float timeout_sec = Utils::max<float>(), float margin = 5, const std::string& vehicle_name = "");
        RovRpcLibClient* rotateByYawRateAsync(float yaw_rate, float duration, const std::string& vehicle_name = "");
        RovRpcLibClient* hoverAsync(const std::string& vehicle_name = "");

        RovRpcLibClient* setRovPose(Pose pose, const vector<float>& tilt_angles, bool ignore_collision, const std::string& vehicle_name = "");
        RovRpcLibClient* moveByMotorPWMsAsync(const vector<float>& pwm_values, float duration, const std::string& vehicle_name = "");

        void setAngleLevelControllerGains(const vector<float>& kp, const vector<float>& ki, const vector<float>& kd, const std::string& vehicle_name = "");
        void setAngleRateControllerGains(const vector<float>& kp, const vector<float>& ki, const vector<float>& kd, const std::string& vehicle_name = "");
        void setVelocityControllerGains(const vector<float>& kp, const vector<float>& ki, const vector<float>& kd, const std::string& vehicle_name = "");
        void setPositionControllerGains(const vector<float>& kp, const vector<float>& ki, const vector<float>& kd, const std::string& vehicle_name = "");
        void moveByRC(const RCData& rc_data, const std::string& vehicle_name = "");

        RovState getRovState(const std::string& vehicle_name = "");
        RotorTiltableStates getRotorStates(const std::string& vehicle_name = "");

        bool setSafety(SafetyEval::SafetyViolationType enable_reasons, float obs_clearance, SafetyEval::ObsAvoidanceStrategy obs_startegy,
                       float obs_avoidance_vel, const Vector3r& origin, float xy_length, float max_z, float min_z, const std::string& vehicle_name = "");

        virtual RovRpcLibClient* waitOnLastTask(bool* task_result = nullptr, float timeout_sec = Utils::nan<float>()) override;

        virtual ~RovRpcLibClient(); //required for pimpl

    private:
        struct impl;
        std::unique_ptr<impl> pimpl_;
    };
}
} //namespace
#endif
