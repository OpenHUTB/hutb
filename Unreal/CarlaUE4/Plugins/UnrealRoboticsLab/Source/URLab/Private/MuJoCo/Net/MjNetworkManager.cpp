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

#include "MuJoCo/Net/MjNetworkManager.h"
#include "MuJoCo/Core/AMjManager.h"
#include "MuJoCo/Net/MjZmqComponent.h"
#include "MuJoCo/Components/Sensors/MjCamera.h"
#include "Utils/URLabLogging.h"

UMjNetworkManager::UMjNetworkManager()
{
    PrimaryComponentTick.bCanEverTick = false;
}

void UMjNetworkManager::UpdateCameraStreamingState()
{
    UE_LOG(LogURLabNet, Log, TEXT("UpdateCameraStreamingState: Setting all cameras to streaming=%s (Global Toggle)"), bEnableAllCameras ? TEXT("TRUE") : TEXT("FALSE"));

    FScopeLock Lock(&CameraMutex);
    int32 Count = 0;
    for (UMjCamera* Cam : ActiveCameras)
    {
        if (Cam)
        {
            Cam->bEnableZmqBroadcast = bEnableAllCameras;
            Cam->SetStreamingEnabled(bEnableAllCameras);
            Count++;
            UE_LOG(LogURLabNet, Log, TEXT(" - %s Camera: %s on Actor: %s"), bEnableAllCameras ? TEXT("Enabled") : TEXT("Disabled"), *Cam->GetName(), Cam->GetOwner() ? *Cam->GetOwner()->GetName() : TEXT("None"));
        }
    }
    UE_LOG(LogURLabNet, Log, TEXT("Global camera toggle processed %d cameras."), Count);
}

void UMjNetworkManager::RegisterCamera(UMjCamera* Cam)
{
    if (!Cam) return;
    FScopeLock Lock(&CameraMutex);
    ActiveCameras.AddUnique(Cam);

    // Sync newly registered camera to the current global toggle state
    Cam->bEnableZmqBroadcast = bEnableAllCameras;
    Cam->SetStreamingEnabled(bEnableAllCameras);

    UE_LOG(LogURLabNet, Log, TEXT("UMjNetworkManager: Registered Camera %s. Total: %d"), *Cam->GetName(), ActiveCameras.Num());
}

void UMjNetworkManager::UnregisterCamera(UMjCamera* Cam)
{
    if (!Cam) return;
    FScopeLock Lock(&CameraMutex);
    ActiveCameras.Remove(Cam);
    UE_LOG(LogURLabNet, Log, TEXT("UMjNetworkManager: Unregistered Camera %s. Total: %d"), *Cam->GetName(), ActiveCameras.Num());
}

TArray<UMjCamera*> UMjNetworkManager::GetActiveCameras()
{
    FScopeLock Lock(&CameraMutex);
    return ActiveCameras;
}

void UMjNetworkManager::DiscoverZmqComponents()
{
    AActor* Owner = GetOwner();
    if (!Owner) return;

    ZmqComponents.Empty();
    TArray<UActorComponent*> Components;
    Owner->GetComponents(UMjZmqComponent::StaticClass(), Components);
    for (UActorComponent* Comp : Components)
    {
        if (UMjZmqComponent* ZmqComp = Cast<UMjZmqComponent>(Comp))
        {
            ZmqComponents.Add(ZmqComp);
        }
    }
}
