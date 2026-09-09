// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#ifndef msr_airlib_RotorTiltableParams_hpp
#define msr_airlib_RotorTiltableParams_hpp

#include "common/Common.hpp"
#include "vehicles/multirotor/RotorParams.hpp"

namespace msr
{
namespace airlib
{

    struct RotorTiltableParams
    {

        bool use_simple_rotor_model = true; //if true, will use method of Rotor to calculate thrust and torque.
            //otherwise, will use more complicated model below

        RotorParams rotor_params = RotorParams(); //only some params will be used if use_simple_rotor_model is false

        real_T angle_signal_filter_tc = 0.005f; //time constant for angle command signal
        real_T angle_filter_tc = 0.1f; //time constant for servo arm to reach angle command signal

        //rotor parameters
        real_T max_voltage = 11.1; //nominal voltage for 3-cell battery. Could increase to 12.6 to indicate battery is fully charged...
        real_T prop_diameter = 7 * (0.0254);
        real_T motor_resistance = 0.3;
        real_T motor_KV = 1450;
        real_T motor_KQ = (1.0 / motor_KV) * 60.0 / (2.0 * M_PIf);
        real_T no_load_current = 0.83;

        //prop second order coefficients
        real_T CT0 = 0.1167;
        real_T CT1 = 0.0144;
        real_T CT2 = -0.1480;
        real_T CQ0 = 0.0088;
        real_T CQ1 = 0.0129;
        real_T CQ2 = -0.0216;
    };

}
} //namespace
#endif
