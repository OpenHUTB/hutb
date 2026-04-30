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

#include "MuJoCo/Components/Actuators/MjIntVelocityActuator.h"

#include "XmlNode.h"
#include "MuJoCo/Utils/MjXmlUtils.h"
#include "Utils/URLabLogging.h"

UMjIntVelocityActuator::UMjIntVelocityActuator()
{
    Type = EMjActuatorType::IntVelocity;
}

void UMjIntVelocityActuator::ParseSpecifics(const FXmlNode* Node)
{
    MjXmlUtils::ReadAttrFloat(Node,  TEXT("kp"),           Kp,           bOverride_Kp);
    MjXmlUtils::ReadAttrDouble(Node, TEXT("kv"),           Kv,           bOverride_Kv);
    MjXmlUtils::ReadAttrFloat(Node,  TEXT("dampratio"),    DampRatio,    bOverride_DampRatio);
    MjXmlUtils::ReadAttrFloat(Node,  TEXT("timeconst"),    TimeConst,    bOverride_TimeConst);
    MjXmlUtils::ReadAttrFloat(Node,  TEXT("inheritrange"), InheritRange, bOverride_InheritRange);
}

void UMjIntVelocityActuator::ExtractSpecifics(const mjsActuator* act)
{
    if (GainPrm.Num() > 0)
    {
        Kp = GainPrm[0];
        bOverride_Kp = true;
    }
    
    if (BiasPrm.Num() > 2)
    {
        Kv = -BiasPrm[2];
        bOverride_Kv = true;
    }

    if (act->inheritrange != 0.0)
    {
        InheritRange = act->inheritrange;
        bOverride_InheritRange = true;
    }
}

void UMjIntVelocityActuator::ExportTo(mjsActuator* act, mjsDefault* def)
{
    if (!act) return;

    double kvArr[1] = { (double)Kv };
    double dampratioArr[1] = { (double)DampRatio };
    double timeconstArr[1] = { (double)TimeConst };

    double* kv_ptr        = bOverride_Kv        ? kvArr        : nullptr;
    double* dampratio_ptr = bOverride_DampRatio  ? dampratioArr : nullptr;
    double* timeconst_ptr = bOverride_TimeConst  ? timeconstArr : nullptr;

    double inherit = bOverride_InheritRange ? InheritRange : 0.0;

    Super::ExportTo(act, def); // 1. Common attributes (populates act from default via mjs_addActuator)

    // Mirror OneActuator: kp = act->gainprm[0] as inherited default, override only if explicitly set.
    double default_kp = bOverride_Kp ? (double)Kp : act->gainprm[0];

    mjs_setToIntVelocity(act, default_kp, kv_ptr, dampratio_ptr, timeconst_ptr, inherit); // 2. Type preset
    ApplyRawOverrides(act, def);                                                           // 3. Raw prm overrides
}
