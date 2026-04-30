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

#include "MuJoCo/Components/Actuators/MjVelocityActuator.h"

#include "XmlNode.h"
#include "MuJoCo/Utils/MjXmlUtils.h"
#include "Utils/URLabLogging.h"

UMjVelocityActuator::UMjVelocityActuator()
{
    Type = EMjActuatorType::Velocity;
}

void UMjVelocityActuator::ParseSpecifics(const FXmlNode* Node)
{
    MjXmlUtils::ReadAttrDouble(Node, TEXT("kv"), Kv, bOverride_Kv);
}

void UMjVelocityActuator::ExtractSpecifics(const mjsActuator* act)
{
    // mjs_setToVelocity sets:
    // gainprm[0] = kv
    // biasprm[2] = -kv
    if (BiasPrm.Num() > 2)
    {
        Kv = -BiasPrm[2];
        bOverride_Kv = true;
    }
}

void UMjVelocityActuator::ExportTo(mjsActuator* act, mjsDefault* def)
{
    if (!act) return;

    Super::ExportTo(act, def); // 1. Common attributes (populates act from default via mjs_addActuator)

    // Mirror OneActuator: kv = act->gainprm[0] as inherited default, override only if explicitly set.
    double kv = bOverride_Kv ? (double)Kv : act->gainprm[0];

    mjs_setToVelocity(act, kv);  // 2. Type preset
    ApplyRawOverrides(act, def); // 3. Raw prm overrides
}
