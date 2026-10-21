#include "EgoSensor.h"
#include "CameraGazeComponent.h"

#include "Carla/Game/CarlaStatics.h"    // GetCurrentEpisode
#include "DReyeVRUtils.h"               // GeneralParams.Get, ComputeClosestToRayIntersection, SaveFrameToDisk
#include "EgoVehicle.h"                 // AEgoVehicle
#include "Engine/TextureRenderTarget2D.h" // UTextureRenderTarget2D (live capture)
#include "Kismet/GameplayStatics.h"     // UGameplayStatics::ProjectWorldToScreen
#include "Kismet/KismetMathLibrary.h"   // Sin, Cos, Normalize
#include "Misc/CommandLine.h"           // FCommandLine (eye poll thread rate)
#include "Misc/DateTime.h"              // FDateTime
#include "Misc/Guid.h"                  // FGuid
#include "Misc/Parse.h"                 // FParse::Value
#include "Misc/Paths.h"                 // FPaths
#include "UObject/UObjectBaseUtility.h" // GetName

#if USE_SRANIPAL_PLUGIN
#include "SRanipal_API.h" // SRanipal_GetVersion
#endif

#if USE_FOVEATED_RENDER
#include "EyeTrackerTypes.h"             // FEyeTrackerStereoGazeData
#include "VRSBlueprintFunctionLibrary.h" // VRS
#endif

#include <string>

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#endif

namespace
{
struct FDebugGazeSegment
{
    FVector Start;
    FVector End;
};

FDebugGazeSegment MakeDebugGazeSegment(const FVector &CameraPosition, const FRotator &CameraRotation,
                                     const FVector &FocusPoint)
{
    // Display only: keep the visible pivot in front of the near clipping plane.
    // Never offset along the gaze direction: that would move the pivot with the eyes.
    constexpr float ForwardOffsetCm = 30.f;
    return {CameraPosition + CameraRotation.Vector() * ForwardOffsetCm, FocusPoint};
}
} // namespace

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDebugGazeAnchorTest, "HUTB.Pimax.GazeDisplay.FixedAnchor",
                                EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDebugGazeAnchorTest::RunTest(const FString &Parameters)
{
    const FVector Camera(100.f, 200.f, 300.f);
    const FVector LeftTarget(1000.f, -100.f, 350.f);
    const FVector RightTarget(1000.f, 500.f, 400.f);
    const auto Left = MakeDebugGazeSegment(Camera, FRotator::ZeroRotator, LeftTarget);
    const auto Right = MakeDebugGazeSegment(Camera, FRotator::ZeroRotator, RightTarget);
    TestTrue(TEXT("Eye movement does not move the displayed origin"), Left.Start.Equals(Right.Start));
    TestTrue(TEXT("Origin is 30 cm in front of the headset"), Left.Start.Equals(Camera + FVector(30.f, 0.f, 0.f)));
    TestTrue(TEXT("Left endpoint is the measured focus point"), Left.End.Equals(LeftTarget));
    TestTrue(TEXT("Right endpoint is the measured focus point"), Right.End.Equals(RightTarget));
    const auto Turned = MakeDebugGazeSegment(Camera, FRotator(0.f, 90.f, 0.f), RightTarget);
    TestTrue(TEXT("Origin follows headset rotation"), Turned.Start.Equals(Camera + FVector(0.f, 30.f, 0.f), 0.001f));
    const FVector Movement(50.f, 20.f, -10.f);
    const auto Moved = MakeDebugGazeSegment(Camera + Movement, FRotator::ZeroRotator, RightTarget);
    TestTrue(TEXT("Origin follows headset translation"), Moved.Start.Equals(Right.Start + Movement));
    return true;
}
#endif

#if PLATFORM_WINDOWS
/// Parse a comma-separated list of hex IDs ("0x34A4, 0x0044") from DReyeVRConfig.ini.
static TArray<int32> ParseHexIdList(const FString &Csv, const TArray<int32> &Fallback)
{
    TArray<int32> Out;
    TArray<FString> Parts;
    Csv.ParseIntoArray(Parts, TEXT(","), /*CullEmpty=*/true);
    for (FString &Part : Parts)
    {
        Part.TrimStartAndEndInline();
        if (!Part.IsEmpty())
            Out.Add((int32)FCString::Strtoi(*Part, nullptr, 16));
    }
    return Out.Num() > 0 ? Out : Fallback;
}
#endif

#ifndef NO_DREYEVR_EXCEPTIONS
#include <exception>
#include <typeinfo>

namespace carla
{
void throw_exception(const std::exception &e)
{
    // It should never reach this part.
    std::terminate();
}
} // namespace carla
#endif

AEgoSensor::AEgoSensor(const FObjectInitializer &ObjectInitializer) : Super(ObjectInitializer)
{
    ReadConfigVariables();

    // Initialize the frame capture (constructor call to create USceneCaptureComponent2D)
    ConstructFrameCapture();
}

void AEgoSensor::ReadConfigVariables()
{
    GeneralParams.Get("EgoSensor", "StreamSensorData", bStreamData);
    GeneralParams.Get("EgoSensor", "MaxTraceLenM", MaxTraceLenM);
    GeneralParams.Get("EgoSensor", "DrawDebugFocusTrace", bDrawDebugFocusTrace);
    GeneralParams.Get("EgoSensor", "UseCameraRelativeGazeDisplay", bUseCameraRelativeGazeDisplay);

    // Pimax PVR eye tracking (dynamically loaded backend, Windows only)
    GeneralParams.Get("Pimax", "EnablePvrEyeTracking", bEnablePvrEyeTracking);

    // variables corresponding to the action of screencapture during replay
    GeneralParams.Get("Replayer", "RecordAllShaders", bRecordAllShaders);
    GeneralParams.Get("Replayer", "RecordAllPoses", bRecordAllPoses);
    GeneralParams.Get("Replayer", "RecordFrames", bCaptureFrameData);
    GeneralParams.Get("Replayer", "FileFormatJPG", bFileFormatJPG);
    GeneralParams.Get("Replayer", "LinearGamma", bFrameCapForceLinearGamma);
    GeneralParams.Get("Replayer", "FrameWidth", FrameCapWidth);
    GeneralParams.Get("Replayer", "FrameHeight", FrameCapHeight);
    GeneralParams.Get("Replayer", "FrameDir", FrameCapLocation);
    GeneralParams.Get("Replayer", "FrameName", FrameCapFilename);

#if USE_FOVEATED_RENDER
    // foveated rendering variables
    GeneralParams.Get("VariableRateShading", "Enabled", bEnableFovRender);
    GeneralParams.Get("VariableRateShading", "UsingEyeTracking", bUseEyeTrackingVRS);
#endif
}

void AEgoSensor::BeginPlay()
{
    Super::BeginPlay();

    World = GetWorld();
    ChronoStartTime = std::chrono::system_clock::now();

    // Initialize the eye tracker hardware
    InitEyeTracker();
    DrivingRecorder.Start();

#if USE_FOVEATED_RENDER
    // Initialize VRS plugin (using our VRS fork!)
    UVariableRateShadingFunctionLibrary::EnableVRS(bEnableFovRender);
    UVariableRateShadingFunctionLibrary::EnableEyeTracking(bUseEyeTrackingVRS);
    LOG("Initialized Variable Rate Shading (VRS) plugin");
#endif

    LOG("Initialized DReyeVR EgoSensor");
}

void AEgoSensor::BeginDestroy()
{
    CleanupLiveCapture();
    DrivingRecorder.Close();
    Super::BeginDestroy();

    if (RecordingCF != nullptr)
    {
        delete RecordingCF;
    }

    DestroyEyeTracker();

    LOG("EgoSensor has been destroyed");
}

void AEgoSensor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    CleanupLiveCapture();
    DrivingRecorder.Close();
    Super::EndPlay(EndPlayReason);
}

void AEgoSensor::ManualTick(float DeltaSeconds)
{
    if (!ADReyeVRSensor::bIsReplaying) // only update the sensor with local values if not replaying
    {
        const float Timestamp = int64_t(1000.f * UGameplayStatics::GetRealTimeSeconds(World));
        // With -DReyeVREyeThreadHz>0 the hardware is polled on its own thread and this only drains it.
        TickEyeTracker();   // query the eye-tracker hardware for current data
        ComputeEgoVars();   // sample this frame's camera pose before computing focus
        ComputeFocusInfo(); // compute gaze focus data using this frame's eye data and pose

        // Update the internal sensor data that gets handed off to Carla (for recording/replaying/PythonAPI)
        const auto &Inputs = Vehicle.IsValid() ? Vehicle.Get()->GetVehicleInputs() : DReyeVR::UserInputs{};
        GetData()->Update(Timestamp,     // TimestampCarla (ms)
                          EyeSensorData, // EyeTrackerData
                          EgoVars,       // EgoVehicleVariables
                          FocusInfoData, // FocusData
                          Inputs         // User inputs
        );
        if (Vehicle.IsValid())
            DrivingRecorder.Record(*Vehicle.Get(), *GetData(), FocusInfoData, EyeSourceName);
        TickFoveatedRender();
        // Live screenshot capture (independent 20 Hz timer, runs with or without CSV recording)
        if (Vehicle.IsValid() && !bLiveCapDisabled)
        {
            auto *Episode = UCarlaStatics::GetCurrentEpisode(World);
            const double SimTime = Episode ? Episode->GetElapsedGameTime() : World->GetTimeSeconds();
            TickLiveCapture(SimTime);
        }
    }
    // Pump async screenshot completions on every tick (no-op when the queue does not exist)
    if (LiveCapQueue)
    {
        LiveCapQueue->Tick();
    }
    UpdateGazeDisplay();
    TickCount++;
}

void AEgoSensor::UpdateGazeDisplay()
{
    if (!bDrawDebugFocusTrace || !bUseCameraRelativeGazeDisplay || !Vehicle.IsValid())
        return;
    auto *Camera = Vehicle->GetCamera();
    if (!Camera)
        return;
    if (!GazeDisplay)
    {
        GazeDisplay = NewObject<UCameraGazeComponent>(this, TEXT("CameraGazeDisplay"));
        GazeDisplay->SetupAttachment(Camera);
        GazeDisplay->RegisterComponent();
        LOG("Gaze display: camera-attached primitive, post-camera tick, XR late update");
    }
    if (GazeDisplay->GetAttachParent() != Camera)
        GazeDisplay->AttachToComponent(Camera, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
    GazeDisplay->Configure(MaxTraceLenM * 100.f, Vehicle.Get());
    // Do not display cached live gaze during replay or tracker failure.
    GazeDisplay->SetSample(EyeSensorData.Combined.GazeOrigin, EyeSensorData.Combined.GazeDir,
                          !ADReyeVRSensor::bIsReplaying && EyeSensorData.Combined.GazeValid);
}

/// ========================================== ///
/// ---------------:EYETRACKER:--------------- ///
/// ========================================== ///

void AEgoSensor::InitEyeTracker()
{
#if USE_SRANIPAL_PLUGIN
    bSRanipalEnabled = false;

    char *SR_Version_chars = new char[128]();
    ViveSR::anipal::SRanipal_GetVersion(SR_Version_chars);
    const FString SR_Version(SR_Version_chars);
    {
        // we found that these versions work great, other versions may cause random crashes
        const std::vector<std::string> SupportedVers = {"1.3.1.1", "1.3.2.0", "1.3.3.0"};
        auto FoundVersion = std::find(SupportedVers.begin(), SupportedVers.end(), std::string(SR_Version_chars));
        bool bIsCompatible = (FoundVersion != SupportedVers.end());
        if (!bIsCompatible)
        {
            std::string SupportedVersStr = "";
            for (const auto &Ver : SupportedVers)
                SupportedVersStr += Ver + ", ";
            LOG_ERROR("Detected incompatible SRanipal version: %s", *SR_Version);
            LOG_WARN("Please use a compatible SRanipal version such as: {%s}", *FString(SupportedVersStr.c_str()));
            LOG_WARN("Check out the DReyeVR documentation to download a supported version.");
            LOG_WARN("Disabling SRanipal for now...");
            bSRanipalEnabled = false;
            return;
        }

        // initialize SRanipal framework for eye tracking
        LOG("Attempting to use SRanipal (%s) for eye tracking", *SR_Version);
    }
    // Initialize the SRanipal eye tracker (WINDOWS ONLY)
    SRanipalFramework = SRanipalEye_Framework::Instance();
    SRanipal = SRanipalEye_Core::Instance();
    // no easily discernible difference between v1 and v2
    /// TODO: use the status output from StartFramework to determine if SRanipal loaded successfully
    int Status = SRanipalFramework->StartFramework(SupportedEyeVersion::version1);
    if (Status == SRanipalEye_Framework::FrameworkStatus::ERROR ||
        Status == SRanipalEye_Framework::FrameworkStatus::NOT_SUPPORT)
    {
        LOG_ERROR("Unable to start SRanipal framework (%d)!", Status);
        return;
    }
    // SRanipal->SetEyeParameter_() // can set the eye gaze jitter parameter
    // see SRanipal_Eyes_Enums.h
    // Get the reference timing to synchronize the SRanipal timer with Carla
    LOG("Successfully started SRanipal (%s) framework", *SR_Version);
    bSRanipalEnabled = true;
#else
    LOG("Not using SRanipal eye tracking");
#endif

#if PLATFORM_WINDOWS
    if (bEnablePvrEyeTracking)
    {
#if USE_SRANIPAL_PLUGIN
        if (bSRanipalEnabled)
        {
            LOG_WARN("Both SRanipal and Pimax PVR eye tracking are enabled; SRanipal takes precedence");
        }
        else
#endif
        {
            FPimaxPvrEyeSource::FConfig PvrConfig;
            GeneralParams.Get("Pimax", "AllowUnknownPimaxDevice", PvrConfig.bAllowUnknownPimaxDevice);
            GeneralParams.Get("Pimax", "MaxSampleAgeSeconds", PvrConfig.MaxSampleAgeSeconds);
            // Samples whose angles never change while the device clock keeps advancing are
            // frozen tracker reads, not fixations (measured: 6.3-43.8% of samples per session).
            // The ini value is a float like every other knob here, so it goes through a float
            // before landing in the double. 0 disables the gate.
            float MaxFrozenSec = static_cast<float>(PvrConfig.MaxFrozenSeconds);
            GeneralParams.Get("Pimax", "MaxFrozenSeconds", MaxFrozenSec);
            float CmdMaxFrozenSec = 0.f;
            if (FParse::Value(FCommandLine::Get(), TEXT("DReyeVREyeMaxFrozenSec="), CmdMaxFrozenSec) &&
                FMath::IsFinite(static_cast<double>(CmdMaxFrozenSec)))
                MaxFrozenSec = CmdMaxFrozenSec;
            if (MaxFrozenSec >= 0.f)
                PvrConfig.MaxFrozenSeconds = MaxFrozenSec;
            GeneralParams.Get("Pimax", "GazeInvertHorizontal", PvrConfig.bInvertHorizontal);
            GeneralParams.Get("Pimax", "GazeInvertVertical", PvrConfig.bInvertVertical);
            PvrConfig.AllowedVendorIds =
                ParseHexIdList(GeneralParams.Get<FString>("Pimax", "AllowedVendorIds"), PvrConfig.AllowedVendorIds);
            PvrConfig.AllowedProductIds =
                ParseHexIdList(GeneralParams.Get<FString>("Pimax", "AllowedProductIds"), PvrConfig.AllowedProductIds);
            // 0 (default) keeps the frame-driven polling path unchanged; >0 polls on a
            // dedicated thread at that rate so the gaze stream keeps its native density
            // even when VR renders at 30 fps.
            float EyeThreadHz = 0.f;
            GeneralParams.Get("Pimax", "EyeThreadHz", EyeThreadHz);
            float CmdEyeThreadHz = 0.f;
            if (FParse::Value(FCommandLine::Get(), TEXT("DReyeVREyeThreadHz="), CmdEyeThreadHz) &&
                CmdEyeThreadHz > 0.f)
                EyeThreadHz = CmdEyeThreadHz;
            if (EyeThreadHz > 0.f)
                PvrConfig.AsyncPollHz = FMath::Clamp((double)EyeThreadHz, 30.0, 500.0);

            PvrEyeSource = TUniquePtr<FPimaxPvrEyeSource>(new FPimaxPvrEyeSource(PvrConfig));
            if (PvrEyeSource->Initialize())
            {
                LOG("Using Pimax PVR eye tracking");
            }
            else
            {
                // keep the object alive: PollLatest() retries initialization with bounded backoff
                LOG_ERROR("Pimax PVR eye tracking failed to initialize; using invalid samples until it recovers");
            }
        }
    }
#endif
}

void AEgoSensor::DestroyEyeTracker()
{
#if USE_SRANIPAL_PLUGIN
    if (SRanipalFramework)
    {
        SRanipalFramework->StopFramework();
        SRanipalEye_Framework::DestroyEyeFramework();
    }
    if (SRanipal)
        SRanipalEye_Core::DestroyEyeModule();
#endif
#if PLATFORM_WINDOWS
    if (PvrEyeSource.IsValid())
    {
        PvrEyeSource->Shutdown();
        PvrEyeSource.Reset();
    }
#endif
}

void AEgoSensor::TickEyeTracker()
{
    /// TODO: move this function to an async thread to obtain 120hz data capture
    auto Combined = &(EyeSensorData.Combined);
    auto Left = &(EyeSensorData.Left);
    auto Right = &(EyeSensorData.Right);
#if USE_SRANIPAL_PLUGIN
    if (bSRanipalEnabled)
    {
        EyeSourceName = TEXT("sranipal");
        /// NOTE: the GazeRay is the normalized direction vector of the actual gaze "ray"
        // Getting real eye tracker data
        check(SRanipal != nullptr);
        // Assigns EyeOrigin and Gaze direction (normalized) of combined gaze
        Combined->GazeValid = SRanipal->GetGazeRay(GazeIndex::COMBINE, Combined->GazeOrigin, Combined->GazeDir);
        // Assign Left/Right Gaze direction
        Left->GazeValid = SRanipal->GetGazeRay(GazeIndex::LEFT, Left->GazeOrigin, Left->GazeDir);
        Right->GazeValid = SRanipal->GetGazeRay(GazeIndex::RIGHT, Right->GazeOrigin, Right->GazeDir);
        /// NOTE: the eye gazes are reversed bc SRanipal has a bug in their closed libraries
        // see: https://forum.vive.com/topic/9306-possible-bug-in-unreal-sdk-for-leftright-eye-gazes
        const bool SRANIPAL_EYE_SWAP_BUG = true;
        if (SRANIPAL_EYE_SWAP_BUG) // if the latest SRanipal does not have this bug
        {
            std::swap(Left->GazeDir, Right->GazeDir); // need to swap the gaze directions
        }
        // Assign Eye openness
        Left->EyeOpennessValid = SRanipal->GetEyeOpenness(EyeIndex::LEFT, Left->EyeOpenness);
        Right->EyeOpennessValid = SRanipal->GetEyeOpenness(EyeIndex::RIGHT, Right->EyeOpenness);
        // Assign Pupil positions
        Left->PupilPositionValid = SRanipal->GetPupilPosition(EyeIndex::LEFT, Left->PupilPosition);
        Right->PupilPositionValid = SRanipal->GetPupilPosition(EyeIndex::RIGHT, Right->PupilPosition);

        // Get the "EyeData" which holds useful information such as the timestamp
        int EyeDataStatus = SRanipal->GetEyeData_(&EyeData);
        if (EyeDataStatus == ViveSR::Error::WORK)
        {
            EyeSensorData.TimestampDevice = EyeData.timestamp;
            EyeSensorData.FrameSequence = EyeData.frame_sequence;
            // Assign Pupil Diameters
            Left->PupilDiameter = EyeData.verbose_data.left.pupil_diameter_mm;
            Right->PupilDiameter = EyeData.verbose_data.right.pupil_diameter_mm;
        }
        Combined->Vergence = ComputeVergence(Left->GazeOrigin, Left->GazeDir, Right->GazeOrigin, Right->GazeDir);
        return;
    }
#endif
#if PLATFORM_WINDOWS
    if (PvrEyeSource.IsValid())
    {
        EyeSourceName = TEXT("pimax");
        TickPimaxEyeTracker();
        return;
    }
#endif
    EyeSourceName = TEXT("dummy");
    ComputeDummyEyeData();
    Combined->Vergence = ComputeVergence(Left->GazeOrigin, Left->GazeDir, Right->GazeOrigin, Right->GazeDir);

    // FPlatformProcess::Sleep(0.00833f); // use in async thread to get 120hz
}

void AEgoSensor::TickPimaxEyeTracker()
{
#if PLATFORM_WINDOWS
    check(PvrEyeSource.IsValid());
    auto Combined = &(EyeSensorData.Combined);
    auto Left = &(EyeSensorData.Left);
    auto Right = &(EyeSensorData.Right);

    // The tracker advances on a ~120 Hz grid while the render loop can be far slower, so
    // drain everything this frame produced instead of keeping only the newest one.
    const TArray<DReyeVR::FEyeSample> Samples = PvrEyeSource->PollAllAvailable();
    const DReyeVR::FEyeSample &Sample = Samples.Last(); // the live path always uses the newest

    // PVR 1.26 gives per-eye angles but no eye origins, pupil diameter or openness:
    // leave those fields invalid instead of fabricating data.
    Combined->GazeValid = Sample.bValid;
    Combined->GazeOrigin = Sample.LocalOrigin; // ZeroVector: approximate, no true origin in PVR
    Combined->GazeDir = Sample.bValid ? Sample.LocalDirection : FVector::ZeroVector;

    Left->GazeValid = Sample.bValid && Sample.Left.bValid;
    Left->GazeOrigin = Sample.LocalOrigin;
    Left->GazeDir = Left->GazeValid ? Sample.Left.Direction : FVector::ZeroVector;
    Right->GazeValid = Sample.bValid && Sample.Right.bValid;
    Right->GazeOrigin = Sample.LocalOrigin;
    Right->GazeDir = Right->GazeValid ? Sample.Right.Direction : FVector::ZeroVector;
    // Vergence here means the distance to the two rays' intersection, which needs
    // separate eye origins PVR does not expose. The origin-free interocular angle is
    // written to the gaze stream instead (vergence_deg).
    Combined->Vergence = -1.f; // sentinel: unavailable

    Left->EyeOpennessValid = false;
    Right->EyeOpennessValid = false;
    Left->PupilPositionValid = false;
    Right->PupilPositionValid = false;

    EyeSensorData.TimestampDevice = Sample.bSourceTimeKnown ? int64_t(Sample.SourceTime * 1000.0) : 0;
    EyeSensorData.FrameSequence = int64_t(Sample.Sequence);

    if (UWorld *World = GetWorld())
        DrivingRecorder.RecordGaze(Samples, *World, GFrameCounter);
#endif
}

void AEgoSensor::ComputeDummyEyeData()
{
    // Function to make "dummy" eye data where the eye gaze just looks around in a CCW circle.
    // Useful for when the eye data is unavailable (Plugin not initialized, on Linux, etc.)
    auto Combined = &(EyeSensorData.Combined);
    auto Left = &(EyeSensorData.Left);
    auto Right = &(EyeSensorData.Right);
    // generate dummy values bc no hardware sensor is present
    EyeSensorData.TimestampDevice = int64_t(
        std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now() - ChronoStartTime)
            .count());
    EyeSensorData.FrameSequence = TickCount; // get the current tick

    // generate gaze that rotates in CCW fashion around the camera ray
    const float TimeNow = EyeSensorData.TimestampDevice / 1000.f;
    Combined->GazeDir.X = 5.0;
    Combined->GazeDir.Y = UKismetMathLibrary::Cos(TimeNow);
    Combined->GazeDir.Z = UKismetMathLibrary::Sin(TimeNow);
    UKismetMathLibrary::Vector_Normalize(Combined->GazeDir, 0.0001);

    // Assign the origin position to the (3D space) origin
    Combined->GazeValid = true; // for our Linux case, this is valid
    Combined->GazeOrigin = FVector::ZeroVector;

    // Assign the endpoint of the combined position (faked in Linux) to the left & right gazes too
    Left->GazeDir = Combined->GazeDir;
    Left->GazeOrigin = Combined->GazeOrigin + 5 * FVector::LeftVector;
    Right->GazeDir = Combined->GazeDir;
    Right->GazeOrigin = Combined->GazeOrigin + 5 * FVector::RightVector;
}

void AEgoSensor::ComputeFocusInfo()
{
    if (!EyeSensorData.Combined.GazeValid)
    {
        // gaze invalid this frame: publish an empty focus straight ahead instead of
        // tracing with a stale/zero gaze direction (reticle stays far away, hit is
        // flagged invalid for the recorder and the PythonAPI)
        const FRotator &WorldRot = EgoVars.CameraRotationAbs;
        const FVector &WorldPos = EgoVars.CameraLocationAbs;
        const float TraceLen = MaxTraceLenM * 100.f; // convert m to cm
        FocusInfoData.Actor = nullptr;
        FocusInfoData.HitPoint = WorldPos + WorldRot.Vector() * TraceLen;
        FocusInfoData.Normal = -WorldRot.Vector();
        FocusInfoData.ActorNameTag = TEXT("None");
        FocusInfoData.Distance = TraceLen;
        FocusInfoData.bDidHit = false;
        return;
    }
    // ECC_Visibility: General visibility testing channel.
    // ECC_Camera: Usually used when tracing from the camera to something.
    // https://docs.unrealengine.com/4.27/en-US/API/Runtime/Engine/Engine/ECollisionChannel/
    // https://zompidev.blogspot.com/2021/08/visibility-vs-camera-trace-channels-in.html
    ComputeTraceFocusInfo(ECC_Visibility);
}

bool AEgoSensor::ComputeGazeTrace(FHitResult &Hit, const ECollisionChannel TraceChannel, float TraceRadius) const
{
    const float TraceLen = MaxTraceLenM * 100.f; // convert to m from cm
    // GetData() is published after focus computation, so in live mode it still
    // contains the previous frame. Replay continues to use the recorded data.
    const bool bReplay = ADReyeVRSensor::bIsReplaying;
    const FRotator WorldRot = bReplay ? GetData()->GetCameraRotationAbs() : EgoVars.CameraRotationAbs;
    const FVector WorldPos = bReplay ? GetData()->GetCameraLocationAbs() : EgoVars.CameraLocationAbs;
    const FVector LocalOrigin = bReplay ? GetData()->GetGazeOrigin() : EyeSensorData.Combined.GazeOrigin;
    const FVector LocalDirection = bReplay ? GetData()->GetGazeDir() : EyeSensorData.Combined.GazeDir;
    const FVector GazeOrigin = WorldPos + WorldRot.RotateVector(LocalOrigin);
    const FVector GazeRay = TraceLen * WorldRot.RotateVector(LocalDirection).GetSafeNormal();
    // Create collision information container.
    FCollisionQueryParams TraceParam;
    TraceParam = FCollisionQueryParams(FName("TraceParam"), true);
    if (Vehicle.IsValid())
        TraceParam.AddIgnoredActor(Vehicle.Get()); // don't collide with the vehicle since that would be useless
    TraceParam.bTraceComplex = true;
    TraceParam.bReturnPhysicalMaterial = false;
    Hit = FHitResult(EForceInit::ForceInit);
    bool bDidHit = false;

    // 0 for a point, >0 for a sphear trace
    TraceRadius = FMath::Max(TraceRadius, 0.f); // clamp to be positive

    ensure(World != nullptr);
    if (TraceRadius == 0.f) // Single ray/line trace
    {
        bDidHit = World->LineTraceSingleByChannel(Hit, GazeOrigin, GazeOrigin + GazeRay, TraceChannel, TraceParam);
    }
    else // Sphear line trace
    {
        FCollisionShape Sphear = FCollisionShape();
        Sphear.SetSphere(TraceRadius);
        bDidHit = World->SweepSingleByChannel(Hit, GazeOrigin, GazeOrigin + GazeRay, FQuat(0.f, 0.f, 0.f, 0.f),
                                              TraceChannel, Sphear, TraceParam);
    }

    if (!bDidHit)
    {
        // focus point is just straight ahead to the maximum trace length
        Hit.Actor = nullptr;
        Hit.Location = GazeOrigin + GazeRay;
        Hit.Distance = TraceLen;
    }

    if (bDrawDebugFocusTrace && !bUseCameraRelativeGazeDisplay)
    {
        DrawDebugSphere(World, Hit.Location, 8.0f, 30, FColor::Blue);
        const auto Segment = MakeDebugGazeSegment(WorldPos, WorldRot, Hit.Location);
        DrawDebugLine(World, Segment.Start, Segment.End, FColor::Blue, false, -1, 0, 1);
    }
    return bDidHit;
}

void AEgoSensor::ComputeTraceFocusInfo(const ECollisionChannel TraceChannel, float TraceRadius)
{
    FHitResult Hit;
    bool bDidHit = ComputeGazeTrace(Hit, TraceChannel, TraceRadius);
    // Update fields
    FString ActorName = "None";
    if (Hit.Actor != nullptr)
    {
        Hit.Actor->GetName(ActorName);
        FString Suffix = ""; // suffix to the actor "name" (actor type we care about)
        if (Cast<AWheeledVehicle>(Hit.Actor) != nullptr)
            Suffix = "_Vehicle";
        else if (Cast<ACharacter>(Hit.Actor) != nullptr)
            Suffix = "_Walker";
        else if (Cast<ATrafficSignBase>(Hit.Actor) != nullptr)
            Suffix = "_TrafficLight";
        /// TODO: add more suffixes here.
        ActorName += Suffix;
    }

    // update internal data structure (see DReyeVRData::FocusInfo)
    FocusInfoData.Actor = Hit.Actor;        // pointer to actor being hit (if any, else nullptr)
    FocusInfoData.HitPoint = Hit.Location;  // absolute (world) location of hit
    FocusInfoData.Normal = Hit.Normal;      // normal of hit surface (if hit)
    FocusInfoData.ActorNameTag = ActorName; // name of the actor being hit (if any, else "None")
    FocusInfoData.Distance = Hit.Distance;  // distance from ray start
    FocusInfoData.bDidHit = bDidHit;        // whether or not there was a hit
}

float AEgoSensor::ComputeVergence(const FVector &L0, const FVector &LDir, const FVector &R0, const FVector &RDir) const
{
    // Compute length of ray-to- intersection of the left and right eye gazes in 3D space (length in centimeters)
    return ComputeClosestToRayIntersection(L0, LDir, R0, RDir).Size(); // units are cm (default for UE4)
}

/// ========================================== ///
/// ---------------:EGOVARS:------------------ ///
/// ========================================== ///

void AEgoSensor::SetEgoVehicle(class AEgoVehicle *NewEgoVehicle)
{
    Vehicle = NewEgoVehicle;
    check(Vehicle.IsValid());

    // Also check that the ConfigFileData variable can be written to with Vehicle params
    check(ConfigFile); // this is a static variable created in the parent (ADReyeVRSensor)

    // track both the VehicleParams and GeneralParams
    const auto ConfigFileStr = Vehicle.Get()->GetVehicleParams().Export() + GeneralParams.Export();
    ConfigFile->Set(ConfigFileStr); // track this config file once

    // saved from some previous request to compare, but failed bc no EgoVehicle
    if (RecordingCF != nullptr)
    {
        UpdateData(*RecordingCF, 0.f);
    }
}

void AEgoSensor::ComputeEgoVars()
{
    if (!Vehicle.IsValid())
    {
        LOG_WARN("Invalid EgoVehicle, cannot compute EgoVars");
        return;
    }
    // See DReyeVRData::EgoVariables
    auto *Ego = Vehicle.Get();
    auto *Camera = Ego->GetCamera();
    check(Camera);
    EgoVars.VehicleLocation = Ego->GetActorLocation();
    EgoVars.VehicleRotation = Ego->GetActorRotation();
    EgoVars.CameraLocation = Camera->GetRelativeLocation();
    EgoVars.CameraRotation = Camera->GetRelativeRotation();
    EgoVars.CameraLocationAbs = Camera->GetComponentLocation();
    EgoVars.CameraRotationAbs = Camera->GetComponentRotation();
    EgoVars.Velocity = Ego->GetVehicleForwardSpeed();
}

/// ========================================== ///
/// ---------------:FRAMECAP:----------------- ///
/// ========================================== ///

void AEgoSensor::ConstructFrameCapture()
{
    if (bCaptureFrameData)
    {
        // Frame capture
        CaptureRenderTarget = CreateDefaultSubobject<UTextureRenderTarget2D>(TEXT("CaptureRenderTarget_DReyeVR"));
        CaptureRenderTarget->CompressionSettings = TextureCompressionSettings::TC_Default;
        CaptureRenderTarget->SRGB = false;
        CaptureRenderTarget->bAutoGenerateMips = false;
        CaptureRenderTarget->bGPUSharedFlag = true;
        CaptureRenderTarget->ClearColor = FLinearColor::Black;
        CaptureRenderTarget->UpdateResourceImmediate(true);
        // CaptureRenderTarget->OverrideFormat = EPixelFormat::PF_FloatRGB;
        CaptureRenderTarget->AddressX = TextureAddress::TA_Clamp;
        CaptureRenderTarget->AddressY = TextureAddress::TA_Clamp;
        CaptureRenderTarget->InitCustomFormat(FrameCapWidth, FrameCapHeight, PF_B8G8R8A8, bFrameCapForceLinearGamma);
        check(CaptureRenderTarget->GetSurfaceWidth() > 0 && CaptureRenderTarget->GetSurfaceHeight() > 0);

        FrameCap = CreateDefaultSubobject<USceneCaptureComponent2D>(TEXT("FrameCap"));
        FrameCap->PrimitiveRenderMode = ESceneCapturePrimitiveRenderMode::PRM_RenderScenePrimitives;
        FrameCap->bCaptureOnMovement = false;
        FrameCap->bCaptureEveryFrame = false;
        FrameCap->bAlwaysPersistRenderingState = true;
        FrameCap->CaptureSource = ESceneCaptureSource::SCS_FinalColorLDR;

        // apply postprocessing effects
        FrameCap->PostProcessSettings = CreatePostProcessingEffect(0);

        FrameCap->Deactivate();
        FrameCap->TextureTarget = CaptureRenderTarget;
        FrameCap->UpdateContent();
        FrameCap->Activate();
    }
    if (FrameCap && !bCaptureFrameData)
    {
        FrameCap->Deactivate();
    }
}

void AEgoSensor::InitFrameCapture()
{
    if (bCaptureFrameData)
    {
        // initialize frame capture attachment to the ego vehicle
        if (FrameCap && Vehicle.IsValid())
            FrameCap->SetupAttachment(Vehicle.Get()->GetCamera());

        // creates the directory for the frame capture to take place
        /// TODO: add check for absolute paths
        FrameCapLocation = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir() + FrameCapLocation);
        // The returned string has the following format: yyyy.mm.dd-hh.mm.ss
        FString DirName = FDateTime::Now().ToString(); // timestamp directory
        FrameCapLocation = FPaths::Combine(FrameCapLocation, DirName);

        // create directory if not present
        LOG("Outputting frame capture data to %s", *FrameCapLocation);
        IPlatformFile &PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
        if (!PlatformFile.DirectoryExists(*FrameCapLocation))
        {
#ifdef CreateDirectory
            // using Windows system calls
            CreateDirectory(*FrameCapLocation, NULL);
#else
            // this only seems to work on Unix systems, else CreateDirectoryW is not linked?
            PlatformFile.CreateDirectory(*FrameCapLocation);
#endif
        }
        bCreatedDirectory = true;
    }
}

void AEgoSensor::TakeScreenshot()
{
    /// NOTE: this is a slow function that takes multiple high-res screenshots (with different shader params)
    // of the current scene and writes the images to disk immediately. The intention is to use this function
    // during synchronized replay with screen capture so that performance is not an issue since the simulator
    // is not necessarily running in real-time.

    // create directory if necessary
    if (bCaptureFrameData && !bCreatedDirectory)
    {
        InitFrameCapture(); // Set up frame capture directory
    }

    // capture the screenshot to the directory
    if (bCaptureFrameData && FrameCap && Vehicle.IsValid())
    {
        for (int i = 0; i < GetNumberOfShaders(); i++)
        {
            // apply the postprocessing effect
            FrameCap->PostProcessSettings = CreatePostProcessingEffect(i);
            // loop through all camera poses
            for (int j = 0; j < Vehicle.Get()->GetNumCameraPoses(); j++)
            {
                // set this pose
                Vehicle.Get()->SetCameraRootPose(j);

                // using 5 digits to reach frame 99999 ~ 30m (assuming ~50fps frame capture)
                // suffix is denoted as _s(hader)X_p(ose)Y_Z.png where X is the shader idx, Y is the pose idx, Z is tick
                const FString Suffix = FString::Printf(TEXT("_s%d_p%d_%05d.png"), i, j, ScreenshotCount);
                // apply the camera view (position & orientation)
                FMinimalViewInfo DesiredView;
                Vehicle.Get()->GetCamera()->GetCameraView(0, DesiredView);
                FrameCap->SetCameraView(DesiredView); // move camera to the camera view
                // capture the scene and save the screenshot to disk
                FrameCap->CaptureScene(); // also available: CaptureSceneDeferred()
                SaveFrameToDisk(*CaptureRenderTarget, FPaths::Combine(FrameCapLocation, FrameCapFilename + Suffix),
                                bFileFormatJPG);
                if (!bRecordAllPoses)
                {
                    // exit after the first camera pose (seated)
                    break;
                }
            }
            // set camera pose back to 0
            Vehicle.Get()->SetCameraRootPose(0);
            if (!bRecordAllShaders)
            {
                // exit after the first shader (rgb)
                break;
            }
        }
        ScreenshotCount++; // progress to next frame
    }
}

/// ========================================== ///
/// ------------:FOVEATEDRENDER:-------------- ///
/// ========================================== ///

void AEgoSensor::ConvertToEyeTrackerSpace(FVector &inVec) const
{
    FVector temp = inVec;
    inVec.X = -1 * temp.Y;
    inVec.Y = temp.Z;
    inVec.Z = temp.X;
}

void AEgoSensor::TickFoveatedRender()
{
#if USE_FOVEATED_RENDER
    FEyeTrackerStereoGazeData F;
    F.LeftEyeOrigin = GetData()->GetGazeOrigin(DReyeVR::Gaze::LEFT);
    F.LeftEyeDirection = GetData()->GetGazeDir(DReyeVR::Gaze::LEFT);
    ConvertToEyeTrackerSpace(F.LeftEyeDirection);
    F.RightEyeOrigin = GetData()->GetGazeOrigin(DReyeVR::Gaze::RIGHT);
    F.RightEyeDirection = GetData()->GetGazeDir(DReyeVR::Gaze::RIGHT);
    ConvertToEyeTrackerSpace(F.RightEyeDirection);
    F.FixationPoint = GetData()->GetFocusActorPoint();
    F.ConfidenceValue = 0.99f;
    UVariableRateShadingFunctionLibrary::UpdateStereoGazeDataToFoveatedRendering(F);
#endif
}

/// ========================================== ///
/// -----------:LIVESCREENCAPTURE:------------ ///
/// ========================================== ///

void AEgoSensor::SetupLiveCapture()
{
    if (bLiveCapReady || bLiveCapDisabled) return;
    if (!Vehicle.IsValid()) return;

    // Parse screenshot frequency from command line (default 20 Hz)
    if (!FParse::Param(FCommandLine::Get(), TEXT("DReyeVRScreenshot")))
    {
        bLiveCapDisabled = true;
        return;
    }
    float Hz = 20.f;
    FParse::Value(FCommandLine::Get(), TEXT("DReyeVRScreenshotHz="), Hz);
    LiveCapInterval = 1.0 / FMath::Clamp(Hz, 1.f, 60.f);
    bLiveCapAsync = FParse::Param(FCommandLine::Get(), TEXT("DReyeVRScreenshotAsync"));

    // Screenshot directory: same base as the CSV when recording, or an independent session base path
    FString BasePath = DrivingRecorder.GetSessionBasePath();
    if (BasePath.IsEmpty())
    {
        FString Directory;
        FParse::Value(FCommandLine::Get(), TEXT("DReyeVRRecordDir="), Directory);
        if (Directory.IsEmpty())
            Directory = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("DrivingStudy"));
        Directory = FPaths::ConvertRelativePathToFull(Directory);
        BasePath = FPaths::Combine(Directory, FDateTime::UtcNow().ToString(TEXT("%Y%m%dT%H%M%SZ")) + TEXT("-") +
                                                  FGuid::NewGuid().ToString(EGuidFormats::Digits));
    }
    LiveCapDir = BasePath + TEXT("_screenshots");
    if (!IFileManager::Get().MakeDirectory(*LiveCapDir, true))
    {
        LOG_ERROR("Driving study: cannot create screenshot dir %s", *LiveCapDir);
        bLiveCapDisabled = true;
        return;
    }

    // Create a lightweight render target (640x480, enough for gaze annotation at 20 Hz)
    LiveCapRT = NewObject<UTextureRenderTarget2D>(this, TEXT("LiveCaptureRT"));
    LiveCapRT->ClearColor = FLinearColor::Black;
    LiveCapRT->bGPUSharedFlag = true;
    LiveCapRT->bAutoGenerateMips = false;
    LiveCapRT->InitCustomFormat(640, 480, PF_B8G8R8A8, /*bForceLinearGamma=*/false);
    check(LiveCapRT->GetSurfaceWidth() > 0 && LiveCapRT->GetSurfaceHeight() > 0);

    // Create a scene capture component attached to the VR camera
    LiveCapComp = NewObject<USceneCaptureComponent2D>(this, TEXT("LiveCapture"));
    LiveCapComp->TextureTarget = LiveCapRT;
    LiveCapComp->bCaptureEveryFrame = false;
    LiveCapComp->bCaptureOnMovement = false;
    LiveCapComp->bAlwaysPersistRenderingState = true;
    LiveCapComp->CaptureSource = ESceneCaptureSource::SCS_FinalColorLDR;
    LiveCapComp->PrimitiveRenderMode = ESceneCapturePrimitiveRenderMode::PRM_RenderScenePrimitives;
    auto *Camera = Vehicle.Get()->GetCamera();
    if (Camera)
    {
        LiveCapComp->SetupAttachment(Camera);
    }
    LiveCapComp->RegisterComponent();

    if (bLiveCapAsync)
    {
        FAsyncFrameCaptureConfig QueueConfig;
        LiveCapReader = MakeUnique<FDReyeVRRenderThreadFrameReader>();
        LiveCapSink = MakeUnique<FDReyeVRImageWriteSink>();
        LiveCapQueue = MakeUnique<FAsyncFrameCaptureQueue>(*LiveCapReader, *LiveCapSink, QueueConfig);
        LOG("Driving study: async screenshot path enabled (max %d in flight)", QueueConfig.MaxInFlightCaptures);
    }

    bLiveCapReady = true;
    LOG("Driving study: live screenshot capture %.1f Hz (640x480 JPEG) to %s", 1.0 / LiveCapInterval, *LiveCapDir);
}

void AEgoSensor::TickLiveCapture(double SimTime)
{
    // Lazy init: Vehicle is not yet available in BeginPlay, so set up on first tick
    if (!bLiveCapReady)
    {
        SetupLiveCapture();
        if (!bLiveCapReady) return;
    }
    if (!LiveCapComp || !LiveCapRT) return;

    // Rate-limit to the configured Hz
    if (LiveCapLastTime >= 0.0 && SimTime < LiveCapLastTime + LiveCapInterval - 1e-6) return;
    LiveCapLastTime = SimTime;

    // Match the SceneCapture viewpoint to the current VR camera
    auto *Camera = Vehicle.IsValid() ? Vehicle.Get()->GetCamera() : nullptr;
    if (Camera)
    {
        FMinimalViewInfo View;
        Camera->GetCameraView(0, View);
        LiveCapComp->SetCameraView(View);
    }

    // CaptureScene renders the scene a second time; the synchronous readback that used to
    // follow it is what stalled the game thread. Measured 2026-10-11: 1554 captures produced
    // 1550 rows whose ue_frame advanced by 2, i.e. one whole extra presented frame each.
    LiveCapComp->CaptureScene();

    // File name encodes frame number and sim_time for CSV alignment
    const FString FileName = FString::Printf(TEXT("%06llu_%.4fs.jpg"), LiveCapCount, SimTime);
    if (LiveCapQueue)
    {
        // async path (-DReyeVRScreenshotAsync): the name and index are frozen at submit time;
        // GPU readback and JPEG encode/write happen off the game thread
        if (!LiveCapQueue->EnqueueCapture(LiveCapRT, FPaths::Combine(LiveCapDir, FileName), LiveCapCount, SimTime) &&
            (++LiveCapDropLogCount % 100) == 1)
        {
            LOG_WARN("Driving study: async screenshot queue full, dropped %llu captures so far",
                     LiveCapQueue->GetDropped());
        }
    }
    else
    {
        SaveFrameToDisk(*LiveCapRT, FPaths::Combine(LiveCapDir, FileName), /*bJPG=*/true);
    }
    ++LiveCapCount;
}

void AEgoSensor::CleanupLiveCapture()
{
    if (LiveCapQueue)
    {
        LiveCapQueue->Flush();
        LOG("Driving study: async screenshot stats: submitted=%llu completed=%llu dropped=%llu maxInFlight=%d",
            LiveCapQueue->GetSubmitted(), LiveCapQueue->GetCompleted(), LiveCapQueue->GetDropped(),
            LiveCapQueue->GetMaxInFlightSeen());
        LiveCapQueue.Reset(); // must die before the reader/sink it references
        LiveCapSink.Reset();
        LiveCapReader.Reset();
    }
    if (LiveCapComp)
    {
        LiveCapComp->DestroyComponent();
        LiveCapComp = nullptr;
    }
    LiveCapRT = nullptr;
    bLiveCapReady = false;
    bLiveCapDisabled = false;
    if (LiveCapCount > 0)
    {
        LOG("Driving study: captured %llu screenshots to %s", LiveCapCount, *LiveCapDir);
    }
}

/// ========================================== ///
/// ----------------:REPLAY:------------------ ///
/// ========================================== ///

// don't need to override the base ADReyeVRSensor::UpdateData<DReyeVR::AggregateData>();

void AEgoSensor::UpdateData(const DReyeVR::ConfigFileData &RecordedParams, const double Per)
{
    if (!Vehicle.IsValid())
    {
        // LOG_WARN("Unable to compare ConfigFile bc EgoVehicle is invalid!");
        RecordingCF = new DReyeVR::ConfigFileData();
        (*RecordingCF) = RecordedParams; // save these params for later (ex. SetEgoVehicle)
        return;
    }
    // compare the incoming (recording) ConfigFile with our current (live) one
    const std::string RecordingExport = TCHAR_TO_UTF8(*RecordedParams.ToString());
    const struct ConfigFile Recorded = ConfigFile::Import(RecordingExport);

    // includes both the vehicle params and general params
    struct ConfigFile LiveConfig;
    LiveConfig.Insert(Vehicle.Get()->GetVehicleParams());
    LiveConfig.Insert(GeneralParams);

    bool bPrintWarnings = true;
    if (LiveConfig.IsSubset(Recorded, bPrintWarnings)) // don't care if Recorded has entries that we dont
    {
        LOG("Config file comparison successful!");
    }
    else
    {
        LOG_WARN("Config file comparison failed! This means the recording was performed with a different configuration "
                 "file than what is currently active. You might want to check that this does not affect the validity "
                 "of your replay.");
    }
}

void AEgoSensor::UpdateData(const DReyeVR::CustomActorData &RecorderData, const double Per)
{
    if (Vehicle.IsValid() && Vehicle.Get()->GetGame())
        Vehicle.Get()->GetGame()->ReplayCustomActor(RecorderData, Per);
}
