#pragma once

#include "CoreMinimal.h"

#if PLATFORM_WINDOWS

#include "IEyeSource.h"

/// Eye-tracking backend for Pimax HMDs (verified on Dream Air) that talks to the
/// Pimax runtime through libPVRClient64.dll.
///
/// The DLL is loaded dynamically and only the PVR interface v1.26 function table is
/// used, so there is no build-time dependency on the Pimax SDK (headers or import
/// libs) and nothing Pimax-specific has to be redistributed with this project.
///
/// Threading: v1 is polled from the UE4 game thread once per tick (~60 Hz), which
/// matches the rate the runtime produces new eye-tracking samples at.
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
        /// Axis fixes in case the runtime's sign convention differs from our assumption.
        bool bInvertHorizontal = false;
        bool bInvertVertical = false;
    };

    explicit FPimaxPvrEyeSource(const FConfig &InConfig);
    virtual ~FPimaxPvrEyeSource() override;

    virtual bool Initialize() override;
    virtual DReyeVR::FEyeSample PollLatest() override;
    virtual void Shutdown() override;

  private:
#if WITH_DEV_AUTOMATION_TESTS
    friend class FPimaxPvrFreshnessTest;
#endif

    enum class EState
    {
        Disabled,     // constructed, never successfully initialized (or shut down)
        Initializing, // inside Initialize()
        Running,      // polling
        Failed,       // unrecoverable error; retried from PollLatest() with bounded backoff
    };

    void ReleaseRuntime(); // destroyHmd/shutdown/FreeLibrary; does not touch State
    bool ValidateDevice(); // getHmdInfo + VID/PID allowlist
    void TransitionTo(EState NewState, const TCHAR *Reason);

    FConfig Config;
    EState State = EState::Disabled;

    void *DllHandle = nullptr;       // HMODULE of libPVRClient64.dll
    void **InterfaceSlots = nullptr; // PVR v1.26 function table as a raw slot array
    void *HmdHandle = nullptr;       // pvrHmdHandle
    bool bRuntimeInitialised = false;

    // staleness tracking: the device timestamp must keep advancing
    double LastSourceTime = 0.0;
    double LastSourceTimeSeenHost = 0.0; // host time when LastSourceTime first appeared
    bool bHasLastSourceTime = false;

    uint64 SampleSequence = 0;
    int32 ConsecutiveFailures = 0;

    // diagnostics (throttled to one log line per 5 s)
    double LastDiagHostTime = 0.0;
    uint64 DiagValidCount = 0;
    uint64 DiagInvalidCount = 0;
    int32 LastResult = -1;        // last getEyeTrackingInfo result
    double LastTimeInSeconds = -1; // last raw device timestamp seen
    int32 LastHmdPresent = -1;    // last getHmdStatus values (-1 = unknown)
    int32 LastHmdMounted = -1;
    bool bEverValid = false;

    // bounded exponential backoff for re-initialization after Failed
    double InitBackoffSeconds = 1.0;
    double NextInitRetryHostTime = 0.0;

    // throttled logging (one warning per 5 s at most)
    double LastWarnHostTime = 0.0;
};

#endif // PLATFORM_WINDOWS
