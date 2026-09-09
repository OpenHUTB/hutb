#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"

#include "PawnEvents.h"
#include "common/Common.hpp"
#include "common/common_utils/Signal.hpp"

class RovPawnEvents : public PawnEvents
{
public: //types
    typedef msr::airlib::real_T real_T;
    struct RotorTiltableInfo
    {
        real_T rotor_speed = 0;
        int rotor_direction = 0;
        real_T rotor_thrust = 0;
        real_T rotor_control_filtered = 0;
        real_T rotor_angle_from_vertical = 0;
        bool is_fixed = false;
    };

    typedef common_utils::Signal<const std::vector<RotorTiltableInfo>&> ActuatorsSignal;

public:
    ActuatorsSignal& getActuatorSignal();

private:
    ActuatorsSignal actuator_signal_;
};
