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

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "MjNetworkManager.generated.h"

// Forward declarations
class UMjZmqComponent;
class UMjCamera;

/**
 * @class UMjNetworkManager
 * @brief Manages network-related functionality: ZMQ component discovery and camera streaming.
 */
UCLASS(ClassGroup=(MuJoCo), meta=(BlueprintSpawnableComponent))
class URLAB_API UMjNetworkManager : public UActorComponent
{
    GENERATED_BODY()

public:
    UMjNetworkManager();

    /** @brief List of ZMQ components discovered on the owner actor. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "MuJoCo|Network")
    TArray<UMjZmqComponent*> ZmqComponents;

    /** @brief If true, forces all UMjCameras to enable ZMQ broadcasting. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MuJoCo|Network")
    bool bEnableAllCameras = true;

    /** @brief Updates the camera streaming state for all cameras according to bEnableAllCameras. */
    void UpdateCameraStreamingState();

    /** @brief Thread-safe camera registration. */
    void RegisterCamera(UMjCamera* Cam);

    /** @brief Thread-safe camera unregistration. */
    void UnregisterCamera(UMjCamera* Cam);

    /** @brief Thread-safe access to active cameras. */
    TArray<UMjCamera*> GetActiveCameras();

    /** @brief Discovers ZMQ components attached to the owner actor. */
    void DiscoverZmqComponents();

private:
    TArray<UMjCamera*> ActiveCameras;
    FCriticalSection CameraMutex;
};
