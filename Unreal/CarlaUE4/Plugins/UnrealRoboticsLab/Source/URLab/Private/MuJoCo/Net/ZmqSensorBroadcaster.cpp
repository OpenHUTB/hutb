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

#include "MuJoCo/Net/ZmqSensorBroadcaster.h"
#include "zmq.h"
#include "MuJoCo/Core/MjArticulation.h"
#include "MuJoCo/Core/AMjManager.h"
#include "MuJoCo/Components/MjComponent.h"
#include "MuJoCo/Input/MjTwistController.h"
#include "Serialization/BufferArchive.h"
#include "Utils/URLabLogging.h"

UZmqSensorBroadcaster::UZmqSensorBroadcaster()
{
	// We need a game-thread tick to build the broadcast cache lazily after all
	// articulations have finished BeginPlay (their component lists need to be
	// stable before we snapshot them).
	PrimaryComponentTick.bCanEverTick = true;
}

void UZmqSensorBroadcaster::BeginPlay()
{
	Super::BeginPlay();
	// ZMQ Init is deferred to the async thread. The broadcast cache is built
	// on the first TickComponent — can't build it here because sibling
	// articulations may not have run BeginPlay yet (actor order is undefined).
}

void UZmqSensorBroadcaster::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// Make sure the physics thread stops reading our cache before we let the
	// manager tear down the articulations.
	bCacheBuilt.store(false, std::memory_order_release);
	CachedRecords.Reset();
	Super::EndPlay(EndPlayReason);
	// ZMQ cleanup happens via ShutdownZmq called by manager,
	// or in destructor if needed, but manager thread is safest.
}

void UZmqSensorBroadcaster::TickComponent(float DeltaTime, ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (!bCacheBuilt.load(std::memory_order_acquire))
	{
		BuildBroadcastCacheGameThread();
	}
}

void UZmqSensorBroadcaster::BuildBroadcastCacheGameThread()
{
	AAMjManager* Manager = Cast<AAMjManager>(GetOwner());
	if (!Manager) return;

	TArray<AMjArticulation*> Articulations = Manager->GetAllArticulations();
	if (Articulations.Num() == 0) return;  // wait until at least one is live

	CachedRecords.Reset(Articulations.Num());
	for (AMjArticulation* Art : Articulations)
	{
		if (!Art) continue;

		FArticulationBroadcastRecord Rec;
		Rec.Articulation = Art;
		Rec.ArticPrefix  = Art->GetName();
		Art->GetComponents<UMjComponent>(Rec.TelemetryComponents);
		Rec.TwistCtrl    = Art->FindComponentByClass<UMjTwistController>();
		CachedRecords.Add(MoveTemp(Rec));
	}

	// Release-store publishes the fully-populated array to the physics thread.
	bCacheBuilt.store(true, std::memory_order_release);

	UE_LOG(LogURLabNet, Log,
		TEXT("ZmqSensorBroadcaster: built broadcast cache (%d articulations)."),
		CachedRecords.Num());
}

void UZmqSensorBroadcaster::InitZmq()
{
	if (bIsInitialized) return;

	ZmqContext = zmq_ctx_new();
	ZmqPublisher = zmq_socket(ZmqContext, ZMQ_PUB);
	
	int rc = zmq_bind(ZmqPublisher, TCHAR_TO_UTF8(*ZmqEndpoint));
	if (rc == 0)
	{
		UE_LOG(LogURLabNet, Log, TEXT("Initalized ZMQ at %s"), *ZmqEndpoint);
		bIsInitialized = true;
	}
	else
	{
		UE_LOG(LogURLabNet, Error, TEXT("Failed to bind ZMQ at %s"), *ZmqEndpoint);
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 10.f, FColor::Red,
			    FString::Printf(TEXT("URLab: ZMQ bind failed on %s — check for port conflicts"), *ZmqEndpoint));
		}
	}
}

void UZmqSensorBroadcaster::ShutdownZmq()
{
	if (!bIsInitialized) return;

	zmq_close(ZmqPublisher);
	zmq_ctx_term(ZmqContext);
	
	ZmqPublisher = nullptr;
	ZmqContext = nullptr;
	bIsInitialized = false;
	
	UE_LOG(LogURLabNet, Log, TEXT("ZMQ Shutdown"));
}

void UZmqSensorBroadcaster::PostStep(mjModel* m, mjData* d)
{
	static constexpr int32 kLogInterval = 500;
	bool bShouldLog = (FrameCounter++ % kLogInterval == 0); // Log once roughly every second (assuming 500hz)

	if (!bIsInitialized)
	{
		InitZmq(); // Try lazy init
		if (!bIsInitialized) return;
	}

	// Acquire-load: if the game thread hasn't published the cache yet, skip
	// this step. We DO NOT touch AActor::OwnedComponents from this thread.
	if (!bCacheBuilt.load(std::memory_order_acquire))
	{
		return;
	}

	int BroadcastCount = 0;

	if (bShouldLog)
	{
		UE_LOG(LogURLabNet, Verbose, TEXT("ZmqSensorBroadcaster PostStep: broadcasting %d cached articulations"),
			CachedRecords.Num());
	}

	for (const FArticulationBroadcastRecord& Rec : CachedRecords)
	{
		if (!Rec.Articulation) continue;

		for (UMjComponent* Comp : Rec.TelemetryComponents)
		{
			if (!Comp || Comp->bIsDefault) continue;

			FString TopicSuffix = Comp->GetTelemetryTopicName();
			if (TopicSuffix.IsEmpty()) continue;

			FString FullTopic = FString::Printf(TEXT("%s/%s"), *Rec.ArticPrefix, *TopicSuffix);

			FBufferArchive Payload;
			Comp->BuildBinaryPayload(Payload);

			if (Payload.Num() > 0)
			{
				zmq_send(ZmqPublisher, TCHAR_TO_UTF8(*FullTopic), FullTopic.Len(), ZMQ_SNDMORE);
				zmq_send(ZmqPublisher, Payload.GetData(), Payload.Num(), 0);
				BroadcastCount++;
			}
		}

		if (Rec.TwistCtrl)
		{
			FVector Twist = Rec.TwistCtrl->GetTwist();
			FString TwistTopic = FString::Printf(TEXT("%s/twist"), *Rec.ArticPrefix);
			float TwistData[3] = { (float)Twist.X, (float)Twist.Y, (float)Twist.Z };
			zmq_send(ZmqPublisher, TCHAR_TO_UTF8(*TwistTopic), TwistTopic.Len(), ZMQ_SNDMORE);
			zmq_send(ZmqPublisher, TwistData, sizeof(TwistData), 0);
			BroadcastCount++;

			int32 Actions = Rec.TwistCtrl->GetActiveActions();
			if (Actions != 0)
			{
				FString ActionTopic = FString::Printf(TEXT("%s/actions"), *Rec.ArticPrefix);
				zmq_send(ZmqPublisher, TCHAR_TO_UTF8(*ActionTopic), ActionTopic.Len(), ZMQ_SNDMORE);
				zmq_send(ZmqPublisher, &Actions, sizeof(Actions), 0);
				BroadcastCount++;
			}
		}
	}

	if (bShouldLog && BroadcastCount == 0)
	{
		UE_LOG(LogURLabNet, Warning, TEXT("ZmqSensorBroadcaster: Found components but NONE produced a valid binary payload!"));
	}
	else if (bShouldLog)
	{
		UE_LOG(LogURLabNet, Verbose, TEXT("ZmqSensorBroadcaster: successfully broadcast %d messages to ZMQ."), BroadcastCount);
	}
}
