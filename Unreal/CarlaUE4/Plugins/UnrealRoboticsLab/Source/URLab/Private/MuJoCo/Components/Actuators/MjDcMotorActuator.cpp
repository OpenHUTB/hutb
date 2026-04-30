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

#include "MuJoCo/Components/Actuators/MjDcMotorActuator.h"

#include "XmlNode.h"
#include "MuJoCo/Utils/MjXmlUtils.h"
#include "Utils/URLabLogging.h"

UMjDcMotorActuator::UMjDcMotorActuator()
{
    Type = EMjActuatorType::DcMotor;
}

namespace
{
    /** Copy UE TArray<float> into a double[N] buffer, zero-padding the tail. */
    template <int N>
    void FillDouble(double (&Out)[N], const TArray<float>& In)
    {
        for (int i = 0; i < N; ++i)
        {
            Out[i] = (i < In.Num()) ? (double)In[i] : 0.0;
        }
    }
}

void UMjDcMotorActuator::ParseSpecifics(const FXmlNode* Node)
{
    MjXmlUtils::ReadAttrFloatArray(Node, TEXT("motorconst"),  MotorConst,  bOverride_MotorConst);
    MjXmlUtils::ReadAttrFloat     (Node, TEXT("resistance"),  Resistance,  bOverride_Resistance);
    MjXmlUtils::ReadAttrFloatArray(Node, TEXT("nominal"),     Nominal,     bOverride_Nominal);
    MjXmlUtils::ReadAttrFloatArray(Node, TEXT("saturation"),  Saturation,  bOverride_Saturation);
    MjXmlUtils::ReadAttrFloatArray(Node, TEXT("inductance"),  Inductance,  bOverride_Inductance);
    MjXmlUtils::ReadAttrFloatArray(Node, TEXT("cogging"),     Cogging,     bOverride_Cogging);
    MjXmlUtils::ReadAttrFloatArray(Node, TEXT("controller"),  Controller,  bOverride_Controller);
    MjXmlUtils::ReadAttrFloatArray(Node, TEXT("thermal"),     Thermal,     bOverride_Thermal);
    MjXmlUtils::ReadAttrFloatArray(Node, TEXT("lugre"),       LuGre,       bOverride_LuGre);

    const FString InputStr = Node->GetAttribute(TEXT("input"));
    if (!InputStr.IsEmpty())
    {
        if      (InputStr == TEXT("voltage"))  Input = EMjDcMotorInput::Voltage;
        else if (InputStr == TEXT("position")) Input = EMjDcMotorInput::Position;
        else if (InputStr == TEXT("velocity")) Input = EMjDcMotorInput::Velocity;
        bOverride_Input = true;
    }
}

void UMjDcMotorActuator::ExtractSpecifics(const mjsActuator* /*Actuator*/)
{
    // DC motor parameters are packed across gainprm/biasprm/dynprm with a
    // non-trivial layout (see `engine/engine_xml_native_reader.cc::dcmotor`).
    // Extracting them cleanly on the reverse path isn't needed for the XML
    // import flow (that goes through ParseSpecifics) and isn't wired up yet.
}

void UMjDcMotorActuator::ExportTo(mjsActuator* Actuator, mjsDefault* Default)
{
    if (!Actuator) return;

    Super::ExportTo(Actuator, Default);

    // mjs_setToDCMotor treats null array pointers as "don't override, keep
    // whatever the default / inherited class set". Pass nullptr for any group
    // the user hasn't opted into so muscles of defaults-chains keep their
    // inherited values.
    double MotorConstBuf [2]; FillDouble(MotorConstBuf, MotorConst);
    double NominalBuf    [3]; FillDouble(NominalBuf,    Nominal);
    double SaturationBuf [3]; FillDouble(SaturationBuf, Saturation);
    double InductanceBuf [2]; FillDouble(InductanceBuf, Inductance);
    double CoggingBuf    [3]; FillDouble(CoggingBuf,    Cogging);
    double ControllerBuf [6]; FillDouble(ControllerBuf, Controller);
    double ThermalBuf    [6]; FillDouble(ThermalBuf,    Thermal);
    double LuGreBuf      [5]; FillDouble(LuGreBuf,      LuGre);

    const double ResistanceArg = bOverride_Resistance ? (double)Resistance : 0.0;
    const int    InputModeArg  = bOverride_Input ? (int)Input : 0;

    const char* err = mjs_setToDCMotor(
        Actuator,
        bOverride_MotorConst  ? MotorConstBuf  : nullptr,
        ResistanceArg,
        bOverride_Nominal     ? NominalBuf     : nullptr,
        bOverride_Saturation  ? SaturationBuf  : nullptr,
        bOverride_Inductance  ? InductanceBuf  : nullptr,
        bOverride_Cogging     ? CoggingBuf     : nullptr,
        bOverride_Controller  ? ControllerBuf  : nullptr,
        bOverride_Thermal     ? ThermalBuf     : nullptr,
        bOverride_LuGre       ? LuGreBuf       : nullptr,
        InputModeArg);

    if (err && err[0])
    {
        UE_LOG(LogURLabImport, Warning,
            TEXT("[MjDcMotorActuator] '%s' mjs_setToDCMotor returned: %s"),
            *GetName(), UTF8_TO_TCHAR(err));
    }

    ApplyRawOverrides(Actuator, Default);
}
