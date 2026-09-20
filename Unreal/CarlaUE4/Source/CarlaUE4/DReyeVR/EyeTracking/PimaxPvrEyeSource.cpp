#include "PimaxPvrEyeSource.h"

#if PLATFORM_WINDOWS

#include "CarlaUE4.h" // LOG / LOG_WARN / LOG_ERROR
#include "HAL/PlatformTime.h"
#include "HAL/PlatformMisc.h"
#include "PimaxRuntimePaths.h"

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
/// Poll getHmdStatus once per N eye-tracking polls (~1 Hz at 60 fps).
constexpr uint64 StatusCheckInterval = 60;
} // namespace

FPimaxPvrEyeSource::FPimaxPvrEyeSource(const FConfig &InConfig) : Config(InConfig)
{
}

FPimaxPvrEyeSource::~FPimaxPvrEyeSource()
{
    Shutdown();
}

void FPimaxPvrEyeSource::TransitionTo(EState NewState, const TCHAR *Reason)
{
    if (State == NewState)
        return;
    LOG("Pimax PVR eye source: state %d -> %d (%s)", (int32)State, (int32)NewState, Reason);
    State = NewState;
}

void FPimaxPvrEyeSource::ReleaseRuntime()
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
    if (State == EState::Running)
        return true;

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
        ReleaseRuntime();
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
        ReleaseRuntime();
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
            ReleaseRuntime();
            TransitionTo(EState::Failed, TEXT("null interface slot"));
            return false;
        }
    }

    // 3. Bring up the client session.
    if (reinterpret_cast<PvrAbi::InitialiseFn>(InterfaceSlots[PvrAbi::Slot_Initialise])() != PvrAbi::Success)
    {
        LOG_ERROR("Pimax PVR: initialise failed (is the Pimax runtime running?)");
        ReleaseRuntime();
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
        ReleaseRuntime();
        TransitionTo(EState::Failed, TEXT("createHmd failed"));
        return false;
    }

    if (!ValidateDevice())
    {
        ReleaseRuntime();
        TransitionTo(EState::Failed, TEXT("device rejected"));
        return false;
    }

    ConsecutiveFailures = 0;
    bHasLastSourceTime = false;
    InitBackoffSeconds = 1.0;
    TransitionTo(EState::Running, TEXT("ready"));
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

DReyeVR::FEyeSample FPimaxPvrEyeSource::PollLatest()
{
    DReyeVR::FEyeSample Sample; // invalid by default
    Sample.ReceivedTime = FPlatformTime::Seconds();
    Sample.Sequence = ++SampleSequence;
    const double Now = Sample.ReceivedTime;

    if (State == EState::Failed)
    {
        // bounded exponential backoff before attempting to bring the session back
        if (Now >= NextInitRetryHostTime)
        {
            LOG_WARN("Pimax PVR: retrying initialization (backoff was %.1fs)", InitBackoffSeconds);
            ReleaseRuntime();
            if (Initialize())
            {
                InitBackoffSeconds = 1.0;
            }
            else
            {
                NextInitRetryHostTime = Now + InitBackoffSeconds;
                InitBackoffSeconds = FMath::Min(InitBackoffSeconds * 2.0, 30.0);
            }
        }
        return Sample;
    }
    if (State != EState::Running)
        return Sample;

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
            TransitionTo(EState::Failed, TEXT("repeated poll failure"));
            NextInitRetryHostTime = Now + InitBackoffSeconds;
        }
    }
    else
    {
        ConsecutiveFailures = 0;

        // the device must actually be present and worn for data to be meaningful
        if ((SampleSequence % StatusCheckInterval) == 1)
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
            // plausibility: finite, bounded tangents
            const float TanHL = Info.GazeTan[0].X, TanVL = Info.GazeTan[0].Y;
            const float TanHR = Info.GazeTan[1].X, TanVR = Info.GazeTan[1].Y;
            if (!FMath::IsFinite(TanHL) || !FMath::IsFinite(TanVL) || !FMath::IsFinite(TanHR) ||
                !FMath::IsFinite(TanVR) || FMath::Abs(TanHL) > MaxPlausibleGazeTan ||
                FMath::Abs(TanVL) > MaxPlausibleGazeTan || FMath::Abs(TanHR) > MaxPlausibleGazeTan ||
                FMath::Abs(TanVR) > MaxPlausibleGazeTan)
            {
                InvalidReason = "implausible-values";
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
                else
                {
                    // average both eyes into a combined (cyclopean) gaze; PVR exposes no per-eye origin
                    float H = FMath::Atan(0.5f * (TanHL + TanHR)); // horizontal angle
                    float V = FMath::Atan(0.5f * (TanVL + TanVR)); // vertical angle
                    if (Config.bInvertHorizontal)
                        H = -H;
                    if (Config.bInvertVertical)
                        V = -V;

                    // The PVR reference math is right-handed with -Z forward:
                    //     dir = (sinH*cosV, sinV, -cosH*cosV)
                    // Converted to UE4 camera-local left-handed (X forward, Y right, Z up):
                    const float CosH = FMath::Cos(H), SinH = FMath::Sin(H);
                    const float CosV = FMath::Cos(V), SinV = FMath::Sin(V);
                    const FVector Direction(CosH * CosV, SinH * CosV, SinV);

                    Sample.LocalDirection = Direction.GetSafeNormal();
                    Sample.LocalOrigin = FVector::ZeroVector; // PVR exposes no eye origin (approximation)
                    Sample.SourceTime = Info.TimeInSeconds;
                    Sample.bSourceTimeKnown = true;
                    Sample.bValid = true;
                }
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
            LOG("Pimax PVR: first valid eye sample (poll #%llu, timeInSec=%.3f, dir=%s)", SampleSequence,
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
        LOG("Pimax PVR diag: valid=%llu invalid=%llu lastResult=%d timeInSec=%.3f present=%d mounted=%d age=%.3f "
            "reason=%s",
            DiagValidCount, DiagInvalidCount, LastResult, LastTimeInSeconds, LastHmdPresent, LastHmdMounted,
            bHasLastSourceTime ? (float)(Now - LastSourceTimeSeenHost) : -1.f,
            InvalidReason != nullptr ? ANSI_TO_TCHAR(InvalidReason) : TEXT("none"));
        DiagValidCount = 0;
        DiagInvalidCount = 0;
    }
    return Sample;
}

void FPimaxPvrEyeSource::Shutdown()
{
    ReleaseRuntime();
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

    FPimaxPvrEyeSource Source{FPimaxPvrEyeSource::FConfig{}};
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
