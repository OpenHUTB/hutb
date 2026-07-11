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


// 将已编译/存在的 MuJoCo mjsActuator 中的低级参数提取回 Unreal组件（UMjCylinderActuator）的成员变量，便于编辑器显示、反向导入round-trip编辑
void UMjCylinderActuator::ExtractSpecifics(const mjsActuator* act)
{
    // 提取参数
    // TimeConst：执行器动力学的时间常数（一阶响应的 τ），控制执行器从控制输入到实际力/压力响应的速度（越大响应越慢）
    if (DynPrm.Num() > 0) { TimeConst = DynPrm[0]; bOverride_TimeConst = true; }
    
    // Bias：执行器的偏置力/压力，通常用于补偿静态负载或预加载
    if (BiasPrm.Num() > 3) { Bias = BiasPrm[3]; bOverride_Bias = true; }
    
    // Area：执行器的有效横截面积，影响力与压力之间的转换
    if (GainPrm.Num() > 0) { Area = GainPrm[0]; bOverride_Area = true; }

    // Diameter：执行器的直径，通常用于计算流体动力学特性或几何约束
    if (GainPrm.Num() > 1) { Diameter = GainPrm[1]; bOverride_Diameter = true; }
}

void UMjCylinderActuator::ExportTo(mjsActuator* act, mjsDefault* def)
{
    if (!act) return;

    Super::ExportTo(act, def); // 1. 常见属性（通过 mjs_addActuator 从默认值填充 act）

    // Mirror OneActuator：从 act 结构读取继承的默认值，只有在显式设置时才覆盖。
    const double timeconst = bOverride_TimeConst ? (double)TimeConst : act->dynprm[0];
    const double bias      = bOverride_Bias      ? (double)Bias      : act->biasprm[0];
    const double area      = bOverride_Area      ? (double)Area      : act->gainprm[0];
    const double diameter  = bOverride_Diameter  ? (double)Diameter  : -1.0; // -1 = 来源于面积

    mjs_setToCylinder(act, timeconst, bias, area, diameter); // 2. 类型预设
    ApplyRawOverrides(act, def);                             // 3. 原始参数覆盖
}
