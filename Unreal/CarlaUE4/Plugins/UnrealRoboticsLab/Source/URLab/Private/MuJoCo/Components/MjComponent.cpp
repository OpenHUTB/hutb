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

#include "MuJoCo/Components/MjComponent.h"
#include "MuJoCo/Core/Spec/MjSpecWrapper.h"
#include "MuJoCo/Components/Defaults/MjDefault.h"
#include "MuJoCo/Components/Bodies/MjBody.h"

#if WITH_EDITOR
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Engine/SimpleConstructionScript.h"
#include "Engine/SCS_Node.h"
#endif

UMjComponent::UMjComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.bStartWithTickEnabled = false;
}

void UMjComponent::BeginPlay()
{
	Super::BeginPlay();
}

void UMjComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
}

mjsDefault* UMjComponent::ResolveDefault(mjSpec* Spec, const FString& ClassName)
{
    // When no explicit class is set, return nullptr so the MuJoCo spec API
    // resolves the parent body's childclass chain automatically.
    if (ClassName.IsEmpty()) return nullptr;

    mjsDefault* def = mjs_findDefault(Spec, TCHAR_TO_UTF8(*ClassName));
    return def ? def : nullptr;
}

void UMjComponent::SetSpecElementName(FMujocoSpecWrapper& Wrapper, mjsElement* Elem, mjtObj ObjType)
{
    FString NameToRegister = MjName.IsEmpty() ? GetName() : MjName;
    FString UniqueName = Wrapper.GetUniqueName(NameToRegister, ObjType, GetOwner());
    mjs_setName(Elem, TCHAR_TO_UTF8(*UniqueName));
}

void UMjComponent::RegisterToSpec(FMujocoSpecWrapper& Wrapper, mjsBody* ParentBody)
{
}

void UMjComponent::Bind(mjModel* model, mjData* data, const FString& Prefix)
{
    m_Model = model;
    m_Data = data;
    m_ID = -1;
}

bool UMjComponent::IsBound() const
{
    return m_Model && m_Data && m_ID >= 0;
}

FString UMjComponent::GetMjName() const
{
    return MjName;
}

UMjDefault* UMjComponent::FindDefaultByClassName(const AActor* Owner, const FString& ClassName)
{
    if (!Owner || ClassName.IsEmpty()) return nullptr;

    TArray<UMjDefault*> Defaults;
    Owner->GetComponents<UMjDefault>(Defaults);
    for (UMjDefault* D : Defaults)
    {
        if (D->ClassName == ClassName) return D;
    }
    return nullptr;
}

UMjDefault* UMjComponent::FindEditorDefault() const
{
    const AActor* Owner = GetOwner();
    if (!Owner) return nullptr;

    // 1. Check this component's own MjClassName (subclass provides via GetMjClassName)
    FString MyClassName = GetMjClassName();
    if (!MyClassName.IsEmpty())
    {
        return FindDefaultByClassName(Owner, MyClassName);
    }

    // 2. Walk up attachment parents to find nearest UMjBody with ChildClassName set
    USceneComponent* Parent = GetAttachParent();
    while (Parent)
    {
        if (UMjBody* Body = Cast<UMjBody>(Parent))
        {
            if (Body->bOverride_ChildClassName && !Body->ChildClassName.IsEmpty())
            {
                return FindDefaultByClassName(Owner, Body->ChildClassName);
            }
        }
        Parent = Parent->GetAttachParent();
    }

    return nullptr;
}

#if WITH_EDITOR
TArray<FString> UMjComponent::GetSiblingComponentOptions(const UObject* CallerComponent, UClass* FilterClass, bool bIncludeDefaults)
{
    TArray<FString> Options;
    Options.Add(TEXT(""));  // Empty = no selection

    if (!CallerComponent || !FilterClass) return Options;

    // Walk the outer chain to find the owning Blueprint
    UBlueprint* BP = nullptr;
    for (UObject* Outer = CallerComponent->GetOuter(); Outer; Outer = Outer->GetOuter())
    {
        if (UBlueprintGeneratedClass* BPGC = Cast<UBlueprintGeneratedClass>(Outer))
        {
            BP = Cast<UBlueprint>(BPGC->ClassGeneratedBy);
            break;
        }
        if (UBlueprint* Found = Cast<UBlueprint>(Outer))
        {
            BP = Found;
            break;
        }
    }

    if (!BP || !BP->SimpleConstructionScript) return Options;

    for (USCS_Node* Node : BP->SimpleConstructionScript->GetAllNodes())
    {
        UMjComponent* MjComp = Cast<UMjComponent>(Node->ComponentTemplate);
        if (MjComp && MjComp->IsA(FilterClass))
        {
            if (MjComp->bIsDefault && !bIncludeDefaults) continue;

            FString DisplayName = MjComp->MjName.IsEmpty()
                ? Node->GetVariableName().ToString()
                : MjComp->MjName;
            Options.Add(DisplayName);
        }
    }

    return Options;
}
#endif
