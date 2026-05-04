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
#include "MuJoCo/Core/MjSimOptions.h"
#include <mujoco/mujoco.h>

void FMuJoCoOptions::ApplyToSpec(mjSpec* Spec) const
{
    if (!Spec) return;

    if (bOverride_MemoryMB)
    {
        Spec->memory = static_cast<mjtSize>(MemoryMB) * 1024 * 1024;
    }

    Spec->option.timestep = Timestep;

    // Gravity: UE cm/s² → MuJoCo m/s², negate Y for handedness
    Spec->option.gravity[0] =  Gravity.X / 100.0;
    Spec->option.gravity[1] = -Gravity.Y / 100.0;
    Spec->option.gravity[2] =  Gravity.Z / 100.0;

    Spec->option.wind[0] =  Wind.X / 100.0;
    Spec->option.wind[1] = -Wind.Y / 100.0;
    Spec->option.wind[2] =  Wind.Z / 100.0;

    Spec->option.magnetic[0] =  Magnetic.X;
    Spec->option.magnetic[1] = -Magnetic.Y;
    Spec->option.magnetic[2] =  Magnetic.Z;

    Spec->option.density = Density;
    Spec->option.viscosity = Viscosity;
    Spec->option.impratio = Impratio;
    Spec->option.tolerance = Tolerance;
    Spec->option.iterations = Iterations;
    Spec->option.ls_iterations = LsIterations;

    if (bOverride_Integrator) Spec->option.integrator = (int)Integrator;
    if (bOverride_Cone)       Spec->option.cone = (int)Cone;
    if (bOverride_Solver)     Spec->option.solver = (int)Solver;

    if (bOverride_NoslipIterations) Spec->option.noslip_iterations = NoslipIterations;
    if (bOverride_NoslipTolerance)  Spec->option.noslip_tolerance = NoslipTolerance;
    if (bOverride_CCD_Iterations)   Spec->option.ccd_iterations = CCD_Iterations;
    if (bOverride_CCD_Tolerance)    Spec->option.ccd_tolerance = CCD_Tolerance;

    // MultiCCD
    constexpr int MJ_ENBL_MULTICCD = 1 << 4;
    if (bEnableMultiCCD)
        Spec->option.enableflags |= MJ_ENBL_MULTICCD;
    else
        Spec->option.enableflags &= ~MJ_ENBL_MULTICCD;

    // Sleep
    constexpr int MJ_ENBL_SLEEP = 1 << 5;
    if (bEnableSleep)
    {
        Spec->option.enableflags |= MJ_ENBL_SLEEP;
        Spec->option.sleep_tolerance = SleepTolerance;
    }
}

void FMuJoCoOptions::ApplyOverridesToModel(mjModel* Model) const
{
    if (!Model) return;

    if (bOverride_Timestep) Model->opt.timestep = Timestep;

    if (bOverride_Gravity)
    {
        Model->opt.gravity[0] =  Gravity.X / 100.0;
        Model->opt.gravity[1] = -Gravity.Y / 100.0;
        Model->opt.gravity[2] =  Gravity.Z / 100.0;
    }

    if (bOverride_Wind)
    {
        Model->opt.wind[0] =  Wind.X / 100.0;
        Model->opt.wind[1] = -Wind.Y / 100.0;
        Model->opt.wind[2] =  Wind.Z / 100.0;
    }

    if (bOverride_Magnetic)
    {
        Model->opt.magnetic[0] =  Magnetic.X;
        Model->opt.magnetic[1] = -Magnetic.Y;
        Model->opt.magnetic[2] =  Magnetic.Z;
    }

    if (bOverride_Density)      Model->opt.density = Density;
    if (bOverride_Viscosity)    Model->opt.viscosity = Viscosity;
    if (bOverride_Impratio)     Model->opt.impratio = Impratio;
    if (bOverride_Tolerance)    Model->opt.tolerance = Tolerance;
    if (bOverride_Iterations)   Model->opt.iterations = Iterations;
    if (bOverride_LsIterations) Model->opt.ls_iterations = LsIterations;

    if (bOverride_Integrator) Model->opt.integrator = (int)Integrator;
    if (bOverride_Cone)       Model->opt.cone = (int)Cone;
    if (bOverride_Solver)     Model->opt.solver = (int)Solver;

    if (bOverride_NoslipIterations) Model->opt.noslip_iterations = NoslipIterations;
    if (bOverride_NoslipTolerance)  Model->opt.noslip_tolerance = NoslipTolerance;
    if (bOverride_CCD_Iterations)   Model->opt.ccd_iterations = CCD_Iterations;
    if (bOverride_CCD_Tolerance)    Model->opt.ccd_tolerance = CCD_Tolerance;

    // MultiCCD
    constexpr int MJ_ENBL_MULTICCD = 1 << 4;
    if (bEnableMultiCCD)
        Model->opt.enableflags |= MJ_ENBL_MULTICCD;
    else
        Model->opt.enableflags &= ~MJ_ENBL_MULTICCD;

    // Sleep
    constexpr int MJ_ENBL_SLEEP = 1 << 5;
    if (bEnableSleep)
    {
        Model->opt.enableflags |= MJ_ENBL_SLEEP;
        Model->opt.sleep_tolerance = SleepTolerance;
    }
    else
    {
        Model->opt.enableflags &= ~MJ_ENBL_SLEEP;
    }
}
