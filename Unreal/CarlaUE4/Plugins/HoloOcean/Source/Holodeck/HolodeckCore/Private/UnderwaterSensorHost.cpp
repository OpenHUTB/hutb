// MIT License — OpenHUTB 水下整合层

#include "Holodeck.h"
#include "UnderwaterSensorHost.h"
#include "UnderwaterSensorSubsystem.h"

#include "Engine/World.h"

AUnderwaterSensorHost::AUnderwaterSensorHost(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// 不接管 pawn，也不需要 tick
	PrimaryActorTick.bCanEverTick = false;
	UE_LOG(LogHolodeck, Log, TEXT("UnderwaterSensorHost created"));
}

void AUnderwaterSensorHost::BeginPlay()
{
	Super::BeginPlay();

	// 场景已加载完毕，让子系统初始化八叉树（声呐射线依赖它）
	if (UWorld* World = GetWorld())
	{
		if (UUnderwaterSensorSubsystem* Subsystem = World->GetSubsystem<UUnderwaterSensorSubsystem>())
		{
			Subsystem->EnsureOctreeReady();
		}
	}
}
