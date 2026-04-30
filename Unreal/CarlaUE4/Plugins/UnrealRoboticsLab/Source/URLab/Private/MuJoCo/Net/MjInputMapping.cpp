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

#include "MuJoCo/Net/MjInputMapping.h"
#include "MuJoCo/Components/Actuators/MjActuator.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Components/InputComponent.h"
#include "Kismet/GameplayStatics.h"

UMjInputMapping::UMjInputMapping()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UMjInputMapping::BeginPlay()
{
	Super::BeginPlay();
    SetupBindings();
}

void UMjInputMapping::SetupBindings()
{
    AActor* Owner = GetOwner();
    if (!Owner) return;

    // Clear cache
    ActionCache.Empty();

    // 1. Gather all Actuators on the owner (or children)
    TArray<UMjActuator*> AllActuators;
    Owner->GetComponents<UMjActuator>(AllActuators, true); // bIncludeFromChildActors = true? Maybe. default false usually fine unless nested.

    // 2. Build Cache
    for (const FMjInputBinding& Bind : Bindings)
    {
        if (!Bind.Action) continue;

        UMjActuator* TargetActuator = nullptr;
        // Find by name
        for (UMjActuator* Act : AllActuators)
        {
            if (Act->GetName().Equals(Bind.ActuatorName))
            {
                TargetActuator = Act;
                break;
            }
        }

        if (TargetActuator)
        {
            FCachedMjBinding CacheItem;
            CacheItem.Actuator = TargetActuator;
            CacheItem.Scale = Bind.Scale;
            CacheItem.bAccumulate = Bind.bAccumulate;
            
            ActionCache.Add(Bind.Action, CacheItem);
        }
        else
        {
            UE_LOG(LogURLab, Warning, TEXT("MjInputMapping: Could not find Actuator '%s' on Actor '%s'"), *Bind.ActuatorName, *Owner->GetName());
        }
    }

    // 3. Add Mapping Context
    if (APawn* Pawn = Cast<APawn>(Owner))
    {
        if (APlayerController* PC = Cast<APlayerController>(Pawn->GetController()))
        {
            if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()))
            {
                if (DefaultMappingContext)
                {
                    Subsystem->AddMappingContext(DefaultMappingContext, Priority);
                }
            }
        }
    }

    // 4. Bind Actions
    if (UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(Owner->InputComponent))
    {
        for (auto& Elem : ActionCache)
        {
            const UInputAction* Action = Elem.Key;
            // Triggered fires every frame while held; Completed/Canceled zero out on release
            EIC->BindAction(Action, ETriggerEvent::Triggered, this, &UMjInputMapping::GenericInputHandler);
        }
        
        for (auto& Elem : ActionCache)
        {
             EIC->BindAction(Elem.Key, ETriggerEvent::Completed, this, &UMjInputMapping::GenericInputHandler);
             EIC->BindAction(Elem.Key, ETriggerEvent::Canceled, this, &UMjInputMapping::GenericInputHandler);
        }
    }
}

void UMjInputMapping::GenericInputHandler(const FInputActionInstance& Instance)
{
    const UInputAction* SourceAction = Instance.GetSourceAction();
    if (!SourceAction) return;

    if (FCachedMjBinding* Binding = ActionCache.Find(SourceAction))
    {
        UMjActuator* Act = Binding->Actuator.Get();
        if (Act)
        {
            float InputVal = Instance.GetValue().Get<float>(); // Handles Axis/bool conversion
            float FinalVal = InputVal * Binding->Scale;
            
            Act->SetControl(FinalVal);
        }
    }
}
