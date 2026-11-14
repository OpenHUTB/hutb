// MIT License — OpenHUTB 水下整合层

#include "Holodeck.h"
#include "UnderwaterSensorSubsystem.h"

#include "UnderwaterSensorHost.h"
#include "HolodeckServer.h"
#include "HolodeckSensor.h"
#include "LocationSensor.h"
#include "ImagingSonar.h"
#include "Octree.h"

#include "Engine/World.h"
#include "Components/SceneComponent.h"
#include "TimerManager.h"

void UUnderwaterSensorSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	UWorld* World = GetWorld();
	if (World == nullptr)
	{
		UE_LOG(LogHolodeck, Warning, TEXT("UnderwaterSensorSubsystem: no world, skip init"));
		return;
	}

	// 只允许在游戏世界初始化，编辑器预览世界不启动
	if (World->WorldType != EWorldType::Game && World->WorldType != EWorldType::PIE)
	{
		UE_LOG(LogHolodeck, Log, TEXT("UnderwaterSensorSubsystem: world type %d, skip"), (int32)World->WorldType);
		return;
	}

	// 注意：这里只创建 Server 与 Host，不初始化八叉树。
	// 八叉树依赖场景中已加载的 Actor，必须推迟到 OnWorldBeginPlay。

	// 1. 创建共享内存分配器
	//    注意：刻意不调用 Server->Start()——那会打开命名信号量并进入与 Python 客户端的
	//    锁步同步。我们的数据在进程内消费（经 ZMQ 转发给 ROS2），不需要跨进程共享。
	Server = NewObject<UHolodeckServer>(this);

	// 2. 创建传感器宿主
	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	SpawnParams.Name = TEXT("UnderwaterSensorHost");
	Host = World->SpawnActor<AUnderwaterSensorHost>(
		AUnderwaterSensorHost::StaticClass(), FTransform::Identity, SpawnParams);

	if (Host != nullptr)
	{
		Host->SetServer(Server);
		UE_LOG(LogHolodeck, Log, TEXT("UnderwaterSensorSubsystem: initialized, server=%p host=%p"),
			Server, Host);
	}
	else
	{
		UE_LOG(LogHolodeck, Error, TEXT("UnderwaterSensorSubsystem: failed to spawn sensor host"));
	}
}

void UUnderwaterSensorSubsystem::EnsureOctreeReady()
{
	if (bOctreeReady)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (World == nullptr)
	{
		return;
	}

	// 到这里场景中的 Actor 已经加载完毕，八叉树才能扫到真实几何。
	// 之前在 Initialize 阶段调用会得到 ±1000 的默认包围盒（空场景），声呐射线全部落空。
	//
	// 另外：HoloOcean 原本只从命令行读环境范围（-EnvMinX= 等，单位米），
	// 缺省会退化成 ±10 米，对 Town10HD 这种公里级地图完全不够。
	//
	// 不能简单遍历 Actor 累加包围盒——天空球/大气等 Actor 的包围盒接近无限大
	// （实测得到 ±500 km），会让八叉树规模爆炸。
	// 因此改用可配置的局部作业区范围，默认以原点为中心 400m × 400m × 100m。
	const FVector Center = EnvironmentCenter;
	const FVector Extent = EnvironmentExtent;
	Octree::setEnvironmentBounds(Center - Extent, Center + Extent);

	Octree::initOctree(World);
	bOctreeReady = true;

	UE_LOG(LogHolodeck, Log, TEXT("UnderwaterSensorSubsystem: octree initialized for world '%s'"),
		*World->GetName());

	// 命令行开关触发的通路自检
	if (FParse::Param(FCommandLine::Get(), TEXT("UnderwaterSensorTest")))
	{
		RunSensorSelfTest();
	}
}

void UUnderwaterSensorSubsystem::RunSensorSelfTest()
{
	UWorld* World = GetWorld();
	if (World == nullptr)
	{
		return;
	}

	// 1. 承载传感器用的 Actor（带根组件，LocationSensor 需要 attach parent）
	//    刻意放在一个已知偏移处，这样读回的值可以反推坐标转换是否正确：
	//    UE 厘米 (1000, 2000, 200)
	//      → UEToClient: ÷100        → (10, 20, 2)
	//      → USE_RHS:    Y 取反      → (10, -20, 2)
	//    Z 取 +200cm（地面上方 2m），否则声呐埋在地下看不到建筑
	const FVector TestOffset(1000.f, 2000.f, 200.f);
	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AActor* TestActor = World->SpawnActor<AActor>(
		AActor::StaticClass(), EnvironmentCenter + TestOffset, FRotator::ZeroRotator, SpawnParams);
	if (TestActor == nullptr)
	{
		UE_LOG(LogHolodeck, Error, TEXT("SelfTest: failed to spawn test actor"));
		return;
	}

	USceneComponent* Root = NewObject<USceneComponent>(TestActor, TEXT("TestRoot"));
	TestActor->SetRootComponent(Root);
	Root->RegisterComponent();

	// 显式设置位置：AActor::StaticClass() 生成时没有根组件，SpawnActor 传入的位置可能不生效
	TestActor->SetActorLocation(EnvironmentCenter + TestOffset);
	UE_LOG(LogHolodeck, Log, TEXT("SelfTest: actor spawned at %s"), *TestActor->GetActorLocation().ToString());

	// 2. 挂 LocationSensor（3 个 float，无需额外参数，适合做通路冒烟测试）
	ULocationSensor* LocSensor = NewObject<ULocationSensor>(TestActor, TEXT("TestLocationSensor"));
	LocSensor->SetupAttachment(Root);
	LocSensor->RegisterComponent();

	// 3. 接入子系统（顺序不能反：先 SetAgentAndController 再 InitializeSensor）
	if (!AttachSensor(LocSensor, TEXT("selftest")))
	{
		UE_LOG(LogHolodeck, Error, TEXT("SelfTest: AttachSensor failed"));
		return;
	}

	UE_LOG(LogHolodeck, Log, TEXT("SelfTest: LocationSensor attached, buffer=%p size=%d"),
		LocSensor->GetDataBuffer(), LocSensor->GetDataBufferSize());

	// 4. 等两秒让传感器 tick 若干帧，再读缓冲区
	FTimerHandle Handle;
	TWeakObjectPtr<ULocationSensor> WeakSensor(LocSensor);
	World->GetTimerManager().SetTimer(Handle, [WeakSensor]()
	{
		ULocationSensor* Sensor = WeakSensor.Get();
		if (Sensor == nullptr)
		{
			UE_LOG(LogHolodeck, Warning, TEXT("SelfTest: sensor gone"));
			return;
		}

		const float* Data = static_cast<const float*>(Sensor->GetDataBuffer());
		if (Data == nullptr)
		{
			UE_LOG(LogHolodeck, Error, TEXT("SelfTest: buffer is null"));
			return;
		}

		UE_LOG(LogHolodeck, Warning,
			TEXT("SelfTest RESULT: Location = (%.2f, %.2f, %.2f)  [expected (10.00, -20.00, 2.00) for UE offset (1000, 2000, 200)]"),
			Data[0], Data[1], Data[2]);
	}, 2.0f, false);

	// ------------------------------------------------------------------
	// 5. 再挂一个 ImagingSonar，验证真实声呐通路
	//    参数单位：RangeMin/RangeMax 为米（内部 ×100 转厘米），角度为度
	// ------------------------------------------------------------------
	UImagingSonar* Sonar = NewObject<UImagingSonar>(TestActor, TEXT("TestImagingSonar"));
	Sonar->SetupAttachment(Root);
	Sonar->RegisterComponent();

	const FString SonarParms = TEXT(
		"{\"RangeMin\": 0.5, \"RangeMax\": 15, \"RangeBins\": 128, "
		"\"Azimuth\": 120, \"AzimuthBins\": 64, "
		"\"Elevation\": 20, \"ElevationBins\": 8}");

	if (!AttachSensor(Sonar, TEXT("selftest_sonar"), SonarParms))
	{
		UE_LOG(LogHolodeck, Error, TEXT("SelfTest: ImagingSonar attach failed"));
		return;
	}

	const int32 SonarBins = Sonar->GetDataBufferSize() / sizeof(float);
	UE_LOG(LogHolodeck, Log, TEXT("SelfTest: ImagingSonar attached, buffer=%p bins=%d"),
		Sonar->GetDataBuffer(), SonarBins);

	TWeakObjectPtr<UImagingSonar> WeakSonar(Sonar);
	FTimerHandle SonarHandle;
	World->GetTimerManager().SetTimer(SonarHandle, [WeakSonar]()
	{
		UImagingSonar* S = WeakSonar.Get();
		if (S == nullptr)
		{
			UE_LOG(LogHolodeck, Warning, TEXT("SelfTest SONAR: sensor gone"));
			return;
		}

		const float* Data = static_cast<const float*>(S->GetDataBuffer());
		const int32 Num = S->GetDataBufferSize() / sizeof(float);
		if (Data == nullptr || Num <= 0)
		{
			UE_LOG(LogHolodeck, Error, TEXT("SelfTest SONAR: buffer null or empty (num=%d)"), Num);
			return;
		}

		int32 NonZero = 0;
		float MinV = TNumericLimits<float>::Max();
		float MaxV = TNumericLimits<float>::Lowest();
		for (int32 i = 0; i < Num; ++i)
		{
			const float V = Data[i];
			if (V != 0.0f)
			{
				++NonZero;
				MinV = FMath::Min(MinV, V);
				MaxV = FMath::Max(MaxV, V);
			}
		}

		if (NonZero > 0)
		{
			UE_LOG(LogHolodeck, Warning,
				TEXT("SelfTest SONAR RESULT: bins=%d nonzero=%d min=%.3f max=%.3f  => 声呐有回波"),
				Num, NonZero, MinV, MaxV);
		}
		else
		{
			UE_LOG(LogHolodeck, Warning,
				TEXT("SelfTest SONAR RESULT: bins=%d nonzero=0  => 全零，范围内可能没有障碍物或八叉树为空"),
				Num);
		}
	}, 3.0f, false);
}

void UUnderwaterSensorSubsystem::Deinitialize()
{
	if (Server != nullptr)
	{
		// 说明：Kill() 在 bIsRunning == false 时会直接 return，因此不会清空内部
		// 的 Memory 映射。这在本方案下是安全的——Server 是 UPROPERTY 持有的
		// UObject，被 GC 回收时其 Memory 成员（unique_ptr 容器）会正常析构，
		// 命名共享内存随之释放。这里调用 Kill() 只是为了让日志与状态显式收尾。
		Server->Kill();
		Server = nullptr;
	}

	Host = nullptr;

	UE_LOG(LogHolodeck, Log, TEXT("UnderwaterSensorSubsystem deinitialized"));
	Super::Deinitialize();
}

bool UUnderwaterSensorSubsystem::AttachSensor(UHolodeckSensor* Sensor, const FString& AgentName, const FString& ParmsJson)
{
	if (Sensor == nullptr || !IsReady())
	{
		UE_LOG(LogHolodeck, Warning, TEXT("AttachSensor: sensor or subsystem not ready"));
		return false;
	}

	// 参数必须先于 InitializeSensor：后者按 GetNumItems() 分配缓冲区
	if (!ParmsJson.IsEmpty())
	{
		Sensor->ParseSensorParms(ParmsJson);
	}

	Sensor->SetAgentAndController(Host, AgentName);
	Sensor->InitializeSensor();

	UE_LOG(LogHolodeck, Log, TEXT("AttachSensor: '%s' attached to agent '%s'"),
		*Sensor->GetName(), *AgentName);
	return true;
}
