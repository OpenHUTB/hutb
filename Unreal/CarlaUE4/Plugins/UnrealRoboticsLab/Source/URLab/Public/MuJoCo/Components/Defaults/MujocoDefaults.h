// Copyright (c) 2026 Jonathan Embley-Riches. All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// --- LEGAL DISCLAIMER ---
// UnrealRoboticsLab is an independent software plugin. It is NOT affiliated with, 
// endorsed by, or sponsored by Epic Games, Inc. "Unreal" and "Unreal Engine" are 
// trademarks or registered trademarks of Epic Games, Inc. in the US and elsewhere.
//
// This plugin incorporates third-party software: MuJoCo (Apache 2.0), 
// CoACD (MIT), and libzmq (MPL 2.0). See ThirdPartyNotices.txt for details.

#pragma once

/**
 * @file MujocoDefaults.h
 * @brief True default values for MuJoCo element properties.
 * 
 * Used during import to determine if a property was explicitly set in the XML
 * vs inherited from class defaults. If value == default, assume not explicitly set.
 */

namespace MujocoDefaults
{
    /**
     * @brief Default values for MuJoCo body elements.
     * @see https://mujoco.readthedocs.io/en/stable/XMLreference.html#body-body
     */
    namespace Body
    {
        constexpr double GravComp = 0.0;
        constexpr bool Mocap = false;
    }

    /**
     * @brief Default values for MuJoCo geom elements.
     * @see https://mujoco.readthedocs.io/en/stable/XMLreference.html#body-geom
     */
    namespace Geom
    {
        constexpr int32 Contype = 1;
        constexpr int32 Conaffinity = 1;
        constexpr int32 Condim = 3;
        constexpr int32 Group = 0;
        constexpr int32 Priority = 0;
        constexpr double Friction[3] = {1.0, 0.005, 0.0001};
        constexpr double SolRef[2] = {0.02, 1.0};
        constexpr double SolImp[5] = {0.9, 0.95, 0.001, 0.5, 2.0};
        constexpr double Margin = 0.0;
        constexpr double Gap = 0.0;
        constexpr double Density = 1000.0;
        constexpr double SolMix = 1.0;
        constexpr double Rgba[4] = {0.5, 0.5, 0.5, 1.0};
    }

    /**
     * @brief Default values for MuJoCo joint elements.
     * @see https://mujoco.readthedocs.io/en/stable/XMLreference.html#body-joint
     */
    namespace Joint
    {
        constexpr double Stiffness = 0.0;
        constexpr double Damping = 0.0;
        constexpr double Armature = 0.0;
        constexpr double FrictionLoss = 0.0;
        constexpr double SpringRef = 0.0;
        constexpr double Margin = 0.0;
        constexpr int32 Limited = 0;  // mjLIMITED_FALSE = 0
        constexpr double Range[2] = {0.0, 0.0};
        constexpr double SolRefLimit[2] = {0.02, 1.0};
        constexpr double SolImpLimit[5] = {0.9, 0.95, 0.001, 0.5, 2.0};
        constexpr double SolRefFriction[2] = {0.02, 1.0};
        constexpr double SolImpFriction[5] = {0.9, 0.95, 0.001, 0.5, 2.0};
    }

    /**
     * @brief Default values for MuJoCo actuator elements.
     * @see https://mujoco.readthedocs.io/en/stable/XMLreference.html#actuator-general
     */
    namespace Actuator
    {
        constexpr double CtrlRange[2] = {0.0, 0.0};
        constexpr double ForceRange[2] = {0.0, 0.0};
        constexpr double ActRange[2] = {0.0, 0.0};
        constexpr double LengthRange[2] = {0.0, 0.0};
        constexpr double Gear[6] = {1.0, 0.0, 0.0, 0.0, 0.0, 0.0};
        constexpr int32 CtrlLimited = 0;
        constexpr int32 ForceLimited = 0;
        constexpr int32 ActLimited = 0;
    }

    /**
     * @brief Default values for MuJoCo sensor elements.
     * @see https://mujoco.readthedocs.io/en/stable/XMLreference.html#sensor
     */
    namespace Sensor
    {
        constexpr double Noise = 0.0;
        constexpr double Cutoff = 0.0;
    }
}
