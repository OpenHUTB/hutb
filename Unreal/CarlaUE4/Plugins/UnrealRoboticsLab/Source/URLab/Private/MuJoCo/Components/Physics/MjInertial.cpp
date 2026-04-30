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

#include "MuJoCo/Components/Physics/MjInertial.h"
#include "mujoco/mujoco.h"

#include "MuJoCo/Utils/MjUtils.h"
#include "MuJoCo/Utils/MjXmlUtils.h"
#include "MuJoCo/Utils/MjOrientationUtils.h"
#include "XmlNode.h"

UMjInertial::UMjInertial()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UMjInertial::ImportFromXml(const FXmlNode* Node)
{
    FMjCompilerSettings DefaultSettings;
    ImportFromXml(Node, DefaultSettings);
}

void UMjInertial::ImportFromXml(const FXmlNode* Node, const FMjCompilerSettings& CompilerSettings)
{
    if (!Node) return;

    // 1. Pos
    FString PosStr = Node->GetAttribute(TEXT("pos"));
    if (!PosStr.IsEmpty())
    {
        FVector MjPos = MjXmlUtils::ParseVector(PosStr);
        SetRelativeLocation(MjUtils::MjToUEPosition(&MjPos.X));
    }

    // 2. Orientation (quat, axisangle, euler, xyaxes, zaxis — priority order)
    double MjQuat[4];
    if (MjOrientationUtils::OrientationToMjQuat(Node, CompilerSettings, MjQuat))
    {
        SetRelativeRotation(MjUtils::MjToUERotation(MjQuat));
    }

    // 3. Mass
    MjXmlUtils::ReadAttrFloat(Node, TEXT("mass"), Mass, bOverride_Mass);

    // 4. DiagInertia
    {
        TArray<float> DiagArr;
        if (MjXmlUtils::ReadAttrFloatArray(Node, TEXT("diaginertia"), DiagArr, bOverride_DiagInertia) && DiagArr.Num() >= 3)
            DiagInertia = FVector(DiagArr[0], DiagArr[1], DiagArr[2]);
    }

    // 5. FullInertia
    MjXmlUtils::ReadAttrFloatArray(Node, TEXT("fullinertia"), FullInertia, bOverride_FullInertia);
}

void UMjInertial::RegisterToSpec(FMujocoSpecWrapper& Wrapper, mjsBody* ParentBody)
{
    if (!ParentBody) return;

    // Set explicit inertial
    ParentBody->explicitinertial = 1;

    ParentBody->mass = Mass;
    ParentBody->inertia[0] = DiagInertia.X;
    ParentBody->inertia[1] = DiagInertia.Y;
    ParentBody->inertia[2] = DiagInertia.Z;

    if (FullInertia.Num() == 6)
    {
        for (int i = 0; i < 6; i++)
        {
            ParentBody->fullinertia[i] = FullInertia[i];
        }
    }
    
    // Transform from Component (Relative to Parent Body)
    FTransform RelTrans = GetRelativeTransform();
    MjUtils::UEToMjPosition(RelTrans.GetLocation(), ParentBody->ipos);
    MjUtils::UEToMjRotation(RelTrans.GetRotation(), ParentBody->iquat);
}

void UMjInertial::Bind(mjModel* model, mjData* data, const FString& Prefix)
{
}
