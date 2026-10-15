#pragma once

#include "CoreMinimal.h"
#include "HAL/CriticalSection.h"
#include "Misc/ScopeLock.h"

#include <atomic>

#if PLATFORM_WINDOWS

#include "IEyeSource.h"

/// Eye-tracking backend for Pimax HMDs (verified on Dream Air) that talks to the
/// Pimax runtime through libPVRClient64.dll.
///
/// The DLL is loaded dynamically and only the PVR interface v1.26 function table is
/// used, so there is no build-time dependency on the Pimax SDK (headers or import
/// libs) and nothing Pimax-specific has to be redistributed with this project.
///
/// Threading: by default polled from the UE4 game thread; the tracker advances on a
/// fixed grid (measured 8.3 ms => 120 Hz on Dream Air) while the render loop can be far
/// slower, so PollAllAvailable() re-queries the earlier grid points the frame would
/// otherwise skip. When FConfig::AsyncPollHz > 0 a dedicated thread polls instead and
/// PollAllAvailable() drains that thread's buffer, so the gaze stream no longer follows
/// the frame rate (the runtime serves history poorly, so probing was not enough).
class FPimaxPvrEyeSource : public DReyeVR::IEyeSource
{
  public:
    struct FConfig
    {
        /// Skip the VID/PID allowlist and accept any device the runtime reports.
        bool bAllowUnknownPimaxDevice = false;
        /// Hex IDs from DReyeVRConfig.ini [Pimax] (Pimax VID = 0x34A4, Dream Air PID = 0x0044).
        TArray<int32> AllowedVendorIds = {0x34A4};
        TArray<int32> AllowedProductIds = {0x0012, 0x0040, 0x0042, 0x0044};
        /// A sample whose device timestamp has not advanced for longer than this is stale.
        float MaxSampleAgeSeconds = 0.5f;
        /// A sample holding the previous distinct angles while its device timestamp keeps
        /// advancing is a frozen tracker read rather than a fixation: measured on three
        /// sessions, 6.3% / 15.6% / 43.8% of their samples sat in such runs (worst case 6996
        /// consecutive samples over 59.2 s), and the staleness gate above cannot see them
        /// because it only watches the timestamp. Inside non-frozen activity the repeat run
        /// is p50=1, p90=1-2, p99=11-14 samples, so the default is ~2x the p99. 0 disables.
        double MaxFrozenSeconds = 0.25;
        /// Axis fixes in case the runtime's sign convention differs from our assumption.
        bool bInvertHorizontal = false;
        bool bInvertVertical = false;
        /// When > 0, a dedicated background thread polls the tracker at this rate
        /// instead of once per game frame. The render loop can be far slower than the
        /// tracker (measured 30 fps in VR), which silently caps the gaze stream at the
        /// frame rate; the thread keeps the stream at the tracker's native density.
        double AsyncPollHz = 0.0;
    };

    explicit FPimaxPvrEyeSource(const FConfig &InConfig);
    virtual ~FPimaxPvrEyeSource() override;

    virtual bool Initialize() override;
    virtual DReyeVR::FEyeSample PollLatest() override;
    virtual TArray<DReyeVR::FEyeSample> PollAllAvailable() override;
    virtual void Shutdown() override;

  private:
#if WITH_DEV_AUTOMATION_TESTS
    friend class FPimaxPvrFreshnessTest;
    friend class FPimaxPvrAsyncThreadTest;
    friend class FPimaxPvrFrozenValueTest;
#endif

    /// PollRunningLocked() body of PollLatest(), i.e. one PVR query with all its
    /// validity gates. The caller must hold RuntimeCS.
    void PollRunningLocked(DReyeVR::FEyeSample &Sample, double Now);

    /// Frozen-value gate on the four raw per-eye tangents: true while they still equal the
    /// last distinct ones and the device clock has run on for longer than
    /// FConfig::MaxFrozenSeconds since that change. Any moved tangent re-baselines the
    /// tracker, so the verdict recovers on the next changed sample. The comparison is
    /// bit-exact deliberately: the measured freezes are bit-identical, and a tolerance would
    /// hide slow drift while erasing real small saccades. Call only with values that already
    /// passed ApplyGazeInfo. Runs under RuntimeCS (the async poll thread calls it too).
    bool IsFrozenValue(float TanHL, float TanVL, float TanHR, float TanVR, double DeviceTimeS);

    /// Unlocked runtime bring-up/teardown, used by the locked public wrappers and by
    /// the recovery path (which already holds no other lock).
    bool InitializeImpl();
    void ReleaseRuntimeImpl();

    /// Failed-state recovery with bounded backoff; runs on the game thread.
    void RecoverIfNeeded();

    void StartPoller();
    void StopPoller();

    enum class EState
    {
        Disabled,     // constructed, never successfully initialized (or shut down)
        Initializing, // inside Initialize()
        Running,      // polling
        Failed,       // unrecoverable error; retried from PollLatest() with bounded backoff
    };

    bool ValidateDevice(); // getHmdInfo + VID/PID allowlist
    void TransitionTo(EState NewState, const TCHAR *Reason);

    FConfig Config;
    /// Atomic: the async poll thread reads it while the game thread transitions it.
    std::atomic<EState> State{EState::Disabled};

    // Serializes every PVR call and the state machine around it: the game thread
    // (initialize/shutdown/recovery) and the async poll thread never touch the
    // runtime concurrently.
    mutable FCriticalSection RuntimeCS;

    // Background sampling thread (nullptr unless Config.AsyncPollHz > 0)
    class FAsyncPoller;
    FAsyncPoller *Poller = nullptr;

    void *DllHandle = nullptr;       // HMODULE of libPVRClient64.dll
    void **InterfaceSlots = nullptr; // PVR v1.26 function table as a raw slot array
    void *HmdHandle = nullptr;       // pvrHmdHandle
    bool bRuntimeInitialised = false;

    // staleness tracking: the device timestamp must keep advancing
    double LastSourceTime = 0.0;
    double LastSourceTimeSeenHost = 0.0; // host time when LastSourceTime first appeared
    bool bHasLastSourceTime = false;

    // frozen-value tracking: the angles must keep changing, not only the timestamp
    float LastDistinctTan[4] = {0.f, 0.f, 0.f, 0.f};
    double LastDistinctChangeTime = 0.0; // device time of that last distinct sample
    bool bHasFrozenBaseline = false;

    std::atomic<uint64> SampleSequence{0};
    int32 ConsecutiveFailures = 0;

    // diagnostics (throttled to one log line per 5 s)
    double LastDiagHostTime = 0.0;
    uint64 DiagValidCount = 0;
    uint64 DiagInvalidCount = 0;
    uint64 DiagFrozenSamples = 0; // of the invalid ones, how many were frozen tracker reads
    int32 LastResult = -1;        // last getEyeTrackingInfo result
    double LastTimeInSeconds = -1; // last raw device timestamp seen
    int32 LastHmdPresent = -1;    // last getHmdStatus values (-1 = unknown)
    int32 LastHmdMounted = -1;
    bool bEverValid = false;

    // stream density per game frame: 1 without AsyncPollHz, > 1 with it
    std::atomic<uint64> DiagSamplesCollected{0};
    std::atomic<uint64> DiagPollAllCalls{0};

    // bounded exponential backoff for re-initialization after Failed
    double InitBackoffSeconds = 1.0;
    double NextInitRetryHostTime = 0.0;
};

#endif // PLATFORM_WINDOWS
