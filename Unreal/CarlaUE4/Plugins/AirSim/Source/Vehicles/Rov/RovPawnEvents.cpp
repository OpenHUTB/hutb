#include "RovPawnEvents.h"

RovPawnEvents::ActuatorsSignal& RovPawnEvents::getActuatorSignal()
{
    return actuator_signal_;
}

RovPawnEvents::KeyboardPwmSignal& RovPawnEvents::getKeyboardPwmSignal()
{
    return keyboard_pwm_signal_;
}
