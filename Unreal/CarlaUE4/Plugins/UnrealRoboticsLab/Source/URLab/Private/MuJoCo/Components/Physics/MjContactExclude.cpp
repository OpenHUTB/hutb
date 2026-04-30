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

#include "MuJoCo/Components/Physics/MjContactExclude.h"
#include "XmlFile.h"
#include "mujoco/mujoco.h"
#include "MuJoCo/Core/Spec/MjSpecWrapper.h"
#include "MuJoCo/Components/MjComponent.h"
#include "MuJoCo/Components/Bodies/MjBody.h"
#include "Utils/URLabLogging.h"

UMjContactExclude::UMjContactExclude()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UMjContactExclude::BeginPlay()
{
	Super::BeginPlay();
}

void UMjContactExclude::ImportFromXml(const FXmlNode* Node)
{
    if (!Node)
    {
        return;
    }

    // Required attributes
    Body1 = Node->GetAttribute(TEXT("body1"));
    Body2 = Node->GetAttribute(TEXT("body2"));

    // Optional attributes
    Name = Node->GetAttribute(TEXT("name"));
}

void UMjContactExclude::ExportTo(mjsExclude* exclude)
{
    if (!exclude)
    {
        return;
    }

    // Set required body names
    mjs_setString(exclude->bodyname1, TCHAR_TO_UTF8(*Body1));
    mjs_setString(exclude->bodyname2, TCHAR_TO_UTF8(*Body2));
}

void UMjContactExclude::RegisterToSpec(FMujocoSpecWrapper& Wrapper, mjsBody* ParentBody)
{
    mjsExclude* exclude = mjs_addExclude(Wrapper.Spec);
    ExportTo(exclude);
    UE_LOG(LogURLabWrapper, Log, TEXT("Added contact exclude: %s<->%s"), *Body1, *Body2);
}

void UMjContactExclude::Bind(mjModel* model, mjData* data, const FString& Prefix)
{
    // Contact excludes are global static data.
}

#if WITH_EDITOR
TArray<FString> UMjContactExclude::GetBodyOptions() const
{
    return UMjComponent::GetSiblingComponentOptions(this, UMjBody::StaticClass());
}
#endif
