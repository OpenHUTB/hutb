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

#include "MuJoCo/Components/Actuators/MjAdhesionActuator.h"

#include "XmlNode.h"
#include "MuJoCo/Utils/MjXmlUtils.h"
#include "Utils/URLabLogging.h"

// 实现 MuJoCo 中“粘附/吸附”类型执行器的导入、绑定与导出逻辑，
// 把编辑器/XML 配置映射到 MuJoCo 的 mjsActuator 预设与参数上。
UMjAdhesionActuator::UMjAdhesionActuator()
{
    Type = EMjActuatorType::Adhesion;
}

void UMjAdhesionActuator::ParseSpecifics(const FXmlNode* Node)
{
    MjXmlUtils::ReadAttrFloat(Node, TEXT("gain"), Kp, bOverride_Kp);
}

void UMjAdhesionActuator::ExtractSpecifics(const mjsActuator* act)
{
    if (GainPrm.Num() > 0)
    {
        Kp = GainPrm[0];
        bOverride_Kp = true;
    }
}

void UMjAdhesionActuator::ExportTo(mjsActuator* act, mjsDefault* def)
{
    if (!act) return;

    UE_LOG(LogURLabImport, Log, TEXT("[MjAdhesion] '%s' ExportTo: class='%s', bOverride_Kp=%s, Kp=%.3f"),
        *GetName(), *MjClassName, bOverride_Kp ? TEXT("true") : TEXT("false"), Kp);
    UE_LOG(LogURLabImport, Log, TEXT("[MjAdhesion] '%s' BEFORE Super::ExportTo: ctrlrange=[%.3f, %.3f], ctrllimited=%d"),
        *GetName(), act->ctrlrange[0], act->ctrlrange[1], act->ctrllimited);

    Super::ExportTo(act, def); // 1. 常见属性

    UE_LOG(LogURLabImport, Log, TEXT("[MjAdhesion] '%s' AFTER Super::ExportTo: ctrlrange=[%.3f, %.3f], ctrllimited=%d, bOverride_CtrlRange=%s"),
        *GetName(), act->ctrlrange[0], act->ctrlrange[1], act->ctrllimited,
        bOverride_CtrlRange ? TEXT("true") : TEXT("false"));

    // 镜像单执行器：增益 = act->gainprm[0]，作为继承的默认值，只有在明确设置时才覆盖。
    double gain = bOverride_Kp ? (double)Kp : act->gainprm[0];

    UE_LOG(LogURLabImport, Log, TEXT("[MjAdhesion] '%s' BEFORE mjs_setToAdhesion: gain=%.3f, ctrlrange=[%.3f, %.3f]"),
        *GetName(), gain, act->ctrlrange[0], act->ctrlrange[1]);

    const char* err = mjs_setToAdhesion(act, gain); // 2. 输入预设

    UE_LOG(LogURLabImport, Log, TEXT("[MjAdhesion] '%s' AFTER mjs_setToAdhesion: ctrlrange=[%.3f, %.3f], ctrllimited=%d, err='%s'"),
        *GetName(), act->ctrlrange[0], act->ctrlrange[1], act->ctrllimited,
        err && err[0] ? UTF8_TO_TCHAR(err) : TEXT("none"));

    ApplyRawOverrides(act, def);  // 3. 原始参数覆盖
}
