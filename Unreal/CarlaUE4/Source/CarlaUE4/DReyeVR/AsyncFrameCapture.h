#pragma once

#include "CoreMinimal.h"

#include <atomic>
#include <cstdint>

/// Payload frozen at submit time. Later stages must carry these values through
/// unchanged and never recompute them from "current" counters.
struct FDReyeVRFrameRequest
{
    FString FileName;
    uint64 CaptureIndex = 0;
    double SimTime = 0.0;
};

/// One harvested readback. Empty Pixels marks a failed read; the queue counts
/// those as drops instead of handing them to the sink.
struct FDReyeVRCompletedFrame
{
    TArray<FColor> Pixels;
    FIntPoint Size = FIntPoint::ZeroValue;
    FDReyeVRFrameRequest Request;
};

/// Produces pixels for one opaque source handle asynchronously.
/// Production source handle: UTextureRenderTarget2D* read back on the render thread.
class IDReyeVRFrameReader
{
  public:
    virtual ~IDReyeVRFrameReader() {}

    /// Game thread. Submit one asynchronous read. Returns false when the read
    /// could not be submitted at all (caller counts a drop).
    virtual bool EnqueueRead(void *SourceHandle, const FDReyeVRFrameRequest &Request) = 0;

    /// Game-thread pump. Harvest completed reads (submit order) into OutCompleted.
    /// When bBlockUntilHarvested is true the call must synchronize so that reads
    /// finished by the underlying machinery are visible before it returns.
    virtual void Harvest(TArray<FDReyeVRCompletedFrame> &OutCompleted, bool bBlockUntilHarvested) = 0;

    /// Reads submitted but not yet harvested (diagnostics only).
    virtual int32 GetNumPendingReads() const = 0;
};

/// Writes harvested pixels to disk.
class IDReyeVRFrameSink
{
  public:
    virtual ~IDReyeVRFrameSink() {}

    /// Returns false when the frame could not be dispatched (e.g. queue at capacity).
    virtual bool Write(TArray<FColor> &&Pixels, FIntPoint Size, const FString &FileName) = 0;

    /// Bounded wait until every dispatched write has finished. False on timeout.
    virtual bool WaitForIdle(double TimeoutSeconds) = 0;

    virtual int32 GetNumPendingWrites() const = 0;
};

struct FAsyncFrameCaptureConfig
{
    int32 MaxInFlightCaptures = 8;
    double FlushTimeoutSeconds = 5.0;
};

/// Game-thread orchestrator: submit -> reader -> sink, with a bounded in-flight
/// window and counters for logging/tests.
class FAsyncFrameCaptureQueue
{
  public:
    FAsyncFrameCaptureQueue(IDReyeVRFrameReader &InReader, IDReyeVRFrameSink &InSink,
                            const FAsyncFrameCaptureConfig &InConfig);
    ~FAsyncFrameCaptureQueue(); // Flush()es

    /// Game thread. FileName/CaptureIndex/SimTime are frozen here. Returns false
    /// (and counts a drop) when the in-flight limit is reached.
    bool EnqueueCapture(void *SourceHandle, const FString &FileName, uint64 CaptureIndex, double SimTime);

    /// Called every game tick so completions keep draining.
    void Tick();

    /// Bounded wait until all in-flight reads reached the sink and the sink's own
    /// queue is idle. Never hangs; logs what is still pending at the deadline.
    void Flush();

    uint64 GetSubmitted() const { return Submitted.load(std::memory_order_relaxed); }
    uint64 GetCompleted() const { return CompletedCount.load(std::memory_order_relaxed); }
    uint64 GetDropped() const { return DroppedCount.load(std::memory_order_relaxed); }
    int32 GetInFlight() const { return InFlight.load(std::memory_order_relaxed); }
    int32 GetMaxInFlightSeen() const { return MaxInFlightSeen.load(std::memory_order_relaxed); }

  private:
    void HarvestAndDeliver(bool bBlock);

    IDReyeVRFrameReader &Reader;
    IDReyeVRFrameSink &Sink;
    FAsyncFrameCaptureConfig Config;
    std::atomic<uint64> Submitted{0};
    std::atomic<uint64> CompletedCount{0};
    std::atomic<uint64> DroppedCount{0};
    std::atomic<int32> InFlight{0};
    std::atomic<int32> MaxInFlightSeen{0};
};

/// Production reader: FRHIGPUTextureReadback on the render thread. The pending
/// readback list is render-thread-owned and only mutated inside render commands;
/// the game thread enqueues commands and reads atomic counters.
class FDReyeVRRenderThreadFrameReader final : public IDReyeVRFrameReader
{
  public:
    ~FDReyeVRRenderThreadFrameReader() override;

    bool EnqueueRead(void *SourceHandle, const FDReyeVRFrameRequest &Request) override;
    void Harvest(TArray<FDReyeVRCompletedFrame> &OutCompleted, bool bBlockUntilHarvested) override;
    int32 GetNumPendingReads() const override { return NumPendingReads.load(std::memory_order_acquire); }

  private:
    struct FPendingRead
    {
        TUniquePtr<class FRHIGPUTextureReadback> Readback; // created and destroyed on the render thread
        FDReyeVRFrameRequest Request;
    };
    void HarvestOnRenderThread(class FRHICommandListImmediate &RHICmdList);

    TArray<FPendingRead> PendingReads;             // render thread only, no lock
    FCriticalSection CompletedCS;                  // guards the handoff buffer only
    TArray<FDReyeVRCompletedFrame> CompletedFrames;
    std::atomic<int32> NumPendingReads{0};         // written on the render thread
};

/// Production sink: FImageWriteTask on GetHighResScreenshotConfig().ImageWriteQueue,
/// enqueued non-blocking (called from the pump; blocking would stall the frame).
class FDReyeVRImageWriteSink final : public IDReyeVRFrameSink
{
  public:
    bool Write(TArray<FColor> &&Pixels, FIntPoint Size, const FString &FileName) override;
    bool WaitForIdle(double TimeoutSeconds) override;
    int32 GetNumPendingWrites() const override;
};
