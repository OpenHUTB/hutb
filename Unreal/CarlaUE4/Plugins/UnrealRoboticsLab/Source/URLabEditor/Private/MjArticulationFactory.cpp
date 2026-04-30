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

#include "MjArticulationFactory.h"
#include "MujocoGenerationAction.h"
#include "MuJoCo/Core/MjArticulation.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Engine/Blueprint.h"
#include "AssetToolsModule.h"
#include "URLab.h"

UMjArticulationFactory::UMjArticulationFactory()
{
    SupportedClass = UBlueprint::StaticClass();
    bCreateNew = true;
    bEditAfterNew = true;
}

UObject* UMjArticulationFactory::FactoryCreateNew(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn)
{
    UClass* ParentClass = AMjArticulation::StaticClass();

    // Create the Blueprint Asset using AMjArticulation as base
    UBlueprint* NewBP = FKismetEditorUtilities::CreateBlueprint(
        ParentClass,
        InParent,
        InName,
        BPTYPE_Normal,
        UBlueprint::StaticClass(),
        UBlueprintGeneratedClass::StaticClass()
    );

    if (NewBP)
    {
        // Pre-populate with organizational roots and a main body
        UMujocoGenerationAction* Generator = NewObject<UMujocoGenerationAction>();
        Generator->SetupEmptyArticulation(NewBP);
        
        // Final compile
        FKismetEditorUtilities::CompileBlueprint(NewBP);
    }

    return NewBP;
}

uint32 UMjArticulationFactory::GetMenuCategories() const
{
    return FURLabModule::Get().GetMujocoAssetCategory();
}

FText UMjArticulationFactory::GetDisplayName() const
{
    return NSLOCTEXT("FURLabModule", "MjArticulationFactoryDisplayName", "MuJoCo Articulation");
}
