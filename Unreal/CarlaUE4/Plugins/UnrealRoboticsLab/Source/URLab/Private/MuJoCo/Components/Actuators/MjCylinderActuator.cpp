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

#include "MuJoCo/Components/Actuators/MjCylinderActuator.h"

#include "XmlNode.h"
#include "MuJoCo/Utils/MjXmlUtils.h"
#include "Utils/URLabLogging.h"

UMjCylinderActuator::UMjCylinderActuator()
{
    Type = EMjActuatorType::Cylinder;
}

void UMjCylinderActuator::ParseSpecifics(const FXmlNode* Node)
{
    MjXmlUtils::ReadAttrFloat(Node, TEXT("timeconst"), TimeConst, bOverride_TimeConst);
    MjXmlUtils::ReadAttrFloat(Node, TEXT("bias"),      Bias,      bOverride_Bias);
    MjXmlUtils::ReadAttrFloat(Node, TEXT("area"),      Area,      bOverride_Area);
    MjXmlUtils::ReadAttrFloat(Node, TEXT("diameter"),  Diameter,  bOverride_Diameter);
}

void UMjCylinderActuator::ExtractSpecifics(const mjsActuator* act)
{
    // Extract parameters
    if (DynPrm.Num() > 0) { TimeConst = DynPrm[0]; bOverride_TimeConst = true; }
    
    if (BiasPrm.Num() > 3) { Bias = BiasPrm[3]; bOverride_Bias = true; }
    
    if (GainPrm.Num() > 0) { Area = GainPrm[0]; bOverride_Area = true; }
    if (GainPrm.Num() > 1) { Diameter = GainPrm[1]; bOverride_Diameter = true; }
}

void UMjCylinderActuator::ExportTo(mjsActuator* act, mjsDefault* def)
{
    if (!act) return;

    Super::ExportTo(act, def); // 1. Common attributes (populates act from default via mjs_addActuator)

    // Mirror OneActuator: read from act struct as inherited defaults, override only if explicitly set.
    const double timeconst = bOverride_TimeConst ? (double)TimeConst : act->dynprm[0];
    const double bias      = bOverride_Bias      ? (double)Bias      : act->biasprm[0];
    const double area      = bOverride_Area      ? (double)Area      : act->gainprm[0];
    const double diameter  = bOverride_Diameter  ? (double)Diameter  : -1.0; // -1 = derive from area

    mjs_setToCylinder(act, timeconst, bias, area, diameter); // 2. Type preset
    ApplyRawOverrides(act, def);                             // 3. Raw prm overrides
}
