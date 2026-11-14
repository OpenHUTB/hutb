// MIT License — OpenHUTB 水下整合层
//
// 用 UWorldSubsystem 承载 HoloOcean 的服务器与传感器宿主，
// 从而完全绕开 AHolodeckGameMode / UHolodeckGameInstance，
// 避免与 Carla 的 ACarlaGameModeBase / UCarlaGameInstance 抢槽位。
//
// 关键依据（见 docs/notes/HoloOcean集成方案.md）：
//   UHolodeckSensor::InitializeSensor() 只做一件事——
//     Controller->GetServer()->Malloc(key, size)
//   而 UHolodeckServer::Malloc 只分配共享内存，不检查 bIsRunning，
//   也不依赖 Start() 建立的信号量，因此无需启动服务器即可工作。

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"

#include "UnderwaterSensorSubsystem.generated.h"

class UHolodeckServer;
class UHolodeckSensor;
class AUnderwaterSensorHost;

/**
 * UUnderwaterSensorSubsystem
 * 在任意 World 中提供 HoloOcean 传感器所需的 Server 与 Host。
 */
UCLASS()
class HOLODECK_API UUnderwaterSensorSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	/**
	 * 初始化八叉树。必须在场景 Actor 加载完成后调用。
	 * UE4.26 的 UWorldSubsystem 没有 OnWorldBeginPlay 钩子，
	 * 因此由 AUnderwaterSensorHost::BeginPlay() 触发。幂等。
	 */
	void EnsureOctreeReady();

	/** 获取共享内存分配器 */
	UFUNCTION(BlueprintCallable, Category = "Underwater|Sensors")
	UHolodeckServer* GetServer() const { return Server; }

	/** 获取传感器宿主控制器 */
	UFUNCTION(BlueprintCallable, Category = "Underwater|Sensors")
	AUnderwaterSensorHost* GetHost() const { return Host; }

	/**
	 * 把一个 HoloOcean 传感器接入本子系统。
	 * 会依次调用 ParseSensorParms（若给了参数）、SetAgentAndController、InitializeSensor。
	 * 顺序不能颠倒——InitializeSensor 里会判 Controller 是否为空，
	 * 且它按 GetNumItems() 分配缓冲区，所以参数必须先于它生效。
	 *
	 * @param Sensor    传感器组件（ImagingSonar / SidescanSonar / DVL 等）
	 * @param AgentName 代理名，用于拼接共享内存的 key
	 * @param ParmsJson 传感器参数 JSON，留空则使用类默认值
	 */
	UFUNCTION(BlueprintCallable, Category = "Underwater|Sensors")
	bool AttachSensor(UHolodeckSensor* Sensor, const FString& AgentName, const FString& ParmsJson = TEXT(""));

	/** 是否已成功初始化（Server 与 Host 均就绪） */
	UFUNCTION(BlueprintCallable, Category = "Underwater|Sensors")
	bool IsReady() const { return Server != nullptr && Host != nullptr; }

	/**
	 * 自检：挂一个 LocationSensor 并读回数据，用于验证
	 * 「Server → Malloc → Controller → Sensor Tick → Buffer」整条通路。
	 * 由命令行开关 -UnderwaterSensorTest 触发，正常运行时不会执行。
	 */
	void RunSensorSelfTest();

	/**
	 * 八叉树作业区中心（UE 厘米单位）。
	 * 声呐的射线加速结构只覆盖这个区域，超出范围的物体不会被探测到。
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Underwater|Sensors")
	FVector EnvironmentCenter = FVector::ZeroVector;

	/**
	 * 八叉树作业区半尺寸（UE 厘米单位），默认 200m × 200m × 50m。
	 * 调大能覆盖更大范围，但八叉树内存与构建时间按体积增长，需谨慎。
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Underwater|Sensors")
	FVector EnvironmentExtent = FVector(20000.f, 20000.f, 5000.f);

private:
	UPROPERTY()
	UHolodeckServer* Server = nullptr;

	UPROPERTY()
	AUnderwaterSensorHost* Host = nullptr;

	bool bOctreeReady = false;
};
