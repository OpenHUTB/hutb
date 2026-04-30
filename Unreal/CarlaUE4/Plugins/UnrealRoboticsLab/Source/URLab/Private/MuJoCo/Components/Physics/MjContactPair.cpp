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

#include "MuJoCo/Components/Physics/MjContactPair.h"
#include "XmlFile.h"
#include "mujoco/mujoco.h"
#include "MuJoCo/Core/Spec/MjSpecWrapper.h"
#include "MuJoCo/Utils/MjXmlUtils.h"
#include "MuJoCo/Components/MjComponent.h"
#include "MuJoCo/Components/Geometry/MjGeom.h"
#include "Utils/URLabLogging.h"

UMjContactPair::UMjContactPair()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UMjContactPair::BeginPlay()
{
	Super::BeginPlay();
}

void UMjContactPair::ImportFromXml(const FXmlNode* Node)
{
    if (!Node)
    {
        return;
    }

    // Required attributes
    MjXmlUtils::ReadAttrString(Node, TEXT("geom1"), Geom1);
    MjXmlUtils::ReadAttrString(Node, TEXT("geom2"), Geom2);

    // Optional attributes
    MjXmlUtils::ReadAttrString(Node, TEXT("name"), Name);

    {
        bool bCondimOverride = false;
        MjXmlUtils::ReadAttrInt(Node, TEXT("condim"), Condim, bCondimOverride);
    }

    // Parse friction/solref/solimp arrays
    {
        bool bFrictionOverride = false, bSolrefOverride = false, bSolimpOverride = false;
        MjXmlUtils::ReadAttrFloatArray(Node, TEXT("friction"), Friction, bFrictionOverride);
        MjXmlUtils::ReadAttrFloatArray(Node, TEXT("solref"),   Solref,   bSolrefOverride);
        MjXmlUtils::ReadAttrFloatArray(Node, TEXT("solimp"),   Solimp,   bSolimpOverride);
    }

    {
        bool bGapOverride = false, bMarginOverride = false;
        MjXmlUtils::ReadAttrFloat(Node, TEXT("gap"),    Gap,    bGapOverride);
        MjXmlUtils::ReadAttrFloat(Node, TEXT("margin"), Margin, bMarginOverride);
    }
}

void UMjContactPair::ExportTo(mjsPair* pair)
{
    if (!pair)
    {
        return;
    }

    // Set required geom names
    mjs_setString(pair->geomname1, TCHAR_TO_UTF8(*Geom1));
    mjs_setString(pair->geomname2, TCHAR_TO_UTF8(*Geom2));

    // Set condim
    pair->condim = Condim;

    // Set friction
    if (Friction.Num() > 0)
    {
        for (int i = 0; i < FMath::Min(Friction.Num(), 5); i++)
        {
            pair->friction[i] = Friction[i];
        }
    }

    // Set solref
    if (Solref.Num() > 0)
    {
        for (int i = 0; i < FMath::Min(Solref.Num(), mjNREF); i++)
        {
            pair->solref[i] = Solref[i];
        }
    }

    // Set solimp
    if (Solimp.Num() > 0)
    {
        for (int i = 0; i < FMath::Min(Solimp.Num(), mjNIMP); i++)
        {
            pair->solimp[i] = Solimp[i];
        }
    }

    // Set gap and margin
    pair->gap = Gap;
    pair->margin = Margin;
}

void UMjContactPair::RegisterToSpec(FMujocoSpecWrapper& Wrapper, mjsBody* ParentBody)
{
    mjsPair* pair = mjs_addPair(Wrapper.Spec, nullptr);
    if (!Name.IsEmpty())
    {
        mjs_setName(pair->element, TCHAR_TO_UTF8(*Name));
    }
    ExportTo(pair);
    UE_LOG(LogURLabWrapper, Log, TEXT("Added contact pair: %s->%s"), *Geom1, *Geom2);
}

void UMjContactPair::Bind(mjModel* model, mjData* data, const FString& Prefix)
{
    // Contact pairs are global static data in MuJoCo, usually not bound to runtime indices easily or needed for runtime update.
}

#if WITH_EDITOR
TArray<FString> UMjContactPair::GetGeomOptions() const
{
    return UMjComponent::GetSiblingComponentOptions(this, UMjGeom::StaticClass());
}
#endif
