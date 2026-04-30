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
#include "Components/SceneCaptureComponent2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "MuJoCo/Components/MjComponent.h"
#include "HAL/Runnable.h"
#include "HAL/ThreadSafeBool.h"
#include "Containers/Queue.h"
#include "MuJoCo/Components/Sensors/MjCameraTypes.h"
#include "MjCamera.generated.h"

/**
 * @class FCameraZmqWorker
 * @brief Background thread for publishing high-bandwidth camera frames via ZeroMQ.
 */
class FCameraZmqWorker : public FRunnable
{
public:
    FCameraZmqWorker(const FString& InEndpoint, const FString& InTopic, FIntPoint InRes);
    virtual ~FCameraZmqWorker();

    virtual bool Init() override;
    virtual uint32 Run() override;
    virtual void Stop() override;
    virtual void Exit() override;

    void PushFrame(const TArray<FColor>& FrameData);
    FString GetBoundEndpoint() const { return BoundEndpoint; }

private:
    FString RequestedEndpoint;
    FString BoundEndpoint;
    FString Topic;
    FIntPoint Resolution;

    void* ZmqContext = nullptr;
    void* ZmqPublisher = nullptr;

    FThreadSafeBool bStopThread;
    TQueue<TArray<FColor>, EQueueMode::Spsc> FrameQueue;
};

/**
 * @class UMjCamera
 * @brief Represents a MuJoCo <camera> element as an Unreal sensor component.
 *
 * Placed in the Sensors group because cameras are observation devices, not geometry.
 * The component is cheap by default: no render target or GPU cost is incurred until
 * SetStreamingEnabled(true) is called.
 *
 * Key design points:
 *  - No ExportTo / RegisterToSpec — camera is UE-side only, not fed back to MuJoCo.
 *  - SetStreamingEnabled() allocates the RT and calls IStreamingManager::AddViewInformation
 *    so textures load correctly even when the player pawn is far away.
 *  - RequestReadback() enqueues a non-blocking GPU→CPU copy; check IsReadbackReady()
 *    on a subsequent tick, then consume with ConsumePixels().
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class URLAB_API UMjCamera : public UMjComponent
{
    GENERATED_BODY()

public:
    UMjCamera();

    // ---- Identification ----


    // ---- Camera Intrinsics ----

    /** @brief Vertical field of view in degrees (fovy= attribute). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MuJoCo|Camera")
    float Fovy = 45.0f;

    /** @brief Capture resolution (pixels). Defaults to 640×480. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MuJoCo|Camera")
    FIntPoint Resolution = FIntPoint(640, 480);

    /** @brief What this camera captures. Read at SetStreamingEnabled(true) time —
     *  toggle streaming off/on after changing. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MuJoCo|Camera")
    EMjCameraMode CaptureMode = EMjCameraMode::Real;

    /** @brief Near clip plane for Depth capture, centimetres. Values below this read as 0. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MuJoCo|Camera",
        meta = (EditCondition = "CaptureMode == EMjCameraMode::Depth", ClampMin = "0.1"))
    float DepthNearCm = 10.0f;

    /** @brief Far clip plane for Depth capture, centimetres. Values beyond read as the maximum. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MuJoCo|Camera",
        meta = (EditCondition = "CaptureMode == EMjCameraMode::Depth", ClampMin = "1.0"))
    float DepthFarCm = 10000.0f;

    // ---- Streaming ----

    /** @brief Boost factor passed to IStreamingManager::AddViewInformation.
     *  Increase to force higher-quality texture mips near this camera. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MuJoCo|Camera|Streaming")
    float StreamingBoost = 1.0f;

    // ---- Capture Components ----

    /** @brief The underlying SceneCaptureComponent2D. Capture is disabled by default. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "MuJoCo|Camera")
    USceneCaptureComponent2D* CaptureComponent;

    /** @brief The render target. Null until SetStreamingEnabled(true) is first called. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "MuJoCo|Camera")
    UTextureRenderTarget2D* RenderTarget = nullptr;

    // ---- ZeroMQ Streaming ----
    
    /** @brief If true, the camera will automatically broadcast its frames over ZeroMQ when streaming is enabled. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MuJoCo|Camera|Network")
    bool bEnableZmqBroadcast = false;

    /** @brief The ZMQ Endpoint for this specific camera (e.g., tcp://*:5558). Must be unique per camera. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MuJoCo|Camera|Network")
    FString ZmqEndpoint = TEXT("tcp://*:5558");

    // ---- Public API ----

    /**
     * @brief Allocates the render target and begins streaming / scene capture.
     *        Call with bEnable=false to stop rendering and free the capture budget.
     */
    UFUNCTION(BlueprintCallable, Category = "MuJoCo|Camera")
    void SetStreamingEnabled(bool bEnable);

    /**
     * @brief Enqueues a non-blocking asynchronous GPU→CPU pixel readback.
     *        No-op if streaming is not enabled or a readback is already in flight.
     */
    UFUNCTION(BlueprintCallable, Category = "MuJoCo|Camera")
    void RequestReadback();

    /**
     * @brief Returns true when the GPU→CPU copy started by RequestReadback() is complete.
     */
    UFUNCTION(BlueprintCallable, Category = "MuJoCo|Camera")
    bool IsReadbackReady() const;

    /**
     * @brief Consumes and returns the pixel array from the last completed readback.
     *        Returns an empty array if not ready. Clears the pending state.
     */
    UFUNCTION(BlueprintCallable, Category = "MuJoCo|Camera")
    TArray<FColor> ConsumePixels();

    /**
     * @brief Returns the ZMQ endpoint actually bound (may differ from ZmqEndpoint if auto-incremented).
     */
    UFUNCTION(BlueprintCallable, Category = "MuJoCo|Camera")
    FString GetActualZmqEndpoint() const;

    /**
     * @brief Returns the bound camera component pointer (for UI wiring).
     */
    UFUNCTION(BlueprintCallable, Category = "MuJoCo|Camera")
    UMjCamera* GetSelf() { return this; }

    /**
     * @brief Exports camera properties to a MuJoCo spec camera structure.
     * @param cam Pointer to the target mjsCamera structure.
     * @param def Optional default structure (unused, kept for API consistency).
     */
    void ExportTo(mjsCamera* cam, mjsDefault* def = nullptr);

    /**
     * @brief Imports properties from a MuJoCo XML <camera> node.
     *        Reads: name, fovy, pos, quat, euler, xyaxes, zaxis,
     *               width/height (MuJoCo 3.x resolution hints).
     */
    void ImportFromXml(const class FXmlNode* Node);
    void ImportFromXml(const class FXmlNode* Node, const struct FMjCompilerSettings& CompilerSettings);

protected:
    virtual void BeginPlay() override;
    virtual void TickComponent(float DeltaTime, ELevelTick TickType,
                               FActorComponentTickFunction* ThisTickFunction) override;
    virtual void OnRegister() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
    // ---- Internal helpers ----
    void SetupRenderTarget();
    void RegisterWithStreamingManager();

    /** For non-seg modes: rebuild HiddenComponents from the currently-live seg pools.
     *  Cheap (N siblings pointer copies), called each tick so a late-starting seg
     *  camera doesn't contaminate an already-streaming RGB/Depth camera. */
    void RefreshHiddenComponentsFromSegPools();

    // ---- Readback state ----
    TOptional<TArray<FColor>> PendingPixels;
    FRenderCommandFence       ReadbackFence;
    bool                      bReadbackPending  = false;
    bool                      bReadbackComplete = false;

    // ---- Streaming state ----
    bool bStreamingEnabled = false;

    // ---- ZMQ Worker ----
    FCameraZmqWorker* ZmqWorker = nullptr;
    FRunnableThread* WorkerThread = nullptr;
};
