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

// --- Forwarding shims: PreCompile, PostCompile, Compile, ApplyOptions ---
// Actual implementations live in UMjPhysicsEngine.

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

    // Sync discovery lists from PhysicsEngine
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

    // Auto-create ZMQ components if none exist on this actor
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

    // Auto-create ReplayManager if none exists in the scene
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

    // Compile via PhysicsEngine (also discovers ZMQ components in PreCompile)
    Compile();
    if (NetworkManager) NetworkManager->UpdateCameraStreamingState();

    // Register ZMQ callbacks on PhysicsEngine
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
        // Register debug data capture as a post-step callback. Fires whenever any
        // debug overlay needs fresh mjData — contact forces (key 1), body shader
        // overlays (Island / Segmentation modes), or tendon/muscle rendering.
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

    // Auto-create simulate widget AFTER Compile so articulations are registered
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

    // Signal the async thread to stop and wait for it to exit — but bound the
    // wait. If mj_step is mid-call on a pathological flex state, the step can
    // take many seconds (or effectively hang). An unbounded Wait() here would
    // freeze the editor's PIE-stop; the user sees UE never coming back. With
    // the timeout we instead detach: the async thread keeps running in the
    // background until its current mj_step returns, then finds bShouldStopTask
    // set and exits cleanly on its own. Cost is a one-time memory leak (we
    // can't delete m_model / m_data while the thread may still read them).
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

    // Clear tracked actors to prevent dangling pointers on level restart
    m_heightfieldActors.Empty();
    m_articulations.Empty();
    m_MujocoComponents.Empty();

    // Cleanup ZMQ Components
    if (NetworkManager)
    {
        for (UMjZmqComponent* Comp : NetworkManager->ZmqComponents)
        {
            if (Comp) Comp->ShutdownZmq();
        }
    }

    // Only touch MuJoCo resources if the async thread actually exited — a
    // detached thread may still be executing mj_step and reading these.
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
    // Hotkey processing handled by UMjInputHandler::TickComponent
    // Debug drawing handled by UMjDebugVisualizer::TickComponent
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

    // Re-sync discovery lists after recompile
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

// --- Replay Testing ---

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
    // NOTE: OnPostStep callback is kept alive — the replay manager gates recording with bIsRecording.
    // The callback must remain set so recording can be restarted without re-registering.
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