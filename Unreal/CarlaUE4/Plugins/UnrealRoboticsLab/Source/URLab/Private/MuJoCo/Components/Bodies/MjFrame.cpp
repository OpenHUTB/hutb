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
    // Frames directly under worldbody receive a null ParentBody; resolve it now.
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
        // 1. Recursive Frames
        if (UMjFrame* MjFrameComp = Cast<UMjFrame>(CurrentComponent))
        {
            MjFrameComp->Setup(this, ParentBody, Wrapper);
            continue;
        }

        // 2. Check for child Bodies (attached to the parent body, not the frame directly, 
        // but we respect Unreal hierarchy for coordinate lookup)
        if (UMjBody* MjBodyComp = Cast<UMjBody>(CurrentComponent))
        {
            MjBodyComp->Setup(this, ParentBody, Wrapper);
            continue;
        }

        // 3. Register standard Spec Elements (Geoms, Sites, etc.) to the frame's element.
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
    // Frames are compiled away by MuJoCo, so they don't have IDs in mjModel/mjData.
    // They don't need runtime binding or ticking.
    m_Model = Model;
    m_Data = Data;
    m_ID = -1; // Not targetable at runtime by ID
}

void UMjFrame::ImportFromXml(const FXmlNode* Node, const FMjCompilerSettings& CompilerSettings)
{
    if (!Node) return;

    // Position — same as body: scale ×100, negate Y
    FString PosStr = Node->GetAttribute(TEXT("pos"));
    if (!PosStr.IsEmpty())
    {
        FVector MjPos = MjXmlUtils::ParseVector(PosStr);
        SetRelativeLocation(MjUtils::MjToUEPosition(&MjPos.X));
    }

    // Orientation (quat/axisangle/euler/xyaxes/zaxis in priority order)
    double MjQuat[4];
    if (MjOrientationUtils::OrientationToMjQuat(Node, CompilerSettings, MjQuat))
    {
        SetRelativeRotation(MjUtils::MjToUERotation(MjQuat));
    }
}

void UMjFrame::RegisterToSpec(FMujocoSpecWrapper& Wrapper, mjsBody* ParentBody)
{
    // MjFrame is handled recursively via Setup().
    if (ParentBody)
    {
        Setup(GetAttachParent(), ParentBody, &Wrapper);
    }
}
