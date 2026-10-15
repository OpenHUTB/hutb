#include "PimaxPvrEyeSource.h"

#if PLATFORM_WINDOWS

#include "CarlaUE4.h" // LOG / LOG_WARN / LOG_ERROR
#include "HAL/PlatformProcess.h"
#include "HAL/PlatformTime.h"
#include "HAL/Runnable.h"
#include "HAL/RunnableThread.h"
#include "PimaxRuntimePaths.h"

#include <atomic>

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#endif

#include "Windows/AllowWindowsPlatformTypes.h"
#include <windows.h>
#include "Windows/HideWindowsPlatformTypes.h"

/// ---------------------------------------------------------------------------
/// Minimal PVR v1.26 ABI declarations.
///
/// These mirror the layouts from the official PVR SDK headers (PVR_Types.h /
/// PVR_Interface.h) just closely enough to call through the function table, and
/// are guarded by static_asserts below. Declaring them locally (rather than
/// shipping the Pimax headers) keeps this repository free of third-party
/// licensed SDK code. The layouts and the slot indices were verified against
/// the official headers and on real Dream Air hardware (see the
/// pimax_eye_probe tool, runtime 1.33.1).
/// ---------------------------------------------------------------------------
namespace PvrAbi
{
typedef int32 FPvrResult; // pvrResult
typedef void *FPvrHmdHandle;

constexpr FPvrResult Success = 0; // pvr_success

struct FPvrVector2f
{
    float X, Y;
};

/// pvrEyeTrackingInfo
struct alignas(8) FPvrEyeTrackingInfo
{
    FPvrVector2f GazeTan[2]; // tangent of the gaze angle, per eye [left, right]
    double TimeInSeconds;    // device clock; stays 0 while the tracker produces no data
};

/// pvrHmdInfo (pvrSizei Resolution flattened to two int32)
struct alignas(8) FPvrHmdInfo
{
    char ProductName[64];
    char Manufacturer[64];
    int32 VendorId;
    int32 ProductId;
    char SerialNumber[24];
    int32 FirmwareMajor;
    int32 FirmwareMinor;
    int32 ResolutionW;
    int32 ResolutionH;
};

/// pvrHmdStatus (6 x pvrBool(char) + padding)
struct alignas(8) FPvrHmdStatus
{
    uint8 IsVisible;
    uint8 HmdPresent;
    uint8 HmdMounted;
    uint8 DisplayLost;
    uint8 ServiceReady;
    uint8 ShouldQuit;
    uint8 _Pad[2];
};

/// Indices into the pvrInterfaceV26 function table.
enum ESlot
{
    Slot_Initialise = 0,
    Slot_Shutdown = 1,
    Slot_CreateHmd = 2,
    Slot_DestroyHmd = 3,
    Slot_GetVersionString = 4,
    Slot_GetTimeSeconds = 5,
    Slot_GetHmdInfo = 6,
    Slot_GetHmdStatus = 9,
    Slot_GetEyeTrackingInfo = 65,
    Slot_Count = 66,
};

/// Function signatures (x64 has a single calling convention, no decoration needed).
typedef FPvrResult (*InitialiseFn)();
typedef void (*ShutdownFn)();
typedef FPvrResult (*CreateHmdFn)(FPvrHmdHandle *);
typedef void (*DestroyHmdFn)(FPvrHmdHandle);
typedef const char *(*GetVersionStringFn)();
typedef double (*GetTimeSecondsFn)();
typedef FPvrResult (*GetHmdInfoFn)(FPvrHmdHandle, FPvrHmdInfo *);
typedef FPvrResult (*GetHmdStatusFn)(FPvrHmdHandle, FPvrHmdStatus *);
typedef FPvrResult (*GetEyeTrackingInfoFn)(FPvrHmdHandle, double AbsTime, FPvrEyeTrackingInfo *);
typedef void *(*GetPvrInterfaceFn)(uint32 Major, uint32 Minor);
} // namespace PvrAbi

static_assert(sizeof(PvrAbi::FPvrVector2f) == 8, "PVR ABI mismatch: pvrVector2f");
static_assert(sizeof(PvrAbi::FPvrEyeTrackingInfo) == 24, "PVR ABI mismatch: pvrEyeTrackingInfo");
static_assert(offsetof(PvrAbi::FPvrEyeTrackingInfo, TimeInSeconds) == 16, "PVR ABI mismatch: TimeInSeconds");
static_assert(sizeof(PvrAbi::FPvrHmdInfo) == 176, "PVR ABI mismatch: pvrHmdInfo");
static_assert(offsetof(PvrAbi::FPvrHmdInfo, VendorId) == 128, "PVR ABI mismatch: VendorId");
static_assert(offsetof(PvrAbi::FPvrHmdInfo, ProductId) == 132, "PVR ABI mismatch: ProductId");
static_assert(sizeof(PvrAbi::FPvrHmdStatus) == 8, "PVR ABI mismatch: pvrHmdStatus");

namespace
{
// Pimax Play supplies this library AND the runtime/services it communicates with.
// These are alternate locations of ONE DLL, not two required libraries.
TArray<FString> FindPvrDllCandidates()
{
    TArray<FString> Roots;
    for (HKEY Hive : {HKEY_LOCAL_MACHINE, HKEY_CURRENT_USER})
    {
        for (REGSAM View : {KEY_WOW64_64KEY, KEY_WOW64_32KEY})
        {
            HKEY Uninstall = nullptr;
            if (RegOpenKeyExW(Hive, L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall", 0,
                              KEY_READ | View, &Uninstall) != ERROR_SUCCESS)
                continue;
            WCHAR Name[256];
            for (DWORD Index = 0;; ++Index)
            {
                DWORD Length = UE_ARRAY_COUNT(Name);
                const LSTATUS Result = RegEnumKeyExW(Uninstall, Index, Name, &Length, nullptr, nullptr, nullptr, nullptr);
                if (Result == ERROR_NO_MORE_ITEMS)
                    break;
                if (Result != ERROR_SUCCESS)
                    continue;
                HKEY Product = nullptr;
                if (RegOpenKeyExW(Uninstall, Name, 0, KEY_READ | View, &Product) != ERROR_SUCCESS)
                    continue;
                WCHAR DisplayName[512] = {}, Location[32768] = {};
                DWORD DisplayBytes = sizeof(DisplayName), LocationBytes = sizeof(Location);
                if (RegGetValueW(Product, nullptr, L"DisplayName", RRF_RT_REG_SZ, nullptr, DisplayName,
                                  &DisplayBytes) == ERROR_SUCCESS &&
                    (FString(DisplayName).Contains(TEXT("Pimax")) || FString(DisplayName).Contains(TEXT("PiTool"))) &&
                    RegGetValueW(Product, nullptr, L"InstallLocation", RRF_RT_REG_SZ | RRF_RT_REG_EXPAND_SZ,
                                  nullptr, Location, &LocationBytes) == ERROR_SUCCESS)
                    Roots.AddUnique(FString(Location));
                RegCloseKey(Product);
            }
            RegCloseKey(Uninstall);
        }
    }
    for (const TCHAR *Variable : {TEXT("ProgramW6432"), TEXT("ProgramFiles"), TEXT("ProgramFiles(x86)")})
    {
        const FString ProgramFiles = FPlatformMisc::GetEnvironmentVariable(Variable);
        if (!ProgramFiles.IsEmpty())
            Roots.AddUnique(FPaths::Combine(ProgramFiles, TEXT("Pimax")));
    }
    WCHAR SystemDirectory[32768] = {};
    const UINT Length = GetSystemDirectoryW(SystemDirectory, UE_ARRAY_COUNT(SystemDirectory));
    return PimaxRuntimePaths::Build(FPlatformMisc::GetEnvironmentVariable(TEXT("PIMAX_PVR_DLL")),
                                   Length > 0 && Length < UE_ARRAY_COUNT(SystemDirectory) ? FString(SystemDirectory)
                                                                                        : FString(), Roots);
}
constexpr uint32 PvrInterfaceMajor = 1;
constexpr uint32 PvrInterfaceMinor = 26;
/// ~2 s of failed polls at 60 fps before the session is declared dead.
constexpr int32 MaxConsecutiveFailures = 120;
/// |tan(angle)| beyond this (~84 deg) is not a plausible gaze direction.
constexpr float MaxPlausibleGazeTan = 10.f;

/// Convert one raw PVR record into per-eye and cyclopean gaze. Shared by the live poll and
/// the background poll thread so both apply the same plausibility and sign rules.
/// Returns false when the tangents are not usable; the caller keeps its own gates.
bool ApplyGazeInfo(const PvrAbi::FPvrEyeTrackingInfo &Info, const FPimaxPvrEyeSource::FConfig &Config,
                   DReyeVR::FEyeSample &Out)
{
    const float TanHL = Info.GazeTan[0].X, TanVL = Info.GazeTan[0].Y;
    const float TanHR = Info.GazeTan[1].X, TanVR = Info.GazeTan[1].Y;
    if (!FMath::IsFinite(TanHL) || !FMath::IsFinite(TanVL) || !FMath::IsFinite(TanHR) ||
        !FMath::IsFinite(TanVR) || FMath::Abs(TanHL) > MaxPlausibleGazeTan ||
        FMath::Abs(TanVL) > MaxPlausibleGazeTan || FMath::Abs(TanHR) > MaxPlausibleGazeTan ||
        FMath::Abs(TanVR) > MaxPlausibleGazeTan)
    {
        return false;
    }

    // PVR is right-handed with -Z forward: dir = (sinH*cosV, sinV, -cosH*cosV).
    // Converted to UE4 camera-local left-handed (X forward, Y right, Z up):
    auto ToEye = [&](float TanH, float TanV, DReyeVR::FEyeGaze &Eye) {
        float H = FMath::Atan(TanH), V = FMath::Atan(TanV);
        if (Config.bInvertHorizontal)
            H = -H;
        if (Config.bInvertVertical)
            V = -V;
        const float CosH = FMath::Cos(H), SinH = FMath::Sin(H);
        const float CosV = FMath::Cos(V), SinV = FMath::Sin(V);
        Eye.Direction = FVector(CosH * CosV, SinH * CosV, SinV).GetSafeNormal();
        Eye.HRad = H;
        Eye.VRad = V;
        Eye.bValid = true;
    };
    ToEye(TanHL, TanVL, Out.Left);
    ToEye(TanHR, TanVR, Out.Right);

    // Cyclopean direction stays the mean of the tangents, as before this refactor.
    float H = FMath::Atan(0.5f * (TanHL + TanHR));
    float V = FMath::Atan(0.5f * (TanVL + TanVR));
    if (Config.bInvertHorizontal)
        H = -H;
    if (Config.bInvertVertical)
        V = -V;
    const float CosH = FMath::Cos(H), SinH = FMath::Sin(H);
    const float CosV = FMath::Cos(V), SinV = FMath::Sin(V);
    Out.LocalDirection = FVector(CosH * CosV, SinH * CosV, SinV).GetSafeNormal();
    return true;
}
/// Poll getHmdStatus once per N eye-tracking polls (~1 Hz at 60 fps).
constexpr uint64 StatusCheckInterval = 60;
} // namespace

/// ---------------------------------------------------------------------------
/// Background sampling thread. Polls on a fixed wall-clock grid so the gaze
/// stream keeps the tracker's native density even when the render loop runs far
/// slower (measured 30 fps in VR against a 120 Hz tracker on Dream Air).
/// ---------------------------------------------------------------------------
class FPimaxPvrEyeSource::FAsyncPoller final : public FRunnable
{
  public:
    /// Backlog cap: ~17 s at 120 Hz before the oldest sample is dropped.
    static constexpr int32 MaxBufferedSamples = 2048;

    FAsyncPoller(FPimaxPvrEyeSource &InOwner, double InHz)
        : Owner(InOwner), PeriodSeconds(1.0 / InHz),
          Timer(CreateWaitableTimerExW(nullptr, nullptr, CREATE_WAITABLE_TIMER_HIGH_RESOLUTION, TIMER_ALL_ACCESS))
    {
        if (Timer == nullptr)
            Timer = CreateWaitableTimerExW(nullptr, nullptr, 0, TIMER_ALL_ACCESS);
    }

    ~FAsyncPoller() override
    {
        if (Thread != nullptr)
            StopAndJoin();
        if (Timer != nullptr)
            CloseHandle(Timer);
    }

    void Start()
    {
        check(Thread == nullptr);
        Thread = FRunnableThread::Create(this, TEXT("PimaxEyePoller"), 0, TPri_AboveNormal);
    }

    /// Not FRunnable::Stop(): Kill(true) calls that virtual from the dying thread, so
    /// sharing the name made Stop() re-enter Kill() until the stack overflowed.
    void StopAndJoin()
    {
        bStop.store(true, std::memory_order_relaxed);
        if (Thread != nullptr)
        {
            Thread->Kill(true); // the loop notices bStop within one poll period
            delete Thread;
            Thread = nullptr;
        }
    }

    /// Hand every buffered sample to the game thread, oldest first.
    TArray<DReyeVR::FEyeSample> Drain()
    {
        FScopeLock Lock(&BufferCS);
        TArray<DReyeVR::FEyeSample> Out(MoveTemp(Buffer));
        Buffer.Reset();
        return Out;
    }

  private:
    uint32 Run() override
    {
        const double Start = FPlatformTime::Seconds();
        uint64 Tick = 0;
        while (!bStop.load(std::memory_order_relaxed))
        {
            // absolute schedule: one late poll never shifts the grid
            WaitUntil(Start + static_cast<double>(++Tick) * PeriodSeconds);
            if (bStop.load(std::memory_order_relaxed))
                break;
            DReyeVR::FEyeSample Sample;
            Sample.ReceivedTime = FPlatformTime::Seconds();
            {
                FScopeLock Lock(&Owner.RuntimeCS);
                if (Owner.State.load() == EState::Running)
                {
                    Sample.Sequence = ++Owner.SampleSequence;
                    Owner.PollRunningLocked(Sample, Sample.ReceivedTime);
                }
            }
            if (!Sample.bValid)
                continue;
            FScopeLock Lock(&BufferCS);
            Buffer.Add(Sample);
            if (Buffer.Num() > MaxBufferedSamples)
            {
                Buffer.RemoveAt(0);
                if ((++DroppedSamples % 240) == 1) // warn once per 2 s of backlog at 120 Hz
                    LOG_WARN("Pimax PVR: gaze sample backlog over %d; dropping the oldest samples", MaxBufferedSamples);
            }
        }
        return 0;
    }

    void WaitUntil(double TargetSeconds) const
    {
        // CREATE_WAITABLE_TIMER_HIGH_RESOLUTION (Win10 1803+) is accurate well below a
        // millisecond, which Sleep() is not. Without it the coarse timer keeps a
        // 1.5 ms margin and busy-waits the remainder.
        for (;;)
        {
            const double Remaining = TargetSeconds - FPlatformTime::Seconds();
            if (Remaining <= 0.0)
                return;
            LARGE_INTEGER Due;
            Due.QuadPart = -static_cast<LONGLONG>(Remaining * 1e7);
            if (Timer == nullptr || !SetWaitableTimer(Timer, &Due, 0, nullptr, nullptr, 0))
            {
                if (Remaining > 0.0015)
                    FPlatformProcess::Sleep(Remaining - 0.0015);
                continue;
            }
            WaitForSingleObject(Timer, INFINITE);
        }
    }

    FPimaxPvrEyeSource &Owner;
    double PeriodSeconds;
    FRunnableThread *Thread = nullptr;
    std::atomic<bool> bStop{false};
    FCriticalSection BufferCS;
    TArray<DReyeVR::FEyeSample> Buffer;
    uint64 DroppedSamples = 0;
    HANDLE Timer = nullptr;
};

FPimaxPvrEyeSource::FPimaxPvrEyeSource(const FConfig &InConfig) : Config(InConfig)
{
}

FPimaxPvrEyeSource::~FPimaxPvrEyeSource()
{
    Shutdown();
}

void FPimaxPvrEyeSource::TransitionTo(EState NewState, const TCHAR *Reason)
{
    if (State.load() == NewState)
        return;
    LOG("Pimax PVR eye source: state %d -> %d (%s)", (int32)State.load(), (int32)NewState, Reason);
    State = NewState;
}

void FPimaxPvrEyeSource::ReleaseRuntimeImpl()
{
    if (InterfaceSlots != nullptr)
    {
        if (HmdHandle != nullptr)
        {
            reinterpret_cast<PvrAbi::DestroyHmdFn>(InterfaceSlots[PvrAbi::Slot_DestroyHmd])(HmdHandle);
            HmdHandle = nullptr;
        }
        if (bRuntimeInitialised)
        {
            reinterpret_cast<PvrAbi::ShutdownFn>(InterfaceSlots[PvrAbi::Slot_Shutdown])();
            bRuntimeInitialised = false;
        }
        InterfaceSlots = nullptr;
    }
    if (DllHandle != nullptr)
    {
        FreeLibrary((HMODULE)DllHandle);
        DllHandle = nullptr;
    }
}

bool FPimaxPvrEyeSource::Initialize()
{
    FScopeLock Lock(&RuntimeCS);
    if (State.load() == EState::Running)
        return true;
    return InitializeImpl();
}

bool FPimaxPvrEyeSource::InitializeImpl()
{
    TransitionTo(EState::Initializing, TEXT("Initialize"));

    // 1. Discover the official installation without assuming a Windows drive letter.
    for (const FString &Candidate : FindPvrDllCandidates())
    {
        DllHandle = LoadLibraryExW(*Candidate, NULL, LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_SYSTEM32);
        if (DllHandle != nullptr)
        {
            LOG("Pimax PVR: loaded %s", *Candidate);
            break;
        }
        LOG_WARN("Pimax PVR: cannot load %s (Windows error %lu)", *Candidate, GetLastError());
    }
    if (DllHandle == nullptr)
    {
        LOG_ERROR("Pimax PVR: install/repair official Pimax Play and start its runtime. "
                  "For a custom installation set PIMAX_PVR_DLL to the absolute path of libPVRClient64.dll. "
                  "Copying the DLL alone does not install the required drivers or eye-tracking services.");
        TransitionTo(EState::Failed, TEXT("dll not found"));
        return false;
    }

    // 2. Fetch the v1.26 function table.
    FARPROC GetPvrInterfaceProc = GetProcAddress((HMODULE)DllHandle, "getPvrInterface");
    if (GetPvrInterfaceProc == nullptr)
    {
        LOG_ERROR("Pimax PVR: getPvrInterface export missing");
        ReleaseRuntimeImpl();
        TransitionTo(EState::Failed, TEXT("getPvrInterface missing"));
        return false;
    }
#pragma warning(push)
#pragma warning(disable : 4191) // FARPROC -> function pointer is the intended GetProcAddress usage
    void *Table = reinterpret_cast<PvrAbi::GetPvrInterfaceFn>(GetPvrInterfaceProc)(PvrInterfaceMajor, PvrInterfaceMinor);
#pragma warning(pop)
    if (Table == nullptr)
    {
        LOG_ERROR("Pimax PVR: runtime does not provide interface v%u.%u", PvrInterfaceMajor, PvrInterfaceMinor);
        ReleaseRuntimeImpl();
        TransitionTo(EState::Failed, TEXT("interface v1.26 unavailable"));
        return false;
    }
    InterfaceSlots = static_cast<void **>(Table);

    // sanity-check every slot we rely on
    constexpr int32 RequiredSlots[] = {PvrAbi::Slot_Initialise, PvrAbi::Slot_Shutdown,         PvrAbi::Slot_CreateHmd,
                                       PvrAbi::Slot_DestroyHmd, PvrAbi::Slot_GetVersionString, PvrAbi::Slot_GetTimeSeconds,
                                       PvrAbi::Slot_GetHmdInfo, PvrAbi::Slot_GetHmdStatus,     PvrAbi::Slot_GetEyeTrackingInfo};
    for (const int32 Slot : RequiredSlots)
    {
        if (InterfaceSlots[Slot] == nullptr)
        {
            LOG_ERROR("Pimax PVR: interface slot %d is null", Slot);
            ReleaseRuntimeImpl();
            TransitionTo(EState::Failed, TEXT("null interface slot"));
            return false;
        }
    }

    // 3. Bring up the client session.
    if (reinterpret_cast<PvrAbi::InitialiseFn>(InterfaceSlots[PvrAbi::Slot_Initialise])() != PvrAbi::Success)
    {
        LOG_ERROR("Pimax PVR: initialise failed (is the Pimax runtime running?)");
        ReleaseRuntimeImpl();
        TransitionTo(EState::Failed, TEXT("initialise failed"));
        return false;
    }
    bRuntimeInitialised = true;

    const char *Version = reinterpret_cast<PvrAbi::GetVersionStringFn>(InterfaceSlots[PvrAbi::Slot_GetVersionString])();
    const FString VersionStr = Version != nullptr ? FString(UTF8_TO_TCHAR(Version)) : TEXT("<unknown>");
    LOG("Pimax PVR: runtime version %s", *VersionStr);

    if (reinterpret_cast<PvrAbi::CreateHmdFn>(InterfaceSlots[PvrAbi::Slot_CreateHmd])(&HmdHandle) != PvrAbi::Success ||
        HmdHandle == nullptr)
    {
        LOG_ERROR("Pimax PVR: createHmd failed (no HMD connected?)");
        ReleaseRuntimeImpl();
        TransitionTo(EState::Failed, TEXT("createHmd failed"));
        return false;
    }

    if (!ValidateDevice())
    {
        ReleaseRuntimeImpl();
        TransitionTo(EState::Failed, TEXT("device rejected"));
        return false;
    }

    ConsecutiveFailures = 0;
    bHasLastSourceTime = false;
    bHasFrozenBaseline = false; // a new session has no previous distinct angle to repeat
    InitBackoffSeconds = 1.0;
    TransitionTo(EState::Running, TEXT("ready"));
    StartPoller();
    return true;
}

bool FPimaxPvrEyeSource::ValidateDevice()
{
    PvrAbi::FPvrHmdInfo Info;
    FMemory::Memzero(Info);
    if (reinterpret_cast<PvrAbi::GetHmdInfoFn>(InterfaceSlots[PvrAbi::Slot_GetHmdInfo])(HmdHandle, &Info) !=
        PvrAbi::Success)
    {
        LOG_ERROR("Pimax PVR: getHmdInfo failed");
        return false;
    }
    // strings from the runtime may not be null-terminated if they fill the buffer
    Info.ProductName[sizeof(Info.ProductName) - 1] = '\0';
    Info.Manufacturer[sizeof(Info.Manufacturer) - 1] = '\0';
    Info.SerialNumber[sizeof(Info.SerialNumber) - 1] = '\0';
    LOG("Pimax PVR: device \"%s\" by \"%s\", VID=0x%04x PID=0x%04x, fw %d.%d, %dx%d", UTF8_TO_TCHAR(Info.ProductName),
        UTF8_TO_TCHAR(Info.Manufacturer), Info.VendorId, Info.ProductId, Info.FirmwareMajor, Info.FirmwareMinor,
        Info.ResolutionW, Info.ResolutionH);

    if (Config.bAllowUnknownPimaxDevice)
        return true;

    if (!Config.AllowedVendorIds.Contains(Info.VendorId) || !Config.AllowedProductIds.Contains(Info.ProductId))
    {
        LOG_ERROR("Pimax PVR: VID 0x%04x / PID 0x%04x not in the allowlist (set AllowUnknownPimaxDevice=True in "
                  "DReyeVRConfig.ini [Pimax] to override)",
                  Info.VendorId, Info.ProductId);
        return false;
    }
    return true;
}

void FPimaxPvrEyeSource::RecoverIfNeeded()
{
    // bounded exponential backoff before attempting to bring the session back
    if (State.load() != EState::Failed)
        return;
    FScopeLock Lock(&RuntimeCS);
    if (State.load() != EState::Failed)
        return;
    const double Now = FPlatformTime::Seconds();
    if (Now < NextInitRetryHostTime)
        return;
    LOG_WARN("Pimax PVR: retrying initialization (backoff was %.1fs)", InitBackoffSeconds);
    ReleaseRuntimeImpl();
    if (InitializeImpl())
    {
        InitBackoffSeconds = 1.0;
    }
    else
    {
        NextInitRetryHostTime = Now + InitBackoffSeconds;
        InitBackoffSeconds = FMath::Min(InitBackoffSeconds * 2.0, 30.0);
    }
}

DReyeVR::FEyeSample FPimaxPvrEyeSource::PollLatest()
{
    DReyeVR::FEyeSample Sample; // invalid by default
    Sample.ReceivedTime = FPlatformTime::Seconds();
    Sample.Sequence = ++SampleSequence;
    const double Now = Sample.ReceivedTime;

    RecoverIfNeeded();
    if (State.load() != EState::Running)
        return Sample;

    FScopeLock Lock(&RuntimeCS);
    PollRunningLocked(Sample, Now);
    return Sample;
}

void FPimaxPvrEyeSource::PollRunningLocked(DReyeVR::FEyeSample &Sample, double Now)
{
    auto GetTimeSeconds = reinterpret_cast<PvrAbi::GetTimeSecondsFn>(InterfaceSlots[PvrAbi::Slot_GetTimeSeconds]);
    auto GetEyeTrackingInfo = reinterpret_cast<PvrAbi::GetEyeTrackingInfoFn>(InterfaceSlots[PvrAbi::Slot_GetEyeTrackingInfo]);

    PvrAbi::FPvrEyeTrackingInfo Info;
    FMemory::Memzero(Info);
    const PvrAbi::FPvrResult Result = GetEyeTrackingInfo(HmdHandle, GetTimeSeconds(), &Info);
    LastResult = Result;
    LastTimeInSeconds = Info.TimeInSeconds;

    const char *InvalidReason = nullptr; // why the sample is invalid (for diagnostics)

    if (Result != PvrAbi::Success)
    {
        ++ConsecutiveFailures;
        InvalidReason = "call-failed";
        if (ConsecutiveFailures >= MaxConsecutiveFailures)
        {
            NextInitRetryHostTime = Now + InitBackoffSeconds;
            TransitionTo(EState::Failed, TEXT("repeated poll failure"));
        }
    }
    else
    {
        ConsecutiveFailures = 0;

        // the device must actually be present and worn for data to be meaningful
        if ((SampleSequence.load(std::memory_order_relaxed) % StatusCheckInterval) == 1)
        {
            PvrAbi::FPvrHmdStatus Status;
            FMemory::Memzero(Status);
            auto GetHmdStatus = reinterpret_cast<PvrAbi::GetHmdStatusFn>(InterfaceSlots[PvrAbi::Slot_GetHmdStatus]);
            if (GetHmdStatus(HmdHandle, &Status) == PvrAbi::Success)
            {
                LastHmdPresent = Status.HmdPresent;
                LastHmdMounted = Status.HmdMounted;
            }
        }
        if (LastHmdPresent == 0 || LastHmdMounted == 0)
        {
            InvalidReason = "not-mounted";
        }
        else if (Info.TimeInSeconds <= 0.0)
        {
            InvalidReason = "timestamp-zero"; // tracker produces no data yet (e.g. not calibrated)
        }
        else
        {
            // staleness: the device timestamp must keep advancing
            if (!bHasLastSourceTime || Info.TimeInSeconds != LastSourceTime)
            {
                LastSourceTime = Info.TimeInSeconds;
                LastSourceTimeSeenHost = Now;
                bHasLastSourceTime = true;
            }
            // New timestamps must also reach gaze conversion below. An
            // else-if here would silently discard every advancing sample.
            if (Now - LastSourceTimeSeenHost > (double)Config.MaxSampleAgeSeconds)
            {
                InvalidReason = "stale";
            }
            else if (!ApplyGazeInfo(Info, Config, Sample))
            {
                InvalidReason = "implausible-values";
            }
            else if (IsFrozenValue(Info.GazeTan[0].X, Info.GazeTan[0].Y, Info.GazeTan[1].X, Info.GazeTan[1].Y,
                                    Info.TimeInSeconds))
            {
                InvalidReason = "frozen-value";
                ++DiagFrozenSamples;
            }
            else
            {
                // PVR exposes no eye origin, so gaze is treated as starting at the camera.
                Sample.LocalOrigin = FVector::ZeroVector;
                Sample.SourceTime = Info.TimeInSeconds;
                Sample.bSourceTimeKnown = true;
                Sample.bValid = true;
            }
        }
    }

    // diagnostics: one summary line every 5 s, plus the first valid sample ever
    if (Sample.bValid)
    {
        ++DiagValidCount;
        if (!bEverValid)
        {
            bEverValid = true;
            LOG("Pimax PVR: first valid eye sample (poll #%llu, timeInSec=%.3f, dir=%s)",
                static_cast<unsigned long long>(SampleSequence.load(std::memory_order_relaxed)),
                Info.TimeInSeconds, *Sample.LocalDirection.ToString());
        }
    }
    else
    {
        ++DiagInvalidCount;
    }
    if (Now - LastDiagHostTime >= 5.0)
    {
        LastDiagHostTime = Now;
        // samples/frame == 1 means this frame got no fresher reading than the last one;
        // > 1 only happens with AsyncPollHz, where the poll thread fills the buffer.
        const uint64 PollAllCalls = DiagPollAllCalls.exchange(0, std::memory_order_relaxed);
        const uint64 SamplesCollected = DiagSamplesCollected.exchange(0, std::memory_order_relaxed);
        LOG("Pimax PVR diag: valid=%llu invalid=%llu frozen=%llu lastResult=%d timeInSec=%.3f present=%d mounted=%d "
            "age=%.3f reason=%s polls=%llu samplesPerPoll=%.2f",
            DiagValidCount, DiagInvalidCount, DiagFrozenSamples, LastResult, LastTimeInSeconds, LastHmdPresent,
            LastHmdMounted,
            bHasLastSourceTime ? (float)(Now - LastSourceTimeSeenHost) : -1.f,
            InvalidReason != nullptr ? ANSI_TO_TCHAR(InvalidReason) : TEXT("none"),
            static_cast<unsigned long long>(PollAllCalls),
            PollAllCalls ? (double)SamplesCollected / PollAllCalls : 0.0);
        DiagValidCount = 0;
        DiagInvalidCount = 0;
        DiagFrozenSamples = 0;
    }
}

bool FPimaxPvrEyeSource::IsFrozenValue(float TanHL, float TanVL, float TanHR, float TanVR, double DeviceTimeS)
{
    const bool bMoved = !bHasFrozenBaseline || TanHL != LastDistinctTan[0] || TanVL != LastDistinctTan[1] ||
                        TanHR != LastDistinctTan[2] || TanVR != LastDistinctTan[3];
    if (bMoved)
    {
        LastDistinctTan[0] = TanHL;
        LastDistinctTan[1] = TanVL;
        LastDistinctTan[2] = TanHR;
        LastDistinctTan[3] = TanVR;
        LastDistinctChangeTime = DeviceTimeS;
        bHasFrozenBaseline = true;
        return false;
    }
    // The tracker clock moved but the eye did not. Measured repeat runs inside normal
    // activity reach p99 = 11-14 samples, so this only fires on the tens-of-seconds holds.
    return Config.MaxFrozenSeconds > 0.0 && DeviceTimeS - LastDistinctChangeTime > Config.MaxFrozenSeconds;
}

TArray<DReyeVR::FEyeSample> FPimaxPvrEyeSource::PollAllAvailable()
{
    if (Poller != nullptr)
    {
        // Async mode: the poll thread owns every PVR call; this frame only drains what
        // it collected. The recovery state machine stays identical to the sync path.
        RecoverIfNeeded();
        TArray<DReyeVR::FEyeSample> Out = Poller->Drain();
        DiagPollAllCalls.fetch_add(1, std::memory_order_relaxed); // samplesPerPoll measures stream density vs the frame rate
        DiagSamplesCollected.fetch_add((uint64)Out.Num(), std::memory_order_relaxed);
        if (Out.Num() == 0)
        {
            DReyeVR::FEyeSample Empty; // callers use Last() unconditionally
            Empty.ReceivedTime = FPlatformTime::Seconds();
            Empty.Sequence = ++SampleSequence;
            Out.Add(Empty);
        }
        return Out;
    }

    // One sample per frame. Probing getEyeTrackingInfo() at earlier device times to recover
    // the samples a slow frame skipped was measured not to work: PVR runtime 1.33.1 ignores the
    // AbsTime argument and re-serves the newest sample (reproduced with a standalone probe
    // against the same handle), so the probes cost cross-DLL calls and never added a row.
    // FConfig::AsyncPollHz is what keeps the stream at 120 Hz under a slower render loop.
    TArray<DReyeVR::FEyeSample> Out;
    Out.Add(PollLatest());
    DiagPollAllCalls.fetch_add(1, std::memory_order_relaxed);
    DiagSamplesCollected.fetch_add(1, std::memory_order_relaxed);
    return Out;
}

void FPimaxPvrEyeSource::StartPoller()
{
    if (Poller != nullptr || !(Config.AsyncPollHz > 0.0))
        return;
    Poller = new FAsyncPoller(*this, Config.AsyncPollHz);
    Poller->Start();
    LOG("Pimax PVR: background sampling thread at %.0f Hz", Config.AsyncPollHz);
}

void FPimaxPvrEyeSource::StopPoller()
{
    if (Poller == nullptr)
        return;
    Poller->StopAndJoin(); // join before touching the runtime the thread is polling
    delete Poller;
    Poller = nullptr;
}

void FPimaxPvrEyeSource::Shutdown()
{
    StopPoller();
    {
        FScopeLock Lock(&RuntimeCS);
        ReleaseRuntimeImpl();
    }
    TransitionTo(EState::Disabled, TEXT("Shutdown"));
}

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPimaxPvrFreshnessTest, "HUTB.Pimax.EyeTracking.Freshness",
                                EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPimaxPvrFreshnessTest::RunTest(const FString &Parameters)
{
    // Exercise the production PollLatest() with a fake PVR table. No headset,
    // Pimax DLL, service settings, or real device session is used by this test.
    struct FFrame
    {
        PvrAbi::FPvrEyeTrackingInfo Info{};
        PvrAbi::FPvrResult Result = PvrAbi::Success;
    } Frame;
    void *Slots[PvrAbi::Slot_Count] = {};
    PvrAbi::GetTimeSecondsFn Clock = +[]() -> double { return 0.0; };
    PvrAbi::GetEyeTrackingInfoFn Read =
        +[](PvrAbi::FPvrHmdHandle Handle, double, PvrAbi::FPvrEyeTrackingInfo *Out) -> PvrAbi::FPvrResult {
        const FFrame &Current = *static_cast<FFrame *>(Handle);
        *Out = Current.Info;
        return Current.Result;
    };
    PvrAbi::GetHmdStatusFn Status =
        +[](PvrAbi::FPvrHmdHandle, PvrAbi::FPvrHmdStatus *Out) -> PvrAbi::FPvrResult {
        FMemory::Memzero(*Out);
        Out->HmdPresent = Out->HmdMounted = Out->ServiceReady = 1;
        return PvrAbi::Success;
    };
    PvrAbi::DestroyHmdFn Destroy = +[](PvrAbi::FPvrHmdHandle) {};
    Slots[PvrAbi::Slot_GetTimeSeconds] = reinterpret_cast<void *>(Clock);
    Slots[PvrAbi::Slot_GetEyeTrackingInfo] = reinterpret_cast<void *>(Read);
    Slots[PvrAbi::Slot_GetHmdStatus] = reinterpret_cast<void *>(Status);
    Slots[PvrAbi::Slot_DestroyHmd] = reinterpret_cast<void *>(Destroy);

    FPimaxPvrEyeSource::FConfig Cfg;
    Cfg.MaxFrozenSeconds = 0.0; // this test is about the timestamp gates; the frozen-value gate has its own
    FPimaxPvrEyeSource Source{Cfg};
    Source.InterfaceSlots = Slots;
    Source.HmdHandle = &Frame;
    Source.State = FPimaxPvrEyeSource::EState::Running;
    Frame.Info.TimeInSeconds = 1.0;
    Frame.Info.GazeTan[0].X = Frame.Info.GazeTan[1].X = 0.25f;
    const auto First = Source.PollLatest();
    TestTrue(TEXT("First fresh sample is usable"), First.bValid);
    TestTrue(TEXT("Fresh sample carries source time"), First.bSourceTimeKnown && First.SourceTime == 1.0);
    TestTrue(TEXT("Direction is normalized and points right"),
             First.LocalDirection.IsNormalized() && First.LocalDirection.Y > 0.f);

    Frame.Info.TimeInSeconds = 2.0;
    Frame.Info.GazeTan[0].X = Frame.Info.GazeTan[1].X = -0.25f;
    const auto Advancing = Source.PollLatest();
    TestTrue(TEXT("Advancing timestamp is usable"), Advancing.bValid);
    TestTrue(TEXT("New direction replaces the previous direction"), Advancing.LocalDirection.Y < 0.f);
    TestTrue(TEXT("Repeated timestamp inside freshness window is usable"), Source.PollLatest().bValid);

    Source.LastSourceTimeSeenHost = FPlatformTime::Seconds() - Source.Config.MaxSampleAgeSeconds - 1.0;
    TestFalse(TEXT("Repeated timestamp outside freshness window is rejected"), Source.PollLatest().bValid);
    Frame.Info.TimeInSeconds = 3.0;
    TestTrue(TEXT("A fresh sample recovers immediately after staleness"), Source.PollLatest().bValid);

    Frame.Info.TimeInSeconds = 0.0;
    TestFalse(TEXT("Zero timestamp remains invalid"), Source.PollLatest().bValid);
    Frame.Info.TimeInSeconds = 4.0;
    Frame.Result = 1;
    TestFalse(TEXT("A failed PVR call remains invalid"), Source.PollLatest().bValid);
    Frame.Result = PvrAbi::Success;
    Frame.Info.GazeTan[0].X = 1000.f;
    TestFalse(TEXT("Implausible gaze remains invalid"), Source.PollLatest().bValid);
    return true;
}
#endif // WITH_DEV_AUTOMATION_TESTS

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPimaxPvrFrozenValueTest, "HUTB.Pimax.EyeTracking.FrozenValue",
                                EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPimaxPvrFrozenValueTest::RunTest(const FString &Parameters)
{
    // Same fake PVR table as the freshness test, no headset and no Pimax runtime: here the
    // device timestamp keeps advancing on the measured grid while the angles stay
    // bit-identical, which is the failure the staleness gate cannot see (three sessions lost
    // 6.3% / 15.6% / 43.8% of their samples to it).
    struct FFrame
    {
        PvrAbi::FPvrEyeTrackingInfo Info{};
        PvrAbi::FPvrResult Result = PvrAbi::Success;
    } Frame;
    void *Slots[PvrAbi::Slot_Count] = {};
    Slots[PvrAbi::Slot_GetTimeSeconds] = reinterpret_cast<void *>(+[]() -> double { return 0.0; });
    Slots[PvrAbi::Slot_GetEyeTrackingInfo] =
        reinterpret_cast<void *>(+[](PvrAbi::FPvrHmdHandle Handle, double, PvrAbi::FPvrEyeTrackingInfo *Out)
                                 -> PvrAbi::FPvrResult {
            const FFrame &Current = *static_cast<FFrame *>(Handle);
            *Out = Current.Info;
            return Current.Result;
        });
    Slots[PvrAbi::Slot_GetHmdStatus] =
        reinterpret_cast<void *>(+[](PvrAbi::FPvrHmdHandle, PvrAbi::FPvrHmdStatus *Out) -> PvrAbi::FPvrResult {
            FMemory::Memzero(*Out);
            Out->HmdPresent = Out->HmdMounted = Out->ServiceReady = 1;
            return PvrAbi::Success;
        });
    Slots[PvrAbi::Slot_DestroyHmd] = reinterpret_cast<void *>(+[](PvrAbi::FPvrHmdHandle) {});

    constexpr double Grid = 1.0 / 120.0;
    FPimaxPvrEyeSource::FConfig Cfg; // the calibrated 250 ms window
    FPimaxPvrEyeSource Source{Cfg};
    Source.InterfaceSlots = Slots;
    Source.HmdHandle = &Frame;
    Source.State = FPimaxPvrEyeSource::EState::Running;

    Frame.Info.TimeInSeconds = 1.0;
    Frame.Info.GazeTan[0] = {0.25f, 0.1f};
    Frame.Info.GazeTan[1] = {0.25f, 0.1f};
    TestTrue(TEXT("The first sample of a session is valid"), Source.PollLatest().bValid);

    int32 Valid = 0, Invalid = 0;
    for (int32 Step = 1; Step <= 30; ++Step) // 250 ms of one identical reading, clock moving
    {
        Frame.Info.TimeInSeconds = 1.0 + Step * Grid;
        if (Source.PollLatest().bValid) ++Valid; else ++Invalid;
    }
    TestEqual(TEXT("A hold up to the window is still recorded"), Valid, 30);
    TestEqual(TEXT("Nothing rejected inside the window"), Invalid, 0);

    bool bAllRejected = true;
    for (int32 Step = 31; Step <= 60; ++Step) // the measured holds run for seconds
    {
        Frame.Info.TimeInSeconds = 1.0 + Step * Grid;
        bAllRejected = bAllRejected && !Source.PollLatest().bValid;
    }
    TestTrue(TEXT("A bit-identical hold past the window is rejected"), bAllRejected);
    TestTrue(TEXT("The freeze is counted for the diagnostic line"), Source.DiagFrozenSamples > 0);

    // The rejected stretch must not look like data: no source time means the recorder skips
    // it, exactly as the other gates behave.
    const auto Frozen = Source.PollLatest();
    TestFalse(TEXT("A frozen sample does not claim a source time"), Frozen.bSourceTimeKnown);

    Frame.Info.TimeInSeconds = 1.0 + 61 * Grid;
    Frame.Info.GazeTan[0].X = 0.251f; // moved by well under a tenth of a degree
    TestTrue(TEXT("The next changed angle is valid again"), Source.PollLatest().bValid);
    bool bRecovered = true;
    for (int32 Step = 62; Step <= 71; ++Step)
    {
        Frame.Info.TimeInSeconds = 1.0 + Step * Grid;
        bRecovered = bRecovered && Source.PollLatest().bValid;
    }
    TestTrue(TEXT("A new hold starts from the moved angle, not the old one"), bRecovered);

    // MaxFrozenSeconds = 0 disables the gate: a multi-second hold stays valid, so the check
    // can be switched off without a rebuild if a future tracker reports genuinely frozen eyes.
    FPimaxPvrEyeSource::FConfig Off;
    Off.MaxFrozenSeconds = 0.0;
    FPimaxPvrEyeSource Disabled{Off};
    Disabled.InterfaceSlots = Slots;
    Disabled.HmdHandle = &Frame;
    Disabled.State = FPimaxPvrEyeSource::EState::Running;
    Frame.Info.TimeInSeconds = 5.0;
    bool bDisabledAllValid = Disabled.PollLatest().bValid;
    for (int32 Step = 1; Step <= 240; ++Step) // 2 s of one identical reading
    {
        Frame.Info.TimeInSeconds = 5.0 + Step * Grid;
        bDisabledAllValid = bDisabledAllValid && Disabled.PollLatest().bValid;
    }
    TestTrue(TEXT("With MaxFrozenSeconds = 0 a multi-second hold stays valid"), bDisabledAllValid);
    return true;
}
#endif // WITH_DEV_AUTOMATION_TESTS

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPimaxPvrAsyncThreadTest, "HUTB.Pimax.EyeTracking.AsyncThread",
                                EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPimaxPvrAsyncThreadTest::RunTest(const FString &Parameters)
{
    // The real runtime only ever serves the newest sample, so the async thread is the
    // only way to keep 120 Hz with a 30 fps render loop. The fake tracker below moves
    // on a host-clock-driven 120 Hz device grid and needs no headset.
    void *Slots[PvrAbi::Slot_Count] = {};
    Slots[PvrAbi::Slot_GetTimeSeconds] =
        reinterpret_cast<void *>(+[]() -> double { return FPlatformTime::Seconds(); });
    Slots[PvrAbi::Slot_GetEyeTrackingInfo] = reinterpret_cast<void *>(
        +[](PvrAbi::FPvrHmdHandle, double, PvrAbi::FPvrEyeTrackingInfo *Out) -> PvrAbi::FPvrResult {
            const double Grid = 1.0 / 120.0;
            Out->TimeInSeconds = FMath::FloorToDouble(FPlatformTime::Seconds() / Grid) * Grid;
            Out->GazeTan[0] = {0.25f, 0.f};
            Out->GazeTan[1] = {0.25f, 0.f};
            return PvrAbi::Success;
        });
    Slots[PvrAbi::Slot_GetHmdStatus] =
        reinterpret_cast<void *>(+[](PvrAbi::FPvrHmdHandle, PvrAbi::FPvrHmdStatus *Out) -> PvrAbi::FPvrResult {
            FMemory::Memzero(*Out);
            Out->HmdPresent = Out->HmdMounted = Out->ServiceReady = 1;
            return PvrAbi::Success;
        });
    Slots[PvrAbi::Slot_DestroyHmd] = reinterpret_cast<void *>(+[](PvrAbi::FPvrHmdHandle) {});

    FPimaxPvrEyeSource::FConfig Cfg;
    Cfg.AsyncPollHz = 500.0; // poll faster than the device grid; duplicates are deduped downstream
    FPimaxPvrEyeSource Source{Cfg};
    Source.InterfaceSlots = Slots;
    Source.HmdHandle = reinterpret_cast<PvrAbi::FPvrHmdHandle>(0x1);
    Source.State = FPimaxPvrEyeSource::EState::Running;
    Source.StartPoller();
    FPlatformProcess::Sleep(0.3); // ~36 device grid points

    const auto First = Source.PollAllAvailable();
    TestTrue(TEXT("The poll thread collected samples well above one per frame"), First.Num() >= 10);
    bool bAscending = true;
    bool bSeqStrictlyIncreasing = First.Num() > 0 && First[0].Sequence > 0;
    for (int32 i = 1; i < First.Num(); ++i)
    {
        bAscending = bAscending && First[i].SourceTime >= First[i - 1].SourceTime;
        bSeqStrictlyIncreasing = bSeqStrictlyIncreasing && First[i].Sequence > First[i - 1].Sequence;
    }
    TestTrue(TEXT("Samples are ordered by device time, oldest first"), bAscending);
    TestTrue(TEXT("Sample.Sequence is strictly increasing across drained samples"), bSeqStrictlyIncreasing);
    TestTrue(TEXT("Samples keep per-eye data"),
             First.Num() > 0 && First.Last().bValid && First.Last().Left.bValid && First.Last().Right.bValid);

    // A drain must hand out only what arrived after the previous drain, and sequences
    // must stay strictly increasing across drains (including empty-drain fallback).
    const auto Second = Source.PollAllAvailable();
    TestTrue(TEXT("A drain does not repeat samples it already returned"), Second.Num() < First.Num());
    {
        FScopeLock Lock(&Source.RuntimeCS);
        Source.State = FPimaxPvrEyeSource::EState::Disabled; // freeze poller additions so the next drain is guaranteed empty
    }
    FPlatformProcess::Sleep(0.005); // let any in-flight poller iteration finish pushing to Buffer
    const auto PreEmptyDrain = Source.PollAllAvailable();
    const auto EmptyDrain = Source.PollAllAvailable();
    TestEqual(TEXT("Empty drain returns one invalid fallback sample"), EmptyDrain.Num(), 1);
    TestFalse(TEXT("Empty-drain fallback sample is marked invalid"), EmptyDrain[0].bValid);
    TestTrue(TEXT("Sample.Sequence strictly increases across drains and empty-drain fallback"),
             First.Num() > 0 && Second.Num() > 0 && PreEmptyDrain.Num() > 0 &&
                 Second[0].Sequence > First.Last().Sequence &&
                 PreEmptyDrain.Last().Sequence >= Second.Last().Sequence &&
                 EmptyDrain[0].Sequence > PreEmptyDrain.Last().Sequence);

    // Shutdown must join the thread before releasing the (fake) runtime.
    Source.Shutdown();
    return true;
}
#endif // WITH_DEV_AUTOMATION_TESTS

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPimaxRuntimePathsTest, "HUTB.Pimax.Runtime.DiscoveryPaths",
                                EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FPimaxRuntimePathsTest::RunTest(const FString &Parameters)
{
    const auto Paths = PimaxRuntimePaths::Build(TEXT(""), TEXT("D:/Windows/System32"),
                                               {TEXT("E:/VR/Pimax"), TEXT("E:/VR/Pimax"), TEXT("relative")});
    TestEqual(TEXT("Alternate Windows drive and deduplicated custom installation"), Paths.Num(), 3);
    TestEqual(TEXT("System directory is not hardcoded"), Paths[0], FString(TEXT("D:/Windows/System32/libPVRClient64.dll")));
    TestTrue(TEXT("Custom installation included"), Paths.Contains(TEXT("E:/VR/Pimax/Runtime/libPVRClient64.dll")));
    TestEqual(TEXT("Relative override fails closed"), PimaxRuntimePaths::Build(TEXT("bad.dll"), TEXT("D:/Windows"), {}).Num(), 0);
    TestEqual(TEXT("Explicit override selects only that installation"),
              PimaxRuntimePaths::Build(TEXT("E:/VR/libPVRClient64.dll"), TEXT("D:/Windows"), {}).Num(), 1);
    return true;
}
#endif

#endif // PLATFORM_WINDOWS
