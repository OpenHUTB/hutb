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


#include "MuJoCo/Core/AMjManager.h"
#include "MuJoCo/Core/MjPhysicsEngine.h"
#include "MuJoCo/Core/MjDebugVisualizer.h"
#include "MuJoCo/Net/MjNetworkManager.h"
#include "MuJoCo/Input/MjInputHandler.h"
#include "MuJoCo/Input/MjPerturbation.h"
#include "Replay/MjReplayManager.h"
#include "mujoco/mujoco.h"

#include "Kismet/GameplayStatics.h"
#include "Blueprint/UserWidget.h"
#include "MuJoCo/Net/MjZmqComponent.h"
#include "MuJoCo/Net/ZmqSensorBroadcaster.h"
#include "MuJoCo/Net/ZmqControlSubscriber.h"
#include "MuJoCo/Core/MjSimulationState.h"
#include "Utils/URLabLogging.h"
#if WITH_EDITOR
#include "Misc/MessageDialog.h"
#endif

AAMjManager* AAMjManager::Instance = nullptr;

AAMjManager::AAMjManager() {
    PrimaryActorTick.bCanEverTick = true;

    PhysicsEngine = CreateDefaultSubobject<UMjPhysicsEngine>(TEXT("PhysicsEngine"));
    DebugVisualizer = CreateDefaultSubobject<UMjDebugVisualizer>(TEXT("DebugVisualizer"));
    NetworkManager = CreateDefaultSubobject<UMjNetworkManager>(TEXT("NetworkManager"));
    InputHandler = CreateDefaultSubobject<UMjInputHandler>(TEXT("InputHandler"));
    Perturbation = CreateDefaultSubobject<UMjPerturbation>(TEXT("Perturbation"));
}

// --- 转发 shims: PreCompile, PostCompile, Compile, ApplyOptions ---
// 实际的实现存在于 UMjPhysicsEngine 中。

void AAMjManager::PreCompile()
{
    if (PhysicsEngine) PhysicsEngine->PreCompile();
    if (NetworkManager) NetworkManager->DiscoverZmqComponents();
}

void AAMjManager::PostCompile()
{
    if (PhysicsEngine) PhysicsEngine->PostCompile();
}

void AAMjManager::Compile()
{
    if (!PhysicsEngine) return;

    if (NetworkManager) NetworkManager->DiscoverZmqComponents();

    PhysicsEngine->Compile();

    // 从物理引擎同步发现列表
    m_MujocoComponents = PhysicsEngine->m_MujocoComponents;
    m_articulations = PhysicsEngine->m_articulations;
    m_heightfieldActors = PhysicsEngine->m_heightfieldActors;
    m_ArticulationMap = PhysicsEngine->m_ArticulationMap;
}



void AAMjManager::BeginPlay() {
    Super::BeginPlay();

    if (Instance != nullptr && Instance != this)
    {
        UE_LOG(LogURLab, Error, TEXT("[AAMjManager] Multiple AAMjManager actors detected in level. Only one is supported — this instance (%s) will be ignored."), *GetName());
        return;
    }
    Instance = this;

    // 如果该参与者没有零代价消息队列（ZMQ）组件，就自动创建
    {
        TArray<UActorComponent*> ExistingZmq;
        GetComponents(UMjZmqComponent::StaticClass(), ExistingZmq);
        if (ExistingZmq.Num() == 0)
        {
            UE_LOG(LogURLab, Log, TEXT("[AAMjManager] No ZMQ components found — auto-creating SensorBroadcaster and ControlSubscriber"));

            UZmqSensorBroadcaster* Broadcaster = NewObject<UZmqSensorBroadcaster>(this, TEXT("AutoZmqBroadcaster"));
            if (Broadcaster)
            {
                Broadcaster->RegisterComponent();
                UE_LOG(LogURLab, Log, TEXT("[AAMjManager] Created UZmqSensorBroadcaster (tcp://*:5555)"));
            }

            UZmqControlSubscriber* Subscriber = NewObject<UZmqControlSubscriber>(this, TEXT("AutoZmqSubscriber"));
            if (Subscriber)
            {
                Subscriber->RegisterComponent();
                UE_LOG(LogURLab, Log, TEXT("[AAMjManager] Created UZmqControlSubscriber (tcp://127.0.0.1:5556)"));
            }
        }
    }

    // 如果场景中没有回放管理器（ReplayManager），则自动创建
    {
        AMjReplayManager* ExistingReplay = Cast<AMjReplayManager>(
            UGameplayStatics::GetActorOfClass(GetWorld(), AMjReplayManager::StaticClass()));
        if (!ExistingReplay)
        {
            FActorSpawnParameters SpawnParams;
            AMjReplayManager* ReplayMgr = GetWorld()->SpawnActor<AMjReplayManager>(SpawnParams);
            if (ReplayMgr)
            {
                UE_LOG(LogURLab, Log, TEXT("[AAMjManager] Auto-created AMjReplayManager"));
            }
        }
    }

    // 通过物理引擎编译（还会在预编译阶段发现 ZMQ 组件）
    Compile();
    if (NetworkManager) NetworkManager->UpdateCameraStreamingState();

    // 在物理引擎上注册 ZMQ 回调
    if (PhysicsEngine && NetworkManager)
    {
        UE_LOG(LogURLab, Log, TEXT("[AAMjManager] Registering %d ZMQ callbacks on PhysicsEngine"), NetworkManager->ZmqComponents.Num());
        for (UMjZmqComponent* ZmqComp : NetworkManager->ZmqComponents)
        {
            if (ZmqComp)
            {
                PhysicsEngine->RegisterPreStepCallback([ZmqComp](mjModel* m, mjData* d) {
                    ZmqComp->PreStep(m, d);
                });
                PhysicsEngine->RegisterPostStepCallback([ZmqComp](mjModel* m, mjData* d) {
                    ZmqComp->PostStep(m, d);
                });
            }
        }
    }

    if (PhysicsEngine)
    {
        // 将调试数据捕获注册为后置步骤回调。
        // 每当任何调试覆盖需要新的 mjData 时都会触发 — 接触力（按键 1）、身体着色器覆盖（岛屿/分割模式）或肌腱/肌肉渲染。
        PhysicsEngine->RegisterPostStepCallback([this](mjModel* m, mjData* d) {
            if (!DebugVisualizer) return;
            const bool bNeedsCapture =
                DebugVisualizer->bShowDebug ||
                DebugVisualizer->DebugShaderMode != EMjDebugShaderMode::Off ||
                DebugVisualizer->bGlobalDrawTendons;
            if (bNeedsCapture)
            {
                DebugVisualizer->CaptureDebugData();
            }
        });

        PhysicsEngine->RunMujocoAsync();
    }

    // 在编译后自动创建模拟小部件，以便注册关节动作
    if (bAutoCreateSimulateWidget && !SimulateWidget)
    {
        static const TCHAR* WidgetBPPath = TEXT("/UnrealRoboticsLab/UI/WBP_MjSimulate.WBP_MjSimulate_C");
        UClass* WidgetClass = LoadClass<UUserWidget>(nullptr, WidgetBPPath);
        if (WidgetClass)
        {
            APlayerController* PC = GetWorld()->GetFirstPlayerController();
            if (PC)
            {
                UUserWidget* Widget = CreateWidget<UUserWidget>(PC, WidgetClass);
                if (Widget)
                {
                    Widget->AddToViewport(0);
                    SimulateWidget = Widget;
                    UE_LOG(LogURLab, Log, TEXT("[AAMjManager] Auto-created MjSimulate widget (press Tab to show)"));
                }
            }
        }
        else
        {
            UE_LOG(LogURLab, Warning, TEXT("[AAMjManager] Could not load WBP_MjSimulate Blueprint class. Widget not created."));
        }
    }
}

void AAMjManager::ToggleSimulateWidget()
{
    if (SimulateWidget)
    {
        bool bIsVisible = SimulateWidget->IsVisible();
        SimulateWidget->SetVisibility(bIsVisible ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
    }
}

void AAMjManager::EndPlay(const EEndPlayReason::Type EndPlayReason) {
    Super::EndPlay(EndPlayReason);
    if (Instance == this) Instance = nullptr;

    // 向异步线程发出停止信号并等待它退出——但要限制等待时间。
    // 如果 mj_step 在一个病态的 flex 状态中调用中途，步骤可能需要好几秒（或者实际上会挂掉）。
    // 这里如果使用无限制的 Wait() 会冻结编辑器的 PIE 停止；用户会看到 UE 永远回不来。
    // 有了超时，我们就会改为分离线程：异步线程会在后台继续运行，直到当前的 mj_step 返回，然后发现 bShouldStopTask 被设置了，自行干净地退出。
    // 代价是一次性的内存泄漏（在线程可能仍会读取它们的时候，我们不能删除 m_model / m_data）。 
    bool bAsyncExited = true;
    if (PhysicsEngine)
    {
        PhysicsEngine->bShouldStopTask = true;
        if (PhysicsEngine->AsyncPhysicsFuture.IsValid())
        {
            constexpr double kShutdownTimeoutSec = 3.0;
            bAsyncExited = PhysicsEngine->AsyncPhysicsFuture.WaitFor(
                FTimespan::FromSeconds(kShutdownTimeoutSec));
            if (!bAsyncExited)
            {
                UE_LOG(LogURLab, Warning,
                    TEXT("Physics async thread did not exit within %.1fs — detaching. ")
                    TEXT("mj_step is likely stuck; MuJoCo resources will leak for this session ")
                    TEXT("to avoid a use-after-free in the still-running step."),
                    kShutdownTimeoutSec);
            }
        }
        PhysicsEngine->ClearCallbacks();
    }

    // 清除已追踪的参与者，以防关卡重启时出现悬挂指针
    m_heightfieldActors.Empty();
    m_articulations.Empty();
    m_MujocoComponents.Empty();

    // 清理 ZMQ 组件
    if (NetworkManager)
    {
        for (UMjZmqComponent* Comp : NetworkManager->ZmqComponents)
        {
            if (Comp) Comp->ShutdownZmq();
        }
    }

    // 只有在异步线程实际上已经退出的情况下才接触 MuJoCo 资源：
    // 一个分离的线程可能仍在执行 mj_step 并读取这些资源。
    if (PhysicsEngine && bAsyncExited)
    {
        if (PhysicsEngine->m_data)
        {
            mj_deleteData(PhysicsEngine->m_data);
            PhysicsEngine->m_data = nullptr;
        }
        if (PhysicsEngine->m_model)
        {
            mj_deleteModel(PhysicsEngine->m_model);
            PhysicsEngine->m_model = nullptr;
        }
        if (PhysicsEngine->m_spec)
        {
            mj_deleteVFS(&PhysicsEngine->m_vfs);
            mj_deleteSpec(PhysicsEngine->m_spec);
            PhysicsEngine->m_spec = nullptr;
        }

        PhysicsEngine->m_heightfieldActors.Empty();
        PhysicsEngine->m_articulations.Empty();
        PhysicsEngine->m_MujocoComponents.Empty();
    }
}

void AAMjManager::Tick(float DeltaTime) {
    Super::Tick(DeltaTime);
    // 热键处理由 UMjInputHandler::TickComponent 负责
    // 调试绘制由 UMjDebugVisualizer::TickComponent 负责
}

AAMjManager* AAMjManager::GetManager()
{
    return Instance;
}

void AAMjManager::SetPaused(bool bPaused)
{
    if (PhysicsEngine) PhysicsEngine->SetPaused(bPaused);
}

bool AAMjManager::IsRunning() const
{
    return PhysicsEngine ? PhysicsEngine->IsRunning() : false;
}

bool AAMjManager::IsInitialized() const
{
    return PhysicsEngine ? PhysicsEngine->IsInitialized() : false;
}

FString AAMjManager::GetLastCompileError() const
{
    return PhysicsEngine ? PhysicsEngine->GetLastCompileError() : FString();
}

void AAMjManager::StepSync(int32 NumSteps)
{
    if (PhysicsEngine) PhysicsEngine->StepSync(NumSteps);
}

bool AAMjManager::CompileModel()
{
    if (!PhysicsEngine) return false;

    bool Result = PhysicsEngine->CompileModel();

    // 重新编译后重新同步发现列表
    m_MujocoComponents = PhysicsEngine->m_MujocoComponents;
    m_articulations = PhysicsEngine->m_articulations;
    m_heightfieldActors = PhysicsEngine->m_heightfieldActors;
    m_ArticulationMap = PhysicsEngine->m_ArticulationMap;

    return Result;
}

AMjArticulation* AAMjManager::GetArticulation(const FString& ActorName) const
{
    return PhysicsEngine ? PhysicsEngine->GetArticulation(ActorName) : nullptr;
}

TArray<AMjArticulation*> AAMjManager::GetAllArticulations() const
{
    return PhysicsEngine ? PhysicsEngine->GetAllArticulations() : m_articulations;
}

TArray<UMjQuickConvertComponent*> AAMjManager::GetAllQuickComponents() const
{
    return PhysicsEngine ? PhysicsEngine->GetAllQuickComponents() : m_MujocoComponents;
}

TArray<AMjHeightfieldActor*> AAMjManager::GetAllHeightfields() const
{
    return PhysicsEngine ? PhysicsEngine->GetAllHeightfields() : m_heightfieldActors;
}

float AAMjManager::GetSimTime() const
{
    return PhysicsEngine ? PhysicsEngine->GetSimTime() : 0.0f;
}

float AAMjManager::GetTimestep() const
{
    return PhysicsEngine ? PhysicsEngine->GetTimestep() : 0.002f;
}

// --- 重放测试 ---

void AAMjManager::StartRecording()
{
    AMjReplayManager* ReplayMgr = Cast<AMjReplayManager>(UGameplayStatics::GetActorOfClass(GetWorld(), AMjReplayManager::StaticClass()));
    if (ReplayMgr)
    {
        ReplayMgr->StartRecording();
        UE_LOG(LogURLab, Log, TEXT("Test: Called StartRecording on ReplayManager."));
    }
    else
    {
        UE_LOG(LogURLab, Warning, TEXT("Test: ReplayManager not found in scene!"));
    }
}

void AAMjManager::StopRecording()
{
    AMjReplayManager* ReplayMgr = Cast<AMjReplayManager>(UGameplayStatics::GetActorOfClass(GetWorld(), AMjReplayManager::StaticClass()));
    if (ReplayMgr)
    {
        ReplayMgr->StopRecording();
        UE_LOG(LogURLab, Log, TEXT("Test: Called StopRecording on ReplayManager."));
    }
    // 注意：OnPostStep 回调会一直保持活跃——回放管理器通过 bIsRecording 控制录制。
    // 回调必须保持设置状态，这样录制才能在不重新注册的情况下重新启动。
}

void AAMjManager::StartReplay()
{
    AMjReplayManager* ReplayMgr = Cast<AMjReplayManager>(UGameplayStatics::GetActorOfClass(GetWorld(), AMjReplayManager::StaticClass()));
    if (ReplayMgr)
    {
        ReplayMgr->StartReplay();
        UE_LOG(LogURLab, Log, TEXT("Test: Called StartReplay on ReplayManager."));
    }
}

void AAMjManager::StopReplay()
{
    AMjReplayManager* ReplayMgr = Cast<AMjReplayManager>(UGameplayStatics::GetActorOfClass(GetWorld(), AMjReplayManager::StaticClass()));
    if (ReplayMgr)
    {
        ReplayMgr->StopReplay();
        UE_LOG(LogURLab, Log, TEXT("Test: Called StopReplay on ReplayManager."));
    }
}

void AAMjManager::ResetSimulation()
{
    if (PhysicsEngine)
    {
        PhysicsEngine->ResetSimulation();
    }
    UE_LOG(LogURLab, Log, TEXT("MuJoCo Manager: Reset requested."));
}

UMjSimulationState* AAMjManager::CaptureSnapshot()
{
    return PhysicsEngine ? PhysicsEngine->CaptureSnapshot() : nullptr;
}

void AAMjManager::RestoreSnapshot(UMjSimulationState* Snapshot)
{
    if (PhysicsEngine) PhysicsEngine->RestoreSnapshot(Snapshot);
}