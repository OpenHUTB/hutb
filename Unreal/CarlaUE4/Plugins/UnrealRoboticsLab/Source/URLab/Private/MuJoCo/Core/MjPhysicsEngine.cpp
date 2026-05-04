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

#include "MuJoCo/Core/MjPhysicsEngine.h"
#include "MuJoCo/Core/MjArticulation.h"
#include "MuJoCo/Components/QuickConvert/MjQuickConvertComponent.h"
#include "MuJoCo/Components/QuickConvert/AMjHeightfieldActor.h"
#include "MuJoCo/Core/MjSimulationState.h"
#include "MuJoCo/Core/AMjManager.h"
#include "Kismet/GameplayStatics.h"
#include "HAL/FileManager.h"
#include "Async/Future.h"
#include "Misc/Paths.h"
#include "XmlFile.h"
#include "Internationalization/Regex.h"
#include "Utils/URLabLogging.h"
#include <atomic>
#if WITH_EDITOR
#include "Misc/MessageDialog.h"
#endif

// Installed as mju_user_error / mju_user_warning so MuJoCo's fatal-error
// path logs via UE_LOG instead of calling exit(1). Without this, any MuJoCo
// internal invariant violation (e.g. "mj_sleep: found sleeping tree N in
// island M" on flex × free-body + SLEEP + MULTICCD) terminates the process:
// exit() unwinds every live FRHIBreadcrumbEventManual's TOptional across
// threads and trips the !Node assertion, killing the editor. Messages are
// deduplicated + throttled so pathological per-step errors can't flood the
// log at step rate.
static FCriticalSection GMujocoLogMutex;
static TMap<FString, int64> GMujocoMsgHistory;  // message text -> next step count at which to log
static std::atomic<int64>   GMujocoMsgStepCounter{0};

static void URLab_LogMujocoMessage(const TCHAR* Severity, const char* Msg, ELogVerbosity::Type Verbosity)
{
    const FString Text = Msg ? FString(UTF8_TO_TCHAR(Msg)) : FString(TEXT("(null)"));
    const int64 Step = GMujocoMsgStepCounter.fetch_add(1, std::memory_order_relaxed);

    int64 FirstHitStep = -1;
    int64 HitCountSinceLastLog = 0;
    {
        FScopeLock Lock(&GMujocoLogMutex);
        int64* NextLog = GMujocoMsgHistory.Find(Text);
        if (!NextLog)
        {
            // First occurrence — log it and start counting future hits.
            GMujocoMsgHistory.Add(Text, Step + 500);  // log again after 500 more messages
            FirstHitStep = Step;
        }
        else if (Step >= *NextLog)
        {
            HitCountSinceLastLog = 500;  // approx — we don't track exact
            *NextLog = Step + 500;
            FirstHitStep = Step;
        }
    }

    if (FirstHitStep >= 0)
    {
        if (HitCountSinceLastLog > 0)
        {
            UE_LOG(LogURLab, Warning, TEXT("[MuJoCo %s x~%lld] %s"), Severity, (long long)HitCountSinceLastLog, *Text);
        }
        else
        {
            if (Verbosity == ELogVerbosity::Error)
            {
                UE_LOG(LogURLab, Error, TEXT("[MuJoCo %s] %s"), Severity, *Text);
            }
            else
            {
                UE_LOG(LogURLab, Warning, TEXT("[MuJoCo %s] %s"), Severity, *Text);
            }
        }
    }
}

static void URLab_OnMujocoError(const char* Msg)
{
    URLab_LogMujocoMessage(TEXT("fatal"), Msg, ELogVerbosity::Error);
}

static void URLab_OnMujocoWarning(const char* Msg)
{
    URLab_LogMujocoMessage(TEXT("warn"), Msg, ELogVerbosity::Warning);
}

static bool GMujocoCallbacksInstalled = false;
static void URLab_InstallMujocoCallbacks()
{
    if (GMujocoCallbacksInstalled) return;

    // mujoco.dll is delay-loaded in URLab's Build.cs, and the linker refuses
    // to bind data symbols through a delayed import. Resolve the two
    // mju_user_* function pointers manually via GetDllExport.
    void* Handle = FPlatformProcess::GetDllHandle(TEXT("mujoco.dll"));
    if (!Handle)
    {
        UE_LOG(LogURLab, Warning, TEXT("[URLab] Could not resolve mujoco.dll to install error callbacks"));
        return;
    }

    using ErrorFnPtr = void(*)(const char*);
    // GetDllExport(hMod, "mju_user_error") returns the address of the
    // exported variable itself — i.e. an ErrorFnPtr*.
    ErrorFnPtr* PErr  = reinterpret_cast<ErrorFnPtr*>(FPlatformProcess::GetDllExport(Handle, TEXT("mju_user_error")));
    ErrorFnPtr* PWarn = reinterpret_cast<ErrorFnPtr*>(FPlatformProcess::GetDllExport(Handle, TEXT("mju_user_warning")));
    if (PErr)  { *PErr  = &URLab_OnMujocoError;   }
    if (PWarn) { *PWarn = &URLab_OnMujocoWarning; }
    UE_LOG(LogURLab, Log, TEXT("[URLab] MuJoCo error callbacks installed (err=%p warn=%p)"), (void*)PErr, (void*)PWarn);
    GMujocoCallbacksInstalled = true;
}

UMjPhysicsEngine::UMjPhysicsEngine()
{
    PrimaryComponentTick.bCanEverTick = false;

    Options.bOverride_Integrator = true;
    Options.Integrator = EMjIntegrator::ImplicitFast;
    ControlSource = EControlSource::ZMQ;

    URLab_InstallMujocoCallbacks();
}

void UMjPhysicsEngine::PreCompile()
{
    m_spec = mj_makeSpec();
    m_spec->compiler.degree = false;
    mj_defaultVFS(&m_vfs);

    UWorld* World = GetWorld();
    if (!World) return;

    TArray<AActor*> FoundActors;
    UGameplayStatics::GetAllActorsOfClass(World, AActor::StaticClass(), FoundActors);

    for (auto actor : FoundActors)
    {
        if (actor->FindComponentByClass<UMjQuickConvertComponent>())
        {
            UMjQuickConvertComponent* CustomPhysicsComponent = actor->FindComponentByClass<UMjQuickConvertComponent>();
            m_MujocoComponents.Add(CustomPhysicsComponent);
            CustomPhysicsComponent->Setup(m_spec, &m_vfs);
        }
        if (AMjArticulation* Articulation = Cast<AMjArticulation>(actor))
        {
            Articulation->Setup(m_spec, &m_vfs);
            m_articulations.Add(Articulation);
        }
        if (AMjHeightfieldActor* HFA = Cast<AMjHeightfieldActor>(actor))
        {
            HFA->Setup(m_spec, &m_vfs);
            m_heightfieldActors.Add(HFA);
        }
    }
}

void UMjPhysicsEngine::PostCompile()
{
    if (!m_model || !m_data)
    {
        UE_LOG(LogURLab, Error, TEXT("Skipping PostCompile: m_model or m_data is invalid."));
        return;
    }

    for (UMjQuickConvertComponent* mujocoComponent : m_MujocoComponents)
    {
        UE_LOG(LogURLab, Verbose, TEXT("Running PostSetup for component '%s'"), *mujocoComponent->GetName());
        mujocoComponent->PostSetup(m_model, m_data);
    }

    m_ArticulationMap.Empty();
    for (AMjArticulation* Art : m_articulations)
    {
        if (Art) m_ArticulationMap.Add(Art->GetName(), Art);
    }

    for (auto articulation : m_articulations)
        articulation->PostSetup(m_model, m_data);
    for (auto hfa : m_heightfieldActors)
        hfa->PostSetup(m_model, m_data);
}

void UMjPhysicsEngine::Compile()
{
    PreCompile();

    UE_LOG(LogURLab, Log, TEXT("Compiling MuJoCo model"));
    m_LastCompileError.Empty();
    m_model = mj_compile(m_spec, &m_vfs);

    if (!m_model)
    {
        const char* spec_error = mjs_getError(m_spec);
        m_LastCompileError = spec_error ? UTF8_TO_TCHAR(spec_error) : TEXT("Unknown compile error");
        UE_LOG(LogURLab, Error, TEXT("Model compilation failed: %s"), *m_LastCompileError);
#if WITH_EDITOR
        FMessageDialog::Open(EAppMsgType::Ok,
            FText::Format(NSLOCTEXT("URLab","CompileError","MuJoCo compile failed:\n\n{0}"), FText::FromString(m_LastCompileError)));
#endif
        return;
    }

    if (bSaveDebugXml)
    {
        FString CacheDir = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("URLab"));
        IFileManager::Get().MakeDirectory(*CacheDir, true);

        int model_size = mj_sizeModel(m_model);
        FString MjbPath = FPaths::Combine(CacheDir, TEXT("scene_compiled.mjb"));
        mj_saveModel(m_model, TCHAR_TO_UTF8(*MjbPath), nullptr, model_size);

        static constexpr int32 kXmlBufferSize = 100 * 1024 * 1024;
        static constexpr int32 kSaveErrorBufferSize = 10000;
        char* xmlBuf = (char*)FMemory::Malloc(kXmlBufferSize);
        if (xmlBuf)
        {
            FMemory::Memzero(xmlBuf, kXmlBufferSize);
            char saveError[kSaveErrorBufferSize] = "";
            int xmlResult = mj_saveXMLString(m_spec, xmlBuf, kXmlBufferSize, saveError, sizeof(saveError));
            int xmlLen = FCStringAnsi::Strlen(xmlBuf);
            if (xmlResult == 0 && xmlLen > 0)
            {
                FString XmlPath = FPaths::Combine(CacheDir, TEXT("scene_compiled.xml"));
                FString XmlContent = UTF8_TO_TCHAR(xmlBuf);

                XmlContent.ReplaceInline(TEXT("//"), TEXT("/"));

                {
                    bool bChanged = true;
                    while (bChanged)
                    {
                        int32 Prev = XmlContent.Len();
                        XmlContent.ReplaceInline(TEXT("file=\"../"), TEXT("file=\""), ESearchCase::CaseSensitive);
                        bChanged = (XmlContent.Len() != Prev);
                    }
                }

                XmlContent.ReplaceInline(TEXT("\\"), TEXT("/"));

                {
                    FRegexPattern Pattern(TEXT("file=\"[^\"]*?Saved/URLab/"));
                    FRegexMatcher Matcher(Pattern, XmlContent);

                    TArray<TPair<int32, int32>> Matches;
                    while (Matcher.FindNext())
                    {
                        Matches.Add(TPair<int32, int32>(Matcher.GetMatchBeginning(), Matcher.GetMatchEnding()));
                    }

                    for (int32 i = Matches.Num() - 1; i >= 0; --i)
                    {
                        int32 Start = Matches[i].Key;
                        int32 End = Matches[i].Value;
                        FString Before = XmlContent.Left(Start);
                        FString After = XmlContent.Mid(End);
                        XmlContent = Before + TEXT("file=\"") + After;
                    }
                }

                FFileHelper::SaveStringToFile(XmlContent, *XmlPath);
                UE_LOG(LogURLab, Log, TEXT("Debug XML saved to: %s (%d bytes, paths relativized)"), *XmlPath, xmlLen);
            }
            FMemory::Free(xmlBuf);
        }
    }

    int version = mj_version();
    UE_LOG(LogURLab, Log, TEXT("Model successfully compiled on version %i"), version);
    m_data = mj_makeData(m_model);

    if (!m_data)
    {
        UE_LOG(LogURLab, Error, TEXT("Data creation failed! m_data is NULL."));
        return;
    }

    UE_LOG(LogURLab, Log, TEXT("Data successfully made"));

    ApplyOptions();
    PostCompile();

    // Step once then reset to ensure all derived quantities (contacts,
    // constraints, sensor data) are fully computed and synced before the
    // user sees the paused scene.
    mj_step(m_model, m_data);
    mj_resetData(m_model, m_data);
    mj_forward(m_model, m_data);
}

void UMjPhysicsEngine::ApplyOptions()
{
    if (!m_model) return;

    Options.ApplyOverridesToModel(m_model);

    UE_LOG(LogURLab, Log, TEXT("Applied manager option overrides (timestep=%.4f from model)"),
        m_model->opt.timestep);
}

void UMjPhysicsEngine::RunMujocoAsync()
{
    if (!m_model || !m_data)
    {
        UE_LOG(LogURLab, Error, TEXT("Skipping RunMuJoCoAsync: m_model or m_data is invalid."));
        return;
    }

    bShouldStopTask = false;
    float TargetInterval = (float)m_model->opt.timestep;

    AsyncPhysicsFuture = Async(EAsyncExecution::Thread, [TargetInterval, this]() {
        FPlatformProcess::Sleep(0.0f);

        while (true)
        {
            const double LoopStartTime = FPlatformTime::Seconds();

            if (bShouldStopTask)
                break;

            {
                FScopeLock Lock(&CallbackMutex);

                if (!m_model || !m_data || bShouldStopTask)
                    break;

                if (bPendingReset)
                {
                    mj_resetData(m_model, m_data);
                    mj_forward(m_model, m_data);
                    bPendingReset = false;

                    // Zero all actuator control values so stale commands
                    // don't persist after reset.
                    for (AMjArticulation* Art : m_articulations)
                    {
                        if (!Art) continue;
                        for (UMjActuator* Act : Art->GetActuators())
                        {
                            if (Act) Act->ResetControl();
                        }
                    }

                    AsyncTask(ENamedThreads::GameThread, [this]() {
                        for (AMjArticulation* Art : m_articulations)
                        {
                            if (Art) Art->OnSimulationReset.Broadcast();
                        }
                    });
                }

                if (bPendingRestore)
                {
                    bPendingRestore = false;
                    if (PendingStateVector.Num() > 0)
                    {
                        mj_setState(m_model, m_data, PendingStateVector.GetData(), PendingStateMask);
                        mj_forward(m_model, m_data);
                    }
                }

                for (const FPhysicsCallback& Cb : PreStepCallbacks)
                {
                    Cb(m_model, m_data);
                }

                for (AMjArticulation* Art : m_articulations)
                {
                    if (Art) Art->ApplyControls();
                }

                if (!bIsPaused)
                {
                    if (CustomStepHandler)
                        CustomStepHandler(m_model, m_data);
                    else
                        mj_step(m_model, m_data);
                }

                for (const FPhysicsCallback& Cb : PostStepCallbacks)
                {
                    Cb(m_model, m_data);
                }

                if (OnPostStep)
                {
                    OnPostStep(m_model, m_data);
                }
            } // FScopeLock released here

            // Spin-wait for precise timing at small timesteps
            const float SpeedFactor = FMath::Clamp(SimSpeedPercent, 5.0f, 100.0f) / 100.0f;
            const double TargetTime = LoopStartTime + (TargetInterval / SpeedFactor);
            while (FPlatformTime::Seconds() < TargetTime)
            {
                FPlatformProcess::YieldThread();
            }
        }
    });
}

void UMjPhysicsEngine::SetControlSource(EControlSource NewSource)
{
    ControlSource = NewSource;
}

EControlSource UMjPhysicsEngine::GetControlSource() const
{
    return ControlSource;
}

void UMjPhysicsEngine::SetPaused(bool bPaused)
{
    bIsPaused = bPaused;
}

bool UMjPhysicsEngine::IsRunning() const
{
    return IsInitialized() && !bIsPaused;
}

bool UMjPhysicsEngine::IsInitialized() const
{
    return (m_model != nullptr && m_data != nullptr);
}

FString UMjPhysicsEngine::GetLastCompileError() const
{
    return m_LastCompileError;
}

void UMjPhysicsEngine::StepSync(int32 NumSteps)
{
    if (!IsInitialized()) return;

    bool bWasPaused = bIsPaused;
    bIsPaused = true;

    FScopeLock Lock(&CallbackMutex);

    for (int32 i = 0; i < NumSteps; ++i)
    {
        mj_step(m_model, m_data);
    }

    bIsPaused = bWasPaused;
}

bool UMjPhysicsEngine::CompileModel()
{
    bShouldStopTask = true;
    {
        FScopeLock Lock(&CallbackMutex);
        if (m_data)  { mj_deleteData(m_data);   m_data  = nullptr; }
        if (m_model) { mj_deleteModel(m_model); m_model = nullptr; }
    }
    if (m_spec)  { mj_deleteSpec(m_spec);   m_spec  = nullptr; }

    // Clear registered scene objects for re-scan
    m_MujocoComponents.Empty();
    m_articulations.Empty();
    m_heightfieldActors.Empty();

    Compile();

    if (!IsInitialized())
    {
        return false;
    }

    RunMujocoAsync();
    return true;
}

AMjArticulation* UMjPhysicsEngine::GetArticulation(const FString& ActorName) const
{
    if (const AMjArticulation* const* Found = m_ArticulationMap.Find(ActorName))
        return const_cast<AMjArticulation*>(*Found);
    for (AMjArticulation* Art : m_articulations)
    {
        if (Art && Art->GetName() == ActorName)
            return Art;
    }
    return nullptr;
}

TArray<AMjArticulation*> UMjPhysicsEngine::GetAllArticulations() const
{
    return m_articulations;
}

TArray<UMjQuickConvertComponent*> UMjPhysicsEngine::GetAllQuickComponents() const
{
    return m_MujocoComponents;
}

TArray<AMjHeightfieldActor*> UMjPhysicsEngine::GetAllHeightfields() const
{
    return m_heightfieldActors;
}

float UMjPhysicsEngine::GetSimTime() const
{
    if (m_data) return (float)m_data->time;
    return 0.0f;
}

float UMjPhysicsEngine::GetTimestep() const
{
    return m_model ? (float)m_model->opt.timestep : 0.002f;
}

void UMjPhysicsEngine::ResetSimulation()
{
    bPendingReset = true;
    UE_LOG(LogURLab, Log, TEXT("MuJoCo PhysicsEngine: Reset requested."));
}

void UMjPhysicsEngine::SetCustomStepHandler(FMujocoStepCallback Handler)
{
    FScopeLock Lock(&CallbackMutex);
    CustomStepHandler = Handler;
}

void UMjPhysicsEngine::ClearCustomStepHandler()
{
    FScopeLock Lock(&CallbackMutex);
    CustomStepHandler = nullptr;
}

UMjSimulationState* UMjPhysicsEngine::CaptureSnapshot()
{
    if (!m_model || !m_data) return nullptr;

    UMjSimulationState* NewSnapshot = NewObject<UMjSimulationState>(GetOwner());

    uint32 Mask = mjSTATE_INTEGRATION;

    int nState = mj_stateSize(m_model, Mask);
    NewSnapshot->StateVector.SetNum(nState);
    NewSnapshot->StateMask = (int32)Mask;
    NewSnapshot->SimTime = (float)m_data->time;

    {
        mj_getState(m_model, m_data, NewSnapshot->StateVector.GetData(), Mask);
    }

    UE_LOG(LogURLab, Log, TEXT("MuJoCo PhysicsEngine: Snapshot captured at t=%f (Size: %d)"), NewSnapshot->SimTime, nState);
    return NewSnapshot;
}

void UMjPhysicsEngine::RestoreSnapshot(UMjSimulationState* Snapshot)
{
    if (!Snapshot) return;

    PendingStateVector = Snapshot->StateVector;
    PendingStateMask   = Snapshot->StateMask;
    bPendingRestore    = true;

    UE_LOG(LogURLab, Log, TEXT("MuJoCo PhysicsEngine: Restore requested for snapshot t=%f"), Snapshot->SimTime);
}

void UMjPhysicsEngine::RegisterPreStepCallback(FPhysicsCallback Callback)
{
    PreStepCallbacks.Add(MoveTemp(Callback));
}

void UMjPhysicsEngine::RegisterPostStepCallback(FPhysicsCallback Callback)
{
    PostStepCallbacks.Add(MoveTemp(Callback));
}

void UMjPhysicsEngine::ClearCallbacks()
{
    PreStepCallbacks.Empty();
    PostStepCallbacks.Empty();
}
