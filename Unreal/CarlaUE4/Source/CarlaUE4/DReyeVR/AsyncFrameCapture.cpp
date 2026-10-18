#include "AsyncFrameCapture.h"

#include "CarlaUE4.h" // LOG / LOG_WARN / LOG_ERROR
#include "Engine/TextureRenderTarget2D.h"
#include "HAL/PlatformProcess.h"
#include "HAL/PlatformTime.h"
#include "HighResScreenshot.h" // GetHighResScreenshotConfig
#include "ImageWriteQueue.h"
#include "ImageWriteTask.h"
#include "RHICommandList.h" // FRHICommandListImmediate
#include "RHIGPUReadback.h" // FRHIGPUTextureReadback
#include "RenderCommandFence.h"
#include "RenderingThread.h" // ENQUEUE_RENDER_COMMAND
#include "TextureResource.h" // FTextureRenderTargetResource

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#endif

/// ---------------------------------------------------------------------------
/// FAsyncFrameCaptureQueue
/// ---------------------------------------------------------------------------

FAsyncFrameCaptureQueue::FAsyncFrameCaptureQueue(IDReyeVRFrameReader &InReader, IDReyeVRFrameSink &InSink,
                                                 const FAsyncFrameCaptureConfig &InConfig)
    : Reader(InReader), Sink(InSink), Config(InConfig)
{
}

FAsyncFrameCaptureQueue::~FAsyncFrameCaptureQueue()
{
    Flush();
}

bool FAsyncFrameCaptureQueue::EnqueueCapture(void *SourceHandle, const FString &FileName, uint64 CaptureIndex,
                                             double SimTime)
{
    if (InFlight.load(std::memory_order_relaxed) >= Config.MaxInFlightCaptures)
    {
        DroppedCount.fetch_add(1, std::memory_order_relaxed);
        return false;
    }
    FDReyeVRFrameRequest Request;
    Request.FileName = FileName; // frozen here; later stages never recompute it
    Request.CaptureIndex = CaptureIndex;
    Request.SimTime = SimTime;
    if (!Reader.EnqueueRead(SourceHandle, Request))
    {
        DroppedCount.fetch_add(1, std::memory_order_relaxed);
        return false;
    }
    Submitted.fetch_add(1, std::memory_order_relaxed);
    const int32 Now = InFlight.fetch_add(1, std::memory_order_relaxed) + 1;
    int32 Seen = MaxInFlightSeen.load(std::memory_order_relaxed);
    while (Now > Seen && !MaxInFlightSeen.compare_exchange_weak(Seen, Now, std::memory_order_relaxed))
    {
    }
    return true;
}

void FAsyncFrameCaptureQueue::Tick()
{
    HarvestAndDeliver(/*bBlock=*/false);
}

void FAsyncFrameCaptureQueue::Flush()
{
    if (InFlight.load(std::memory_order_acquire) == 0 &&
        Reader.GetNumPendingReads() == 0 &&
        Sink.GetNumPendingWrites() == 0)
    {
        return;
    }
    const double Deadline = FPlatformTime::Seconds() + FMath::Max(Config.FlushTimeoutSeconds, 0.0);
    while (InFlight.load(std::memory_order_relaxed) > 0 && FPlatformTime::Seconds() < Deadline)
    {
        HarvestAndDeliver(/*bBlock=*/true);
        if (InFlight.load(std::memory_order_relaxed) > 0)
            FPlatformProcess::Sleep(0.002f);
    }
    if (InFlight.load(std::memory_order_relaxed) > 0)
    {
        LOG_WARN("DReyeVR async capture: flush timed out with %d of %llu captures still in flight",
                 InFlight.load(std::memory_order_relaxed), Submitted.load(std::memory_order_relaxed));
    }
    // The sink gets its own budget: readbacks that consumed the deadline above must
    // not starve writes that were already dispatched.
    if (!Sink.WaitForIdle(FMath::Max(Config.FlushTimeoutSeconds, 0.001)))
    {
        LOG_WARN("DReyeVR async capture: %d image writes still pending after flush", Sink.GetNumPendingWrites());
    }
}

void FAsyncFrameCaptureQueue::HarvestAndDeliver(bool bBlock)
{
    TArray<FDReyeVRCompletedFrame> Frames;
    Reader.Harvest(Frames, bBlock);
    for (FDReyeVRCompletedFrame &Frame : Frames)
    {
        if (Frame.Pixels.Num() == 0) // failed readback
        {
            DroppedCount.fetch_add(1, std::memory_order_relaxed);
            InFlight.fetch_sub(1, std::memory_order_relaxed);
            LOG_WARN("DReyeVR async capture: GPU readback failed for %s", *Frame.Request.FileName);
            continue;
        }
        if (Sink.Write(MoveTemp(Frame.Pixels), Frame.Size, Frame.Request.FileName))
            CompletedCount.fetch_add(1, std::memory_order_relaxed);
        else
            DroppedCount.fetch_add(1, std::memory_order_relaxed);
        InFlight.fetch_sub(1, std::memory_order_relaxed);
    }
}

/// ---------------------------------------------------------------------------
/// FDReyeVRRenderThreadFrameReader
/// ---------------------------------------------------------------------------

FDReyeVRRenderThreadFrameReader::~FDReyeVRRenderThreadFrameReader()
{
    // FRHIGPUTextureReadback instances must die on the render thread.
    ENQUEUE_RENDER_COMMAND(DReyeVRAsyncCapReaderShutdown)(
        [this](FRHICommandListImmediate &) { PendingReads.Empty(); });
    FRenderCommandFence Fence;
    Fence.BeginFence();
    Fence.Wait();
}

bool FDReyeVRRenderThreadFrameReader::EnqueueRead(void *SourceHandle, const FDReyeVRFrameRequest &Request)
{
    auto *RenderTarget = static_cast<UTextureRenderTarget2D *>(SourceHandle);
    if (RenderTarget == nullptr)
        return false;
    FTextureRenderTargetResource *Resource = RenderTarget->GameThread_GetRenderTargetResource();
    if (Resource == nullptr)
    {
        LOG_ERROR("DReyeVR async capture: missing render target resource");
        return false;
    }
    ENQUEUE_RENDER_COMMAND(DReyeVRAsyncCapEnqueueRead)([this, Resource, Request](FRHICommandListImmediate &RHICmdList) {
        FRHITexture *Texture = Resource->GetRenderTargetTexture();
        if (Texture == nullptr)
        {
            // deliver an empty (failed) frame so the queue releases the in-flight slot
            FDReyeVRCompletedFrame Failed;
            Failed.Request = Request;
            FScopeLock Lock(&CompletedCS);
            CompletedFrames.Add(MoveTemp(Failed));
            return;
        }
        FPendingRead Pending;
        Pending.Readback = MakeUnique<FRHIGPUTextureReadback>(TEXT("DReyeVRLiveCap"));
        Pending.Readback->EnqueueCopy(RHICmdList, Texture);
        Pending.Request = Request;
        PendingReads.Add(MoveTemp(Pending));
        NumPendingReads.store(PendingReads.Num(), std::memory_order_release);
    });
    return true;
}

void FDReyeVRRenderThreadFrameReader::HarvestOnRenderThread(FRHICommandListImmediate &RHICmdList)
{
    TArray<int32> ReadyIndices;
    for (int32 i = 0; i < PendingReads.Num(); ++i)
    {
        if (PendingReads[i].Readback->IsReady())
            ReadyIndices.Add(i);
    }
    if (ReadyIndices.Num() == 0)
        return;
    TArray<FDReyeVRCompletedFrame> Harvested;
    Harvested.Reserve(ReadyIndices.Num());
    for (const int32 Index : ReadyIndices)
    {
        FRHIGPUTextureReadback &Readback = *PendingReads[Index].Readback;
        FDReyeVRCompletedFrame Frame;
        Frame.Request = PendingReads[Index].Request;
        const FIntVector Size = Readback.GetSizeXYZ();
        void *Data = nullptr;
        int32 RowPitchInPixels = 0;
        Readback.LockTexture(RHICmdList, Data, RowPitchInPixels);
        // The per-row copy below assumes 32-bit BGRA. A format mismatch would still produce
        // a valid-looking JPEG of garbage, so fail the read and let the queue count a drop.
        if (Data != nullptr && Size.X > 0 && Size.Y > 0 && RowPitchInPixels >= Size.X &&
            Readback.GetFormat() == PF_B8G8R8A8)
        {
            Frame.Size = FIntPoint(Size.X, Size.Y);
            Frame.Pixels.SetNumUninitialized(Size.X * Size.Y);
            const uint8 *Src = static_cast<const uint8 *>(Data);
            for (int32 Row = 0; Row < Size.Y; ++Row)
            {
                FMemory::Memcpy(Frame.Pixels.GetData() + Row * Size.X, Src + (int64)Row * RowPitchInPixels * sizeof(FColor),
                                Size.X * sizeof(FColor));
            }
        }
        Readback.Unlock();
        Harvested.Add(MoveTemp(Frame)); // empty Pixels marks the failure path
    }
    for (int32 i = ReadyIndices.Num() - 1; i >= 0; --i)
        PendingReads.RemoveAt(ReadyIndices[i]);
    NumPendingReads.store(PendingReads.Num(), std::memory_order_release);
    {
        FScopeLock Lock(&CompletedCS);
        CompletedFrames.Append(MoveTemp(Harvested));
    }
}

void FDReyeVRRenderThreadFrameReader::Harvest(TArray<FDReyeVRCompletedFrame> &OutCompleted, bool bBlockUntilHarvested)
{
    ENQUEUE_RENDER_COMMAND(DReyeVRAsyncCapHarvest)(
        [this](FRHICommandListImmediate &RHICmdList) { HarvestOnRenderThread(RHICmdList); });
    if (bBlockUntilHarvested)
    {
        FRenderCommandFence Fence;
        Fence.BeginFence();
        Fence.Wait();
    }
    FScopeLock Lock(&CompletedCS);
    OutCompleted.Append(MoveTemp(CompletedFrames));
    CompletedFrames.Reset();
}

/// ---------------------------------------------------------------------------
/// FDReyeVRImageWriteSink
/// ---------------------------------------------------------------------------

bool FDReyeVRImageWriteSink::Write(TArray<FColor> &&Pixels, FIntPoint Size, const FString &FileName)
{
    FHighResScreenshotConfig &ScreenshotConfig = GetHighResScreenshotConfig();
    if (ScreenshotConfig.ImageWriteQueue == nullptr)
        return false;
    TUniquePtr<TImagePixelData<FColor>> PixelData = MakeUnique<TImagePixelData<FColor>>(Size);
    PixelData->Pixels.SetNumUninitialized(Pixels.Num());
    FMemory::Memcpy(PixelData->Pixels.GetData(), Pixels.GetData(), Pixels.Num() * sizeof(FColor));
    TUniquePtr<FImageWriteTask> ImageTask = MakeUnique<FImageWriteTask>();
    ImageTask->PixelData = MoveTemp(PixelData);
    ImageTask->Filename = FileName;
    ImageTask->Format = EImageFormat::JPEG;
    ImageTask->CompressionQuality = (int32)EImageCompressionQuality::Default;
    ImageTask->bOverwriteFile = true;
    ImageTask->PixelPreProcessors.Add(TAsyncAlphaWrite<FColor>(255));
    // Non-blocking: called from the game-thread pump; a full queue drops instead of stalling.
    TFuture<bool> Future = ScreenshotConfig.ImageWriteQueue->Enqueue(MoveTemp(ImageTask), /*bInBlockIfAtCapacity=*/false);
    return Future.IsValid();
}

bool FDReyeVRImageWriteSink::WaitForIdle(double TimeoutSeconds)
{
    FHighResScreenshotConfig &ScreenshotConfig = GetHighResScreenshotConfig();
    if (ScreenshotConfig.ImageWriteQueue == nullptr)
        return true;
    TFuture<void> Fence = ScreenshotConfig.ImageWriteQueue->CreateFence();
    return Fence.WaitFor(FTimespan::FromSeconds(TimeoutSeconds));
}

int32 FDReyeVRImageWriteSink::GetNumPendingWrites() const
{
    FHighResScreenshotConfig &ScreenshotConfig = GetHighResScreenshotConfig();
    return ScreenshotConfig.ImageWriteQueue != nullptr ? ScreenshotConfig.ImageWriteQueue->GetNumPendingTasks() : 0;
}

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAsyncFrameCaptureQueueTest, "HUTB.DReyeVR.LiveCapture.AsyncQueue",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

namespace
{
/// Holds completions for N Harvest calls, then releases them in submit order.
class FFakeFrameReader final : public IDReyeVRFrameReader
{
  public:
    int32 HoldHarvests = 0;
    bool bFailSubmit = false;

    bool EnqueueRead(void *, const FDReyeVRFrameRequest &Request) override
    {
        if (bFailSubmit)
            return false;
        FEntry Entry;
        Entry.Request = Request;
        Entry.HarvestsRemaining = HoldHarvests;
        Pending.Add(MoveTemp(Entry));
        return true;
    }

    void Harvest(TArray<FDReyeVRCompletedFrame> &OutCompleted, bool) override
    {
        TArray<int32> Ready;
        for (int32 i = 0; i < Pending.Num(); ++i)
        {
            if (--Pending[i].HarvestsRemaining <= 0)
                Ready.Add(i);
        }
        for (const int32 Index : Ready) // submit order preserved
        {
            FDReyeVRCompletedFrame Frame;
            Frame.Request = Pending[Index].Request;
            Frame.Size = FIntPoint(2, 2);
            Frame.Pixels.Init(FColor::Black, 4);
            OutCompleted.Add(MoveTemp(Frame));
        }
        for (int32 i = Ready.Num() - 1; i >= 0; --i)
            Pending.RemoveAt(Ready[i]);
    }

    int32 GetNumPendingReads() const override { return Pending.Num(); }

  private:
    struct FEntry
    {
        FDReyeVRFrameRequest Request;
        int32 HarvestsRemaining = 0;
    };
    TArray<FEntry> Pending;
};

class FFakeFrameSink final : public IDReyeVRFrameSink
{
  public:
    TArray<FString> WrittenFileNames;
    bool bFailWrites = false;

    bool Write(TArray<FColor> &&, FIntPoint, const FString &FileName) override
    {
        if (bFailWrites)
            return false;
        WrittenFileNames.Add(FileName);
        ++WritesAccepted;
        return true;
    }

    bool WaitForIdle(double) override
    {
        WritesAccepted = 0; // the fake completes writes instantly
        return true;
    }

    int32 GetNumPendingWrites() const override { return WritesAccepted; }

  private:
    int32 WritesAccepted = 0;
};

FAsyncFrameCaptureConfig MakeConfig(int32 MaxInFlight, double TimeoutSeconds = 2.0)
{
    FAsyncFrameCaptureConfig Config;
    Config.MaxInFlightCaptures = MaxInFlight;
    Config.FlushTimeoutSeconds = TimeoutSeconds;
    return Config;
}
} // namespace

bool FAsyncFrameCaptureQueueTest::RunTest(const FString &Parameters)
{
    // 1. Accepted requests reach the sink exactly once with the submit-time file name,
    //    even when the "current" index/sim-time move on before completion.
    {
        FFakeFrameReader Reader;
        FFakeFrameSink Sink;
        Reader.HoldHarvests = 2;
        FAsyncFrameCaptureQueue Queue(Reader, Sink, MakeConfig(8));
        uint64 CurrentIndex = 0;
        double CurrentSimTime = 1.0;
        TArray<FString> ExpectedNames;
        for (int32 i = 0; i < 4; ++i)
        {
            const FString Name = FString::Printf(TEXT("%06llu_%.4fs.jpg"), CurrentIndex, CurrentSimTime);
            ExpectedNames.Add(Name);
            TestTrue(TEXT("Capture accepted"), Queue.EnqueueCapture(nullptr, Name, CurrentIndex, CurrentSimTime));
            ++CurrentIndex;      // the world moves on; frozen names must not follow
            CurrentSimTime += 0.05;
        }
        Queue.Tick(); // first harvest: still held
        TestEqual(TEXT("Nothing reaches the sink while the reader holds"), Sink.WrittenFileNames.Num(), 0);
        Queue.Tick(); // second harvest releases all four
        TestEqual(TEXT("Every accepted request reaches the sink exactly once"), Sink.WrittenFileNames.Num(), 4);
        bool bNamesFrozen = Sink.WrittenFileNames.Num() == ExpectedNames.Num();
        for (int32 i = 0; i < Sink.WrittenFileNames.Num() && i < ExpectedNames.Num(); ++i)
            bNamesFrozen = bNamesFrozen && Sink.WrittenFileNames[i] == ExpectedNames[i];
        TestTrue(TEXT("File names are the ones frozen at submit time, in submit order"), bNamesFrozen);
        TestEqual(TEXT("Completed counter matches"), (int32)Queue.GetCompleted(), 4);
        TestEqual(TEXT("Nothing dropped"), (int32)Queue.GetDropped(), 0);
        TestEqual(TEXT("In-flight drained to zero"), Queue.GetInFlight(), 0);
    }

    // 2. In-flight limit: while the reader holds everything, further submits fail
    //    and increment the drop counter.
    {
        FFakeFrameReader Reader;
        FFakeFrameSink Sink;
        Reader.HoldHarvests = 5; // released by repeated harvests during Flush
        FAsyncFrameCaptureQueue Queue(Reader, Sink, MakeConfig(3));
        for (int32 i = 0; i < 3; ++i)
            TestTrue(TEXT("Under the limit"), Queue.EnqueueCapture(nullptr, FString::Printf(TEXT("%d.jpg"), i), i, 0.0));
        TestFalse(TEXT("Over the limit"), Queue.EnqueueCapture(nullptr, TEXT("3.jpg"), 3, 0.0));
        TestFalse(TEXT("Still over the limit"), Queue.EnqueueCapture(nullptr, TEXT("4.jpg"), 4, 0.0));
        TestEqual(TEXT("Drop counter incremented"), (int32)Queue.GetDropped(), 2);
        TestEqual(TEXT("Submitted counts accepted only"), (int32)Queue.GetSubmitted(), 3);
        TestEqual(TEXT("In-flight is at the limit"), Queue.GetInFlight(), 3);
        TestEqual(TEXT("Max in-flight seen"), Queue.GetMaxInFlightSeen(), 3);
        TestEqual(TEXT("The sink saw nothing"), Sink.WrittenFileNames.Num(), 0);

        // 3. Flush completes everything still in flight and leaves zero in flight.
        Queue.Flush();
        TestEqual(TEXT("Flush delivered every held capture"), Sink.WrittenFileNames.Num(), 3);
        TestEqual(TEXT("Flush order is submit order"), Sink.WrittenFileNames[2], FString(TEXT("2.jpg")));
        TestEqual(TEXT("In-flight zero after flush"), Queue.GetInFlight(), 0);
        TestEqual(TEXT("Completed counter after flush"), (int32)Queue.GetCompleted(), 3);
    }

    // 4. The destructor flushes: submitted captures are not lost.
    {
        FFakeFrameReader Reader;
        FFakeFrameSink Sink;
        Reader.HoldHarvests = 2;
        {
            FAsyncFrameCaptureQueue Queue(Reader, Sink, MakeConfig(8));
            for (int32 i = 0; i < 5; ++i)
                Queue.EnqueueCapture(nullptr, FString::Printf(TEXT("%d.jpg"), i), i, 0.0);
            Queue.Tick(); // only partially drained on purpose
        } // ~FAsyncFrameCaptureQueue must deliver the rest
        TestEqual(TEXT("Destructor loses no submitted captures"), Sink.WrittenFileNames.Num(), 5);
        TestEqual(TEXT("Destructor preserves submit order"), Sink.WrittenFileNames[4], FString(TEXT("4.jpg")));
    }

    // 5. Failed submits and failed readbacks count as drops, not completions.
    {
        FFakeFrameReader Reader;
        FFakeFrameSink Sink;
        Reader.bFailSubmit = true;
        FAsyncFrameCaptureQueue Queue(Reader, Sink, MakeConfig(4));
        TestFalse(TEXT("Failed submit is refused"), Queue.EnqueueCapture(nullptr, TEXT("x.jpg"), 0, 0.0));
        TestEqual(TEXT("Failed submit counted as drop"), (int32)Queue.GetDropped(), 1);
        TestEqual(TEXT("Failed submit not counted as submitted"), (int32)Queue.GetSubmitted(), 0);
        TestEqual(TEXT("Failed submit leaves in-flight at zero"), Queue.GetInFlight(), 0);
    }
    return true;
}
#endif // WITH_DEV_AUTOMATION_TESTS
