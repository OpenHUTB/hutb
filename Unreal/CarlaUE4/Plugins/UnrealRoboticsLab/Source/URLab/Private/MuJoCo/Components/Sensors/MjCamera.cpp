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

#include "MuJoCo/Components/Sensors/MjCamera.h"
#include "MuJoCo/Core/AMjManager.h"
#include "MuJoCo/Core/MjDebugVisualizer.h"
#include "MuJoCo/Net/MjNetworkManager.h"
#include "Engine/PostProcessVolume.h"
#include "EngineUtils.h"
#include "MuJoCo/Utils/MjUtils.h"
#include "MuJoCo/Utils/MjXmlUtils.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/Engine.h"
#include "ContentStreaming.h"
#include "XmlNode.h"
#include "RHICommandList.h"
#include "Utils/URLabLogging.h"
#include "MuJoCo/Core/MjArticulation.h"
#include "MuJoCo/Utils/MjOrientationUtils.h"
#include "zmq.h"

// ---------------------------------------------------------------------------
// FCameraZmqWorker
// ---------------------------------------------------------------------------

FCameraZmqWorker::FCameraZmqWorker(const FString& InEndpoint, const FString& InTopic, FIntPoint InRes)
    : RequestedEndpoint(InEndpoint), Topic(InTopic), Resolution(InRes), bStopThread(false)
{
    BoundEndpoint = RequestedEndpoint;
}

FCameraZmqWorker::~FCameraZmqWorker()
{
    Stop();
}

bool FCameraZmqWorker::Init()
{
    ZmqContext = zmq_ctx_new();
    ZmqPublisher = zmq_socket(ZmqContext, ZMQ_PUB);
    
    // Optimize for High Bandwidth (large HWM)
    int hwm = 10;
    zmq_setsockopt(ZmqPublisher, ZMQ_SNDHWM, &hwm, sizeof(hwm));

    // Simple port increment logic if port is busy
    FString TryEndpoint = RequestedEndpoint;
    int32 Port = 5558;
    FString BaseAddr = TEXT("tcp://*:");

    // Extract port from requested if it's not the default format
    if (RequestedEndpoint.Contains(TEXT(":")))
    {
        FString Left, Right;
        RequestedEndpoint.Split(TEXT(":"), &Left, &Right, ESearchCase::IgnoreCase, ESearchDir::FromEnd);
        if (Right.IsNumeric())
        {
            Port = FCString::Atoi(*Right);
            BaseAddr = Left + TEXT(":");
        }
    }

    int rc = -1;
    for (int i = 0; i < 10; ++i)
    {
        TryEndpoint = FString::Printf(TEXT("%s%d"), *BaseAddr, Port + i);
        rc = zmq_bind(ZmqPublisher, TCHAR_TO_UTF8(*TryEndpoint));
        if (rc == 0)
        {
            BoundEndpoint = TryEndpoint;
            break;
        }
    }

    if (rc != 0)
    {
        UE_LOG(LogURLabNet, Error, TEXT("CameraZmqWorker Failed to bind ZMQ after 10 retries. Starting at %s"), *RequestedEndpoint);
        return false;
    }
    
    UE_LOG(LogURLabNet, Log, TEXT("CameraZmqWorker Bound at %s [Topic: %s]"), *BoundEndpoint, *Topic);
    return true;
}

uint32 FCameraZmqWorker::Run()
{
    while (!bStopThread)
    {
        TArray<FColor> FrameData;
        
        // Blockingly wait for the next frame (using a small sleep to avoid spinning)
        // A proper implementation might use FEvent, but a tight sleep is fine for this queue.
        if (FrameQueue.Dequeue(FrameData))
        {
            if (FrameData.Num() != Resolution.X * Resolution.Y) continue;

            // FColor is BGRA (4 bytes per pixel)
            size_t PayloadSize = FrameData.Num() * sizeof(FColor);

            // Send Topic (ZMQ_SNDMORE)
            FString TopicSpace = Topic + TEXT(" ");
            zmq_send(ZmqPublisher, TCHAR_TO_UTF8(*TopicSpace), TopicSpace.Len(), ZMQ_SNDMORE);
            
            // Send Binary Payload
            zmq_send(ZmqPublisher, FrameData.GetData(), PayloadSize, 0);
        }
        else
        {
            FPlatformProcess::Sleep(0.002f); // 2ms sleep when idle
        }
    }
    return 0;
}

void FCameraZmqWorker::Stop()
{
    bStopThread = true;
}

void FCameraZmqWorker::Exit()
{
    if (ZmqPublisher)
    {
        zmq_close(ZmqPublisher);
        ZmqPublisher = nullptr;
    }
    if (ZmqContext)
    {
        zmq_ctx_term(ZmqContext);
        ZmqContext = nullptr;
    }
}

void FCameraZmqWorker::PushFrame(const TArray<FColor>& FrameData)
{
    // If the queue gets backed up (network is slow), we should probably drop old frames.
    // TQueue doesn't have an easy Count(), so we just push for now. 
    // The HWM on the ZMQ socket handles drops.
    FrameQueue.Enqueue(FrameData);
}

// ---------------------------------------------------------------------------
// UMjCamera
// ---------------------------------------------------------------------------

namespace
{
    bool IsSegMode(EMjCameraMode Mode)
    {
        return Mode == EMjCameraMode::SemanticSegmentation
            || Mode == EMjCameraMode::InstanceSegmentation;
    }

    UMjDebugVisualizer* FindDebugVisualizer(UWorld* FallbackWorld = nullptr)
    {
        if (AAMjManager* Manager = AAMjManager::GetManager())
        {
            return Manager->DebugVisualizer;
        }
        // Test/editor worlds don't dispatch BeginPlay, so the singleton may be unset.
        if (FallbackWorld)
        {
            for (TActorIterator<AAMjManager> It(FallbackWorld); It; ++It)
            {
                if (AAMjManager* Manager = *It)
                {
                    return Manager->DebugVisualizer;
                }
            }
        }
        return nullptr;
    }
}

UMjCamera::UMjCamera()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.bStartWithTickEnabled = true;

    // Create the scene capture sub-component
    CaptureComponent = CreateDefaultSubobject<USceneCaptureComponent2D>(TEXT("SceneCapture"));
    if (CaptureComponent)
    {
        CaptureComponent->SetupAttachment(this);
        CaptureComponent->SetRelativeScale3D(FVector(0.15f));

        // MuJoCo cameras look down -Z (forward), +Y (up).
        // Unreal cameras look down +X (forward), +Z (up).
        // MjToUERotation negates X/Z quat components (handedness flip),
        // which mirrors the Y axis — so "up" becomes -Y after conversion.
        const FVector  MjForward    = FVector(0.0f,  0.0f, -1.0f);
        const FVector  MjUp         = FVector(0.0f, -1.0f,  0.0f);
        const FRotator CorrectionRot = FRotationMatrix::MakeFromXZ(MjForward, MjUp).Rotator();
        CaptureComponent->SetRelativeRotation(CorrectionRot);

        // Start dormant — no capture cost until explicitly enabled
        CaptureComponent->bCaptureEveryFrame    = false;
        CaptureComponent->bCaptureOnMovement    = false;
        CaptureComponent->bAlwaysPersistRenderingState = true;
        CaptureComponent->MaxViewDistanceOverride      = -1.0f;

        // SceneCaptureComponent2D does NOT automatically respect scene Post Process Volumes.
        // We set PostProcessBlendWeight=1 so the component's own PostProcessSettings are used.
        // At BeginPlay, we copy settings from the scene's PPV to match the viewport look.
        CaptureComponent->PostProcessBlendWeight = 1.0f;
        CaptureComponent->bUseRayTracingIfEnabled = false;

        CaptureComponent->bHiddenInGame = true;
    }
}

void UMjCamera::OnRegister()
{
    Super::OnRegister();
    if (CaptureComponent)
    {
        CaptureComponent->FOVAngle = Fovy;
    }
}

void UMjCamera::BeginPlay()
{
    Super::BeginPlay();

    if (AAMjManager* Manager = AAMjManager::GetManager())
    {
        if (Manager->NetworkManager) Manager->NetworkManager->RegisterCamera(this);
    }

    // Copy post-process settings from the scene's Post Process Volume(s)
    // so the capture component matches the viewport look.
    if (CaptureComponent && GetWorld())
    {
        for (TActorIterator<APostProcessVolume> It(GetWorld()); It; ++It)
        {
            APostProcessVolume* PPV = *It;
            if (PPV && PPV->bEnabled)
            {
                CaptureComponent->PostProcessSettings = PPV->Settings;
                CaptureComponent->PostProcessBlendWeight = 1.0f;
                UE_LOG(LogURLab, Log, TEXT("[MjCamera] Copied post-process settings from '%s'"), *PPV->GetName());
                break; // Use the first enabled PPV
            }
        }
    }

    if (bEnableZmqBroadcast)
    {
        SetStreamingEnabled(true);
    }
}

void UMjCamera::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (AAMjManager* Manager = AAMjManager::GetManager())
    {
        if (Manager->NetworkManager) Manager->NetworkManager->UnregisterCamera(this);
    }

    if (WorkerThread)
    {
        WorkerThread->Kill(true);
        delete WorkerThread;
        WorkerThread = nullptr;
    }
    if (ZmqWorker)
    {
        delete ZmqWorker;
        ZmqWorker = nullptr;
    }

    // Make sure we stop rendering when the actor is torn down
    if (bStreamingEnabled)
    {
        SetStreamingEnabled(false);
    }
    Super::EndPlay(EndPlayReason);
}

void UMjCamera::TickComponent(float DeltaTime, ELevelTick TickType,
                               FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    if (bStreamingEnabled && CaptureComponent && CaptureComponent->TextureTarget)
    {
        // Register this viewpoint with the streaming manager every tick
        // (IStreamingManager uses timeout-based decay).
        RegisterWithStreamingManager();

        // Non-seg cameras re-sync their HiddenComponents each tick so siblings
        // spawned by a late-starting seg camera don't leak into this capture.
        if (!IsSegMode(CaptureMode))
        {
            RefreshHiddenComponentsFromSegPools();
        }

        // bCaptureEveryFrame (set by SetStreamingEnabled) drives the per-tick
        // capture. Calling CaptureScene() again here is redundant — UE warns
        // "Scene capture with bCaptureEveryFrame enabled was told to update —
        // major inefficiency", and under sustained render pressure the doubled
        // command submission can leave RHI frame breadcrumbs unbalanced.
    }

    // Check if an in-flight readback has completed
    if (bReadbackPending && ReadbackFence.IsFenceComplete())
    {
        bReadbackPending  = false;
        bReadbackComplete = true;

        if (bEnableZmqBroadcast && ZmqWorker && PendingPixels.IsSet())
        {
            ZmqWorker->PushFrame(PendingPixels.GetValue());
        }
    }

    // Auto-request the next frame if ZMQ is enabled to maintain the stream.
    // Depth mode doesn't have a float-capable ZMQ path yet (P5 follow-up), so skip.
    if (bStreamingEnabled && bEnableZmqBroadcast && !bReadbackPending
        && CaptureMode != EMjCameraMode::Depth)
    {
        RequestReadback();
    }
}

// ---------------------------------------------------------------------------
// Streaming setup
// ---------------------------------------------------------------------------

void UMjCamera::SetupRenderTarget()
{
    UTextureRenderTarget2D* RT = NewObject<UTextureRenderTarget2D>(this);

    const bool bDepthMode = (CaptureMode == EMjCameraMode::Depth);
    if (bDepthMode)
    {
        RT->RenderTargetFormat = ETextureRenderTargetFormat::RTF_R32f;
        RT->InitCustomFormat(Resolution.X, Resolution.Y, PF_R32_FLOAT, /*bForceLinearGamma=*/true);
    }
    else
    {
        RT->RenderTargetFormat = ETextureRenderTargetFormat::RTF_RGBA8;
        RT->InitCustomFormat(Resolution.X, Resolution.Y, PF_B8G8R8A8, /*bForceLinearGamma=*/true);
    }

    RT->bGPUSharedFlag = true;
    if (GEngine)
    {
        RT->TargetGamma = GEngine->GetDisplayGamma();
    }

    CaptureComponent->TextureTarget = RT;
    CaptureComponent->bAlwaysPersistRenderingState = true;
    CaptureComponent->MaxViewDistanceOverride = -1.0f;

    switch (CaptureMode)
    {
    case EMjCameraMode::Depth:
        CaptureComponent->CaptureSource = ESceneCaptureSource::SCS_SceneDepth;
        CaptureComponent->bOverride_CustomNearClippingPlane = true;
        CaptureComponent->CustomNearClippingPlane           = DepthNearCm;
        break;

    case EMjCameraMode::SemanticSegmentation:
    case EMjCameraMode::InstanceSegmentation:
        // Use the standard final-tone-curve pipeline — this is what the viewport
        // overlay uses and is known to render the MID tint correctly.
        // SCS_BaseColor was attempted but BasicShapeMaterial's `Color` vector
        // parameter isn't wired directly to the BaseColor G-buffer output,
        // so nothing appeared in the RT. Tints end up lit here (not pure flat
        // masks); a future pass that ships a dedicated unlit material would
        // give pixel-exact ground-truth colours.
        CaptureComponent->CaptureSource       = ESceneCaptureSource::SCS_FinalToneCurveHDR;
        CaptureComponent->PrimitiveRenderMode = ESceneCapturePrimitiveRenderMode::PRM_UseShowOnlyList;
        break;

    case EMjCameraMode::Real:
    default:
        CaptureComponent->CaptureSource      = ESceneCaptureSource::SCS_FinalToneCurveHDR;
        CaptureComponent->PrimitiveRenderMode = ESceneCapturePrimitiveRenderMode::PRM_LegacySceneCapture;
        break;
    }

    RenderTarget = RT;
    UE_LOG(LogURLabImport, Log,
        TEXT("[MjCamera] '%s' RT created mode=%s (%dx%d)"),
        *MjName,
        *UEnum::GetValueAsString(CaptureMode),
        Resolution.X, Resolution.Y);
}

void UMjCamera::RefreshHiddenComponentsFromSegPools()
{
    if (!CaptureComponent) return;

    UMjDebugVisualizer* Viz = FindDebugVisualizer(GetWorld());
    if (!Viz) return;

    CaptureComponent->HiddenComponents.Reset();

    TArray<UPrimitiveComponent*> Pool;
    Viz->GetSegPoolSiblings(EMjCameraMode::InstanceSegmentation, Pool);
    for (UPrimitiveComponent* P : Pool) CaptureComponent->HiddenComponents.Add(P);

    Pool.Reset();
    Viz->GetSegPoolSiblings(EMjCameraMode::SemanticSegmentation, Pool);
    for (UPrimitiveComponent* P : Pool) CaptureComponent->HiddenComponents.Add(P);
}

void UMjCamera::RegisterWithStreamingManager()
{
    // Inform the texture streaming system that this camera is an active viewpoint.
    // This ensures textures are streamed for the camera's frustum even when the
    // player pawn is far away. Mirrors the IStreamingManager call in old_camera.h.
    const float HFov     = CaptureComponent ? CaptureComponent->FOVAngle : Fovy;
    const float Distance = (HFov > 0.0f)
        ? Resolution.X / FMath::Tan(FMath::DegreesToRadians(HFov * 0.5f))
        : 1000.0f;

    IStreamingManager::Get().AddViewInformation(
        GetComponentLocation(),
        Resolution.X,
        Distance,
        StreamingBoost,
        /*bOverrideLocation=*/false,
        /*Duration=*/0.0f,
        GetOwner());
}

void UMjCamera::SetStreamingEnabled(bool bEnable)
{
    if (bEnable)
    {
        if (!RenderTarget)
        {
            SetupRenderTarget();
        }

        // Seg modes: subscribe to the shared sibling-mesh pool and point
        // ShowOnlyComponents at it. Pool is built lazily on first subscriber.
        if (IsSegMode(CaptureMode))
        {
            if (UMjDebugVisualizer* Viz = FindDebugVisualizer(GetWorld()))
            {
                TArray<UPrimitiveComponent*> Siblings;
                Viz->AcquireSegPool(CaptureMode, this, Siblings);

                CaptureComponent->ShowOnlyComponents.Reset();
                CaptureComponent->ShowOnlyComponents.Reserve(Siblings.Num());
                for (UPrimitiveComponent* Sib : Siblings)
                {
                    CaptureComponent->ShowOnlyComponents.Add(Sib);
                }
                UE_LOG(LogURLabImport, Log,
                    TEXT("[MjCamera] '%s' seg mode: acquired %d sibling(s) into ShowOnlyComponents."),
                    *MjName, Siblings.Num());
            }
            else
            {
                UE_LOG(LogURLabImport, Warning,
                    TEXT("[MjCamera] '%s' seg mode requested but no DebugVisualizer found — seg cam will show nothing."),
                    *MjName);
            }
        }
        else
        {
            // Non-seg modes (Real, Depth): siblings from other seg cameras are
            // bVisibleInSceneCaptureOnly=true, so they'd otherwise appear in RGB
            // captures. Seed HiddenComponents now; tick-time refresh keeps it in
            // sync with late-starting seg cameras.
            RefreshHiddenComponentsFromSegPools();
        }

        if (bEnableZmqBroadcast && !ZmqWorker)
        {
            if (CaptureMode == EMjCameraMode::Depth)
            {
                UE_LOG(LogURLabNet, Warning,
                    TEXT("[MjCamera] '%s' ZMQ broadcast skipped — Depth mode transports floats, the BGRA worker is RGB-only."),
                    *MjName);
            }
            else
            {
                AMjArticulation* Articulation = Cast<AMjArticulation>(GetOwner());
                FString Prefix = Articulation ? Articulation->GetName() : (GetOwner() ? GetOwner()->GetName() : TEXT("unknown"));
                FString Topic = FString::Printf(TEXT("%s/camera/%s"), *Prefix, *GetName());

                ZmqWorker = new FCameraZmqWorker(ZmqEndpoint, Topic, Resolution);
                WorkerThread = FRunnableThread::Create(ZmqWorker, TEXT("CameraZmqWorkerThread"), 0, TPri_BelowNormal);
            }
        }
        if (CaptureComponent)
        {
            CaptureComponent->FOVAngle          = Fovy;

            // CRITICAL: SetVisibility(true) must be called to allow the component
            // to dispatch scene capture updates. bHiddenInGame alone is not sufficient —
            // the capture system checks IsVisible() each frame.
            CaptureComponent->SetVisibility(true);
            CaptureComponent->SetActive(true);
            CaptureComponent->bHiddenInGame      = false;
            CaptureComponent->bCaptureEveryFrame = true;
            CaptureComponent->bCaptureOnMovement = false; // We drive capture manually each tick
        }
        bStreamingEnabled = true;
        RegisterWithStreamingManager();

        // Force an immediate capture so the UI doesn't wait a full tick.
        // CaptureScene() bypasses the visibility check — it always fires.
        if (CaptureComponent)
        {
            CaptureComponent->CaptureScene();
        }

        UE_LOG(LogURLabImport, Log, TEXT("[MjCamera] '%s' streaming ENABLED."), *MjName);
    }
    else
    {
        bStreamingEnabled = false;

        // Release the seg pool first, while CaptureMode still reflects what we subscribed as.
        if (IsSegMode(CaptureMode))
        {
            if (UMjDebugVisualizer* Viz = FindDebugVisualizer(GetWorld()))
            {
                Viz->ReleaseSegPool(CaptureMode, this);
            }
            if (CaptureComponent)
            {
                CaptureComponent->ShowOnlyComponents.Reset();
                CaptureComponent->PrimitiveRenderMode = ESceneCapturePrimitiveRenderMode::PRM_LegacySceneCapture;
            }
        }

        if (CaptureComponent)
        {
            CaptureComponent->bCaptureEveryFrame = false;
            CaptureComponent->SetVisibility(false); // Stop the capture dispatch loop
            CaptureComponent->TextureTarget = nullptr;
        }

        // Drop the RT so the next enable rebuilds it in the current CaptureMode.
        RenderTarget = nullptr;

        if (WorkerThread)
        {
            WorkerThread->Kill(true);
            delete WorkerThread;
            WorkerThread = nullptr;
        }
        if (ZmqWorker)
        {
            delete ZmqWorker;
            ZmqWorker = nullptr;
        }

        UE_LOG(LogURLabImport, Log, TEXT("[MjCamera] '%s' streaming DISABLED."), *MjName);
    }
}

// ---------------------------------------------------------------------------
// On-demand readback
// ---------------------------------------------------------------------------

void UMjCamera::RequestReadback()
{
    if (bReadbackPending || !RenderTarget)
    {
        return;
    }

    FTextureRenderTargetResource* Resource =
        RenderTarget->GameThread_GetRenderTargetResource();
    if (!Resource)
    {
        return;
    }

    bReadbackComplete = false;
    bReadbackPending  = true;
    PendingPixels.Emplace();  // Allocate output array

    TArray<FColor>* PixelsPtr = &PendingPixels.GetValue();
    const FIntRect  Rect(0, 0, Resource->GetSizeXY().X, Resource->GetSizeXY().Y);

    ENQUEUE_RENDER_COMMAND(MjCameraReadback)(
        [Resource, PixelsPtr, Rect](FRHICommandListImmediate& RHICmdList)
        {
            RHICmdList.ReadSurfaceData(
                Resource->GetRenderTargetTexture(),
                Rect,
                *PixelsPtr,
                FReadSurfaceDataFlags(RCM_UNorm, CubeFace_MAX));
        });

    ReadbackFence.BeginFence();
}

bool UMjCamera::IsReadbackReady() const
{
    return bReadbackComplete;
}

TArray<FColor> UMjCamera::ConsumePixels()
{
    if (PendingPixels.IsSet())
    {
        TArray<FColor> Result = MoveTemp(PendingPixels.GetValue());
        PendingPixels.Reset();
        bReadbackComplete = false;
        return Result;
    }
    return TArray<FColor>();
}

FString UMjCamera::GetActualZmqEndpoint() const
{
    if (ZmqWorker)
    {
        return ZmqWorker->GetBoundEndpoint();
    }
    return ZmqEndpoint;
}

// ---------------------------------------------------------------------------
// ExportTo
// ---------------------------------------------------------------------------

void UMjCamera::ExportTo(mjsCamera* cam, mjsDefault* /*def*/)
{
    if (!cam) return;

    cam->fovy = (double)Fovy;
    cam->resolution[0] = (int)Resolution.X;
    cam->resolution[1] = (int)Resolution.Y;
}

// ---------------------------------------------------------------------------
// XML Import
// ---------------------------------------------------------------------------

void UMjCamera::ImportFromXml(const FXmlNode* Node)
{
    FMjCompilerSettings DefaultSettings;
    ImportFromXml(Node, DefaultSettings);
}

void UMjCamera::ImportFromXml(const FXmlNode* Node, const FMjCompilerSettings& CompilerSettings)
{
    if (!Node) return;

    // Name
    if (!MjXmlUtils::ReadAttrString(Node, TEXT("name"), MjName))
        MjName = TEXT("Camera");

    // fovy
    FString FovyStr = Node->GetAttribute(TEXT("fovy"));
    if (!FovyStr.IsEmpty())
    {
        Fovy = FCString::Atof(*FovyStr);
        if (CaptureComponent)
        {
            CaptureComponent->FOVAngle = Fovy;
        }
    }

    // Resolution hints (MuJoCo 3.x: width= / height=)
    {
        bool bWOverride = false, bHOverride = false;
        MjXmlUtils::ReadAttrInt(Node, TEXT("width"),  Resolution.X, bWOverride);
        MjXmlUtils::ReadAttrInt(Node, TEXT("height"), Resolution.Y, bHOverride);
    }

    // Position
    FString PosStr = Node->GetAttribute(TEXT("pos"));
    if (!PosStr.IsEmpty())
    {
        TArray<FString> Parts;
        PosStr.ParseIntoArray(Parts, TEXT(" "), true);
        if (Parts.Num() >= 3)
        {
            double pos[3] = {
                FCString::Atod(*Parts[0]),
                FCString::Atod(*Parts[1]),
                FCString::Atod(*Parts[2])
            };
            SetRelativeLocation(MjUtils::MjToUEPosition(pos));
        }
    }

    // Orientation (quat, axisangle, euler, xyaxes, zaxis — priority order)
    double MjQuat[4];
    if (MjOrientationUtils::OrientationToMjQuat(Node, CompilerSettings, MjQuat))
    {
        SetRelativeRotation(MjUtils::MjToUERotation(MjQuat));
    }
}
