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
#include "MuJoCo/Net/MjZmqComponent.h"
#include "ZmqControlSubscriber.generated.h"

class UMjActuator;

UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class URLAB_API UZmqControlSubscriber : public UMjZmqComponent
{
	GENERATED_BODY()

public:	
	UZmqControlSubscriber();

	// ZMQ Settings
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ZMQ")
	FString ControlEndpoint = "tcp://*:5556"; // SUB - Inputs

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ZMQ")
	FString InfoEndpoint = "tcp://*:5557"; // PUB - Actuator Info

	// Overrides
	virtual void InitZmq() override;
	virtual void ShutdownZmq() override;
	virtual void PreStep(mjModel* m, mjData* d) override;

private:
	void* ZmqContext = nullptr;
	void* ControlSubscriber = nullptr; // SUB
	void* InfoPublisher = nullptr;     // PUB
	
	int InfoBroadcastCounter = 0;
	int TotalStepCount = 0;
	TMap<FString, int> ActuatorCache;
	TMap<int32, UMjActuator*> ActuatorComponentCache;
	bool bCacheBuilt = false;

	int32 ControlLogCounter = 0;

	void BuildCache(mjModel* m);
	void BroadcastInfo(mjModel* m);
};
