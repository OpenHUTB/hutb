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

#include "MuJoCo/Components/Defaults/MjDefault.h"
#include "XmlNode.h"
#include "MuJoCo/Utils/MjBind.h"
#include "Utils/URLabLogging.h"
#include "MuJoCo/Components/Geometry/MjGeom.h"
#include "MuJoCo/Components/Geometry/MjSite.h"
#include "MuJoCo/Components/Joints/MjJoint.h"
#include "MuJoCo/Components/Actuators/MjActuator.h"
#include "MuJoCo/Components/Tendons/MjTendon.h"
#include "MuJoCo/Components/Sensors/MjCamera.h"

UMjDefault::UMjDefault()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UMjDefault::ImportFromXml(const FXmlNode* Node)
{
    if (!Node) return;

    UE_LOG(LogURLabWrapper, Verbose, TEXT("[UMjDefault::ImportFromXml] Importing Default Class '%s' (Parent: '%s')"), *ClassName, *ParentClassName);
    FString ClassAttr = Node->GetAttribute(TEXT("class"));
    if (!ClassAttr.IsEmpty()) ClassName = ClassAttr;
    else ClassName = TEXT("main");
}

#if WITH_EDITOR
void UMjDefault::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
    Super::PostEditChangeProperty(PropertyChangedEvent);

    // If ClassName is empty after any edit, auto-populate from component name
    if (ClassName.IsEmpty())
    {
        ClassName = GetName();
        ClassName.ReplaceInline(TEXT("_GEN_VARIABLE"), TEXT(""));
    }
}
#endif



void UMjDefault::ExportTo(mjsDefault* def)
{
    if (!def) return;

    UE_LOG(LogURLabWrapper, Verbose, TEXT("[UMjDefault::ExportTo] Exporting Default Class '%s' (Parent: '%s')"), *ClassName, *ParentClassName);

    // Iterate over attached children to find default definitions
    TArray<USceneComponent*> Children;
    GetChildrenComponents(false, Children); // Use false to get direct children only

    for (USceneComponent* Child : Children)
    {
        if (UMjGeom* GeomComp = Cast<UMjGeom>(Child))
        {
            // Export to def->geom
            // We pass nullptr as the second argument because we are defining the default itself, not using one.
            GeomComp->ExportTo(def->geom, nullptr);
            UE_LOG(LogURLabWrapper, Verbose, TEXT("  - [Geom] Exported geometry defaults from %s"), *GeomComp->GetName());
        }
        else if (UMjJoint* JointComp = Cast<UMjJoint>(Child))
        {
            // Export to def->joint
            JointComp->ExportTo(def->joint, nullptr);
            UE_LOG(LogURLabWrapper, Verbose, TEXT("  - [Joint] Exported joint defaults from %s"), *JointComp->GetName());
        }
        else if (UMjActuator* ActuatorComp = Cast<UMjActuator>(Child))
        {
            ActuatorComp->ExportTo(def->actuator, nullptr);
            UE_LOG(LogURLabWrapper, Verbose, TEXT("  - [Actuator] Exported actuator defaults from %s"), *ActuatorComp->GetName());
        }
        else if (UMjTendon* TendonComp = Cast<UMjTendon>(Child))
        {
            TendonComp->ExportTo(def->tendon, nullptr);
            UE_LOG(LogURLabWrapper, Verbose, TEXT("  - [Tendon] Exported tendon defaults from %s"), *TendonComp->GetName());
        }
        else if (UMjSite* SiteComp = Cast<UMjSite>(Child))
        {
            SiteComp->ExportTo(def->site, nullptr);
            UE_LOG(LogURLabWrapper, Verbose, TEXT("  - [Site] Exported site defaults from %s"), *SiteComp->GetName());
        }
        else if (UMjCamera* CameraComp = Cast<UMjCamera>(Child))
        {
            CameraComp->ExportTo(def->camera, nullptr);
            UE_LOG(LogURLabWrapper, Verbose, TEXT("  - [Camera] Exported camera defaults from %s"), *CameraComp->GetName());
        }
    }
}
