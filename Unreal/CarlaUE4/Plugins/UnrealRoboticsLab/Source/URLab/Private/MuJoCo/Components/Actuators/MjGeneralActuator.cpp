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

#include "MuJoCo/Components/Actuators/MjGeneralActuator.h"

UMjGeneralActuator::UMjGeneralActuator()
{
    // A general actuator may default to type Motor initially if unspecified, but we map it directly without internal overrides.
    Type = EMjActuatorType::Motor;
}

void UMjGeneralActuator::ExportTo(mjsActuator* act, mjsDefault* def)
{
    if (!act) return;

    // General actuator has no mjs_setTo* — it is configured entirely via explicit
    // dyntype/gaintype/biastype and the raw gainprm/biasprm/dynprm arrays.
    // Super sets common attributes (transmission, gear, ranges, group, etc.),
    // then ApplyRawOverrides applies any explicit prm overrides.
    Super::ExportTo(act, def);
    ApplyRawOverrides(act, def);
}
