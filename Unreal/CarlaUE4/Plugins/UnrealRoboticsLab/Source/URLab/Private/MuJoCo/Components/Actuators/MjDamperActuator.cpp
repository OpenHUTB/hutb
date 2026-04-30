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

#include "MuJoCo/Components/Actuators/MjDamperActuator.h"

#include "XmlNode.h"
#include "MuJoCo/Utils/MjXmlUtils.h"
#include "Utils/URLabLogging.h"

UMjDamperActuator::UMjDamperActuator()
{
    Type = EMjActuatorType::Damper;
}

void UMjDamperActuator::ParseSpecifics(const FXmlNode* Node)
{
    MjXmlUtils::ReadAttrDouble(Node, TEXT("kv"), Kv, bOverride_Kv);
}

void UMjDamperActuator::ExtractSpecifics(const mjsActuator* act)
{
    // mjs_setToDamper sets:
    // gainprm[0] = kv
    // biasprm[2] = -kv
    if (BiasPrm.Num() > 2)
    {
        Kv = -BiasPrm[2];
        bOverride_Kv = true;
    }
}

void UMjDamperActuator::ExportTo(mjsActuator* act, mjsDefault* def)
{
    if (!act) return;

    Super::ExportTo(act, def);                         // 1. Common: transmission, gear, ranges, group...
    mjs_setToDamper(act, bOverride_Kv ? Kv : 0.01f);  // 2. Type preset: sets dyntype/gaintype/biastype/prm
    ApplyRawOverrides(act, def);                       // 3. Raw prm overrides win last (if set)
}
