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

#include "MuJoCo/Components/Actuators/MjMuscleActuator.h"

#include "XmlNode.h"
#include "MuJoCo/Utils/MjXmlUtils.h"
#include "Utils/URLabLogging.h"

UMjMuscleActuator::UMjMuscleActuator()
{
    Type = EMjActuatorType::Muscle;
}

void UMjMuscleActuator::ParseSpecifics(const FXmlNode* Node)
{
    // Parses timeconst[2] "0.01 0.04"
    FString TimeConstStr = Node->GetAttribute(TEXT("timeconst"));
    if (!TimeConstStr.IsEmpty()) 
    {
        TArray<FString> Parts;
        TimeConstStr.ParseIntoArray(Parts, TEXT(" "), true);
        if (Parts.Num() > 0) 
        { 
            TimeConst = FCString::Atof(*Parts[0]); 
            bOverride_TimeConst = true; 
            UE_LOG(LogURLabImport, Log, TEXT("[MjMuscleActuator::ParseSpecifics] '%s' -> TimeConst: %f"), *GetName(), TimeConst);
        }
        if (Parts.Num() > 1) 
        { 
            TimeConst2 = FCString::Atof(*Parts[1]); 
            bOverride_TimeConst2 = true; 
            UE_LOG(LogURLabImport, Log, TEXT("[MjMuscleActuator::ParseSpecifics] '%s' -> TimeConst2: %f"), *GetName(), TimeConst2);
        }
    }

    MjXmlUtils::ReadAttrFloat(Node, TEXT("tausmooth"), TauSmooth, bOverride_TauSmooth);
    MjXmlUtils::ReadAttrFloat(Node, TEXT("force"),     Force,     bOverride_Force);
    MjXmlUtils::ReadAttrFloat(Node, TEXT("scale"),     Scale,     bOverride_Scale);
    MjXmlUtils::ReadAttrFloat(Node, TEXT("lmin"),      LMin,      bOverride_LMin);
    MjXmlUtils::ReadAttrFloat(Node, TEXT("lmax"),      LMax,      bOverride_LMax);
    MjXmlUtils::ReadAttrFloat(Node, TEXT("vmax"),      VMax,      bOverride_VMax);
    MjXmlUtils::ReadAttrFloat(Node, TEXT("fpmax"),     FPMax,     bOverride_FPMax);
    MjXmlUtils::ReadAttrFloat(Node, TEXT("fvmax"),     FVMax,     bOverride_FVMax);
}

void UMjMuscleActuator::ExtractSpecifics(const mjsActuator* act)
{
    // Extract from parameters
    if (DynPrm.Num() > 0) { TimeConst = DynPrm[0]; bOverride_TimeConst = true; }
    if (DynPrm.Num() > 1) { TimeConst2 = DynPrm[1]; bOverride_TimeConst2 = true; }
    if (DynPrm.Num() > 2) { TauSmooth = DynPrm[2]; bOverride_TauSmooth = true; }

    if (GainPrm.Num() > 2) { Force = GainPrm[2]; bOverride_Force = true; }
    if (GainPrm.Num() > 3) { Scale = GainPrm[3]; bOverride_Scale = true; }
    if (GainPrm.Num() > 4) { LMin = GainPrm[4]; bOverride_LMin = true; }
    if (GainPrm.Num() > 5) { LMax = GainPrm[5]; bOverride_LMax = true; }
    if (GainPrm.Num() > 6) { VMax = GainPrm[6]; bOverride_VMax = true; }
    if (GainPrm.Num() > 7) { FPMax = GainPrm[7]; bOverride_FPMax = true; }
    if (GainPrm.Num() > 8) { FVMax = GainPrm[8]; bOverride_FVMax = true; }
}

void UMjMuscleActuator::ExportTo(mjsActuator* act, mjsDefault* def)
{
    if (!act) return;

    Super::ExportTo(act, def);

    // mjs_setToMuscle dereferences timeconst[0..1] and range[0..1] unconditionally,
    // treating -1 as the "not overridden — leave alone" sentinel. Never pass null.
    // `range` here is the muscle-specific length range (gainprm[0..1] = lrmin, lrmax),
    // NOT the actuator's ctrlrange — passing ctrlrange here would clobber the
    // muscle defaults (0.75, 1.05). We don't yet expose this as a UPROPERTY, so
    // pass the sentinel to keep MuJoCo's built-in muscle defaults.
    double timeconst[2] = {
        bOverride_TimeConst  ? (double)TimeConst  : -1.0,
        bOverride_TimeConst2 ? (double)TimeConst2 : -1.0
    };
    double range[2] = { -1.0, -1.0 };

    const double tausmooth = bOverride_TauSmooth ? (double)TauSmooth : act->dynprm[2];

    mjs_setToMuscle(act,
        timeconst,
        tausmooth,
        range,
        bOverride_Force  ? (double)Force  : -1.0,
        bOverride_Scale  ? (double)Scale  : -1.0,
        bOverride_LMin   ? (double)LMin   : -1.0,
        bOverride_LMax   ? (double)LMax   : -1.0,
        bOverride_VMax   ? (double)VMax   : -1.0,
        bOverride_FPMax  ? (double)FPMax  : -1.0,
        bOverride_FVMax  ? (double)FVMax  : -1.0);

    ApplyRawOverrides(act, def);
}
