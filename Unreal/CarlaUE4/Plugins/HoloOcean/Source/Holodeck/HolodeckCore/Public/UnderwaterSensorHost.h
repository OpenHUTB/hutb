// MIT License — OpenHUTB 水下整合层
//
// 最小化控制器：唯一职责是持有一个 UHolodeckServer，
// 让 HoloOcean 传感器能通过 Controller->GetServer()->Malloc(...) 拿到缓冲区。
//
// 刻意不实现任何控制方案（AllocateBuffers / AddControlSchemes 等全部空实现），
// 因为 ROV 的运动由 URLab/MuJoCo 驱动，这里若也驱动 pawn 会互相抢控制权。
//
// 对应方案文档：underwater/docs/notes/HoloOcean集成方案.md

#pragma once

#include "CoreMinimal.h"
#include "HolodeckPawnControllerInterface.h"

#include "UnderwaterSensorHost.generated.h"

/**
 * AUnderwaterSensorHost
 * 只提供 Server 的空控制器，用于挂载 HoloOcean 传感器。
 */
UCLASS()
class HOLODECK_API AUnderwaterSensorHost : public AHolodeckPawnControllerInterface
{
	GENERATED_BODY()

public:
	AUnderwaterSensorHost(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/**
	 * BeginPlay 在本 Actor 生成且场景 Actor 均已加载后触发，
	 * 是初始化八叉树的正确时机（UE4.26 的 UWorldSubsystem 没有对应钩子）。
	 */
	virtual void BeginPlay() override;

	// --- 只实现这两个 ---
	virtual void SetServer(UHolodeckServer* const ServerParam) override { Server = ServerParam; }
	virtual UHolodeckServer* GetServer() override { return Server; }

	// --- 控制相关全部空实现：本控制器不驱动任何 pawn ---
	virtual void AllocateBuffers(const FString& AgentName) override {}
	virtual void AddControlSchemes() override {}
	virtual void ExecuteTeleport() override {}
	virtual void ExecuteSetState() override {}

private:
	UPROPERTY()
	UHolodeckServer* Server = nullptr;
};
