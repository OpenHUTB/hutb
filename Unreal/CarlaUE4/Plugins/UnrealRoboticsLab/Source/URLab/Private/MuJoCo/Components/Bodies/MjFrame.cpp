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

#include "MuJoCo/Components/Bodies/MjFrame.h"
#include "MuJoCo/Core/Spec/MjSpecWrapper.h"
#include "MuJoCo/Components/Geometry/MjGeom.h"
#include "MuJoCo/Components/Geometry/MjSite.h"
#include "MuJoCo/Components/Joints/MjJoint.h"
#include "MuJoCo/Components/Sensors/MjSensor.h"
#include "MuJoCo/Components/Actuators/MjActuator.h"
#include "MuJoCo/Components/Bodies/MjBody.h"
#include "MuJoCo/Utils/MjBind.h"
#include "MuJoCo/Utils/MjUtils.h"
#include "MuJoCo/Utils/MjXmlUtils.h"
#include "MuJoCo/Utils/MjOrientationUtils.h"
#include "XmlNode.h"

UMjFrame::UMjFrame()
{
    PrimaryComponentTick.bCanEverTick = false;
}

void UMjFrame::Setup(USceneComponent* Parent, mjsBody* ParentBody, FMujocoSpecWrapper* Wrapper)
{
    // 世界主体(worldbody)正下方的帧接收到空的 ParentBody；立即解决它。
    if (!ParentBody && Wrapper && Wrapper->Spec)
        ParentBody = mjs_findBody(Wrapper->Spec, "world");

    FTransform TargetTransform = GetRelativeTransform();

    FString NameToRegister = MjName.IsEmpty() ? GetName() : MjName;
    mjsFrame* FrameInSpec = Wrapper->CreateFrame(
        NameToRegister,
        ParentBody,
        TargetTransform
    );

    if (FrameInSpec)
    {
        m_SpecElement = FrameInSpec->element;
    }

    TArray<USceneComponent*> DirectChildren = GetAttachChildren();

    for (USceneComponent* CurrentComponent : DirectChildren)
    {
        // 1. 递归帧
        if (UMjFrame* MjFrameComp = Cast<UMjFrame>(CurrentComponent))
        {
            MjFrameComp->Setup(this, ParentBody, Wrapper);
            continue;
        }

        // 2. 检查子体(Bodies)（附加到父体上，而不是直接附加到帧上，
        // 但我们遵循虚幻引擎的层级结构进行坐标查找）
        if (UMjBody* MjBodyComp = Cast<UMjBody>(CurrentComponent))
        {
            MjBodyComp->Setup(this, ParentBody, Wrapper);
            continue;
        }

        // 3. 将标准规范元素（几何体、站点等）注册到帧元素。
        if (CurrentComponent->GetClass()->ImplementsInterface(UMjSpecElement::StaticClass()))
        {
            IMjSpecElement* SpecElem = Cast<IMjSpecElement>(CurrentComponent);
            if (SpecElem)
            {
                SpecElem->RegisterToSpec(*Wrapper, ParentBody);
                m_SpecElements.Emplace(CurrentComponent);
            }
        }
    }
}

void UMjFrame::Bind(mjModel* Model, mjData* Data, const FString& Prefix)
{
    // MuJoCo 会将帧编译掉，因此 mjModel/mjData 中没有帧 ID。
    // 它们不需要运行时绑定或节拍机制。
    m_Model = Model;
    m_Data = Data;
    m_ID = -1; // 运行时无法通过 ID 定位
}

void UMjFrame::ImportFromXml(const FXmlNode* Node, const FMjCompilerSettings& CompilerSettings)
{
    if (!Node) return;

    // 位置——与身体相同：缩放×100，Y轴取反。
    FString PosStr = Node->GetAttribute(TEXT("pos"));
    if (!PosStr.IsEmpty())
    {
        FVector MjPos = MjXmlUtils::ParseVector(PosStr);
        SetRelativeLocation(MjUtils::MjToUEPosition(&MjPos.X));
    }

    // 朝向 (按优先级顺序为：quat/axisangle/euler/xyaxes/zaxis)
    double MjQuat[4];
    if (MjOrientationUtils::OrientationToMjQuat(Node, CompilerSettings, MjQuat))
    {
        SetRelativeRotation(MjUtils::MjToUERotation(MjQuat));
    }
}

void UMjFrame::RegisterToSpec(FMujocoSpecWrapper& Wrapper, mjsBody* ParentBody)
{
    // MjFrame 通过 Setup() 递归处理。
    if (ParentBody)
    {
        Setup(GetAttachParent(), ParentBody, &Wrapper);
    }
}
