#pragma once

#include <vector>
#include <algorithm>
#include "Params.hpp"
#include "interfaces/CommonStructs.hpp"

namespace Rov_simple
{

class Mixer
{
public:
    Mixer(const Params* params)
        : params_(params)
    {
    }

    void getMotorOutput(const Axis4r& controls, std::vector<float>& motor_outputs) const
    {
        if (controls.throttle() < params_->actuator.min_angling_throttle) {
            motor_outputs.assign(params_->actuator.actuator_count, controls.throttle());
            return;
        }

        for (int motor_index = 0; motor_index < kMotorCount; ++motor_index) {
            motor_outputs[motor_index] =
                controls.throttle() * mixerQuadX[motor_index].throttle + controls.pitch() * mixerQuadX[motor_index].pitch + controls.roll() * mixerQuadX[motor_index].roll + controls.yaw() * mixerQuadX[motor_index].yaw;
        }

        float min_motor = *std::min_element(motor_outputs.begin(), motor_outputs.begin() + kMotorCount);
        if (min_motor < params_->actuator.min_actuator_output) {
            float undershoot = params_->actuator.min_actuator_output - min_motor;
            for (int motor_index = 0; motor_index < kMotorCount; ++motor_index)
                motor_outputs[motor_index] += undershoot;
        }

        float max_motor = *std::max_element(motor_outputs.begin(), motor_outputs.begin() + kMotorCount);
        float scale = max_motor / params_->actuator.max_actuator_output;
        if (scale > params_->actuator.max_actuator_output) {
            for (int motor_index = 0; motor_index < kMotorCount; ++motor_index)
                motor_outputs[motor_index] /= scale;
        }

        for (int motor_index = 0; motor_index < kMotorCount; ++motor_index)
            motor_outputs[motor_index] = std::max(params_->actuator.min_actuator_output,
                                                  std::min(motor_outputs[motor_index], params_->actuator.max_actuator_output));
    }

private:
    const int kMotorCount = 4;

    const Params* params_;

    // Custom mixer data per motor
    typedef struct motorMixer_t
    {
        float throttle;
        float roll;
        float pitch;
        float yaw;
    } motorMixer_t;

    //only thing that this matrix does is change the sign
    const motorMixer_t mixerQuadX[4] = {
        //QuadX config
        { 1.0f, -1.0f, 1.0f, 1.0f }, // FRONT_R
        { 1.0f, 1.0f, -1.0f, 1.0f }, // REAR_L
        { 1.0f, 1.0f, 1.0f, -1.0f }, // FRONT_L
        { 1.0f, -1.0f, -1.0f, -1.0f }, // REAR_R
    };
};

} //namespace
