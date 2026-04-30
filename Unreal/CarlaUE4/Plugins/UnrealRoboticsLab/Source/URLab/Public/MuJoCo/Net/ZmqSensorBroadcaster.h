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

#include <atomic>

#include "CoreMinimal.h"
#include "MuJoCo/Net/MjZmqComponent.h"
#include "ZmqSensorBroadcaster.generated.h"

class AMjArticulation;
class UMjComponent;
class UMjTwistController;

UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class URLAB_API UZmqSensorBroadcaster : public UMjZmqComponent
{
	GENERATED_BODY()

public:
	UZmqSensorBroadcaster();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** Game-thread tick: builds the broadcast cache lazily. */
	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;

	// ZMQ Settings
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ZMQ")
	FString ZmqEndpoint = "tcp://*:5555";

	// Called from MuJoCo Async Thread
	virtual void InitZmq() override;
	virtual void ShutdownZmq() override;
	virtual void PostStep(mjModel* m, mjData* d) override;

private:
	void* ZmqContext = nullptr;
	void* ZmqPublisher = nullptr;
	int32 FrameCounter = 0;

	/** Per-articulation snapshot built once on the game thread and read
	 *  repeatedly from the physics thread in PostStep. Iterating
	 *  OwnedComponents on the physics thread is unsafe (the game thread
	 *  can mutate it during actor BeginPlay — e.g. auto-created twist
	 *  controllers), and tripping the sparse-array range-for ensure
	 *  corrupts nearby heap state, producing seemingly-unrelated RHI
	 *  crashes further along. */
	struct FArticulationBroadcastRecord
	{
		AMjArticulation* Articulation = nullptr;
		FString ArticPrefix;
		TArray<UMjComponent*> TelemetryComponents;
		UMjTwistController* TwistCtrl = nullptr;
	};

	/** Populated once on the game thread. bCacheBuilt (with acquire/release
	 *  ordering) publishes visibility to the physics thread. No mid-play
	 *  refresh — broadcaster assumes articulations and their components are
	 *  stable across a single play session. */
	TArray<FArticulationBroadcastRecord> CachedRecords;
	std::atomic<bool> bCacheBuilt { false };

	/** Game-thread-only: enumerate articulations + components into CachedRecords. */
	void BuildBroadcastCacheGameThread();
};
