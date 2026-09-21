#pragma once

#include "Carla/Sensor/DReyeVRData.h"           // DReyeVR namespace
#include "Carla/Sensor/DReyeVRSensor.h"         // ADReyeVRSensor
#include "Components/SceneCaptureComponent2D.h" // USceneCaptureComponent2D
#include <chrono>                               // timing threads
#include <cstdint>

#if USE_SRANIPAL_PLUGIN

// avoid macro conflict since SRanipal uses "ERROR" often
#ifdef ERROR
#undef ERROR
#endif

/// NOTE: Can only use SRanipal on Windows machines
#include "SRanipalEye.h"      // SRanipal Module Framework
#include "SRanipalEye_Core.h" // SRanipal Eye Tracker
#include "ViveSR_Enums.h"     // ViveSR::Error::WORK
// Make sure to patch sranipal before using it here!
#include "SRanipalEye_Framework.h" // StartFramework
#endif

#if PLATFORM_WINDOWS
#include "EyeTracking/PimaxPvrEyeSource.h" // FPimaxPvrEyeSource
#endif

#include "EgoSensor.generated.h"

class AEgoVehicle;
class ADReyeVRGameMode;

UCLASS()
class CARLAUE4_API AEgoSensor : public ADReyeVRSensor
{
    GENERATED_BODY()

  public:
    AEgoSensor(const FObjectInitializer &ObjectInitializer);

    void ManualTick(float DeltaSeconds); // Tick called explicitly from DReyeVR EgoVehicle

    void SetEgoVehicle(class AEgoVehicle *EgoVehicle); // provide access to EgoVehicle (and by extension its camera)

    void UpdateData(const DReyeVR::ConfigFileData &RecorderData, const double Per) override;
    void UpdateData(const DReyeVR::CustomActorData &RecorderData, const double Per) override;

    // function where replayer requests a screenshot
    void TakeScreenshot() override;
    bool ComputeGazeTrace(FHitResult &Hit, const ECollisionChannel TraceChannel, float TraceRadius = 0.f) const;

  protected:
    void BeginPlay();
    void BeginDestroy();

    class UWorld *World; // to get info about the world: time, frames, etc.

  private:
    int64_t TickCount = 0; // how many ticks have been executed
    void ReadConfigVariables();

  private: // eye tracker
    void InitEyeTracker();
    void DestroyEyeTracker();
    void ComputeDummyEyeData(); // when no hardware sensor is present
    void TickEyeTracker();      // tick hardware sensor
    void ComputeFocusInfo();
    void ComputeTraceFocusInfo(const ECollisionChannel TraceChannel, float TraceRadius = 0.f);
    float MaxTraceLenM = 100.f;        // maximum trace length in m
    bool bDrawDebugFocusTrace = false; // draw the trace ray and hit point or not
    bool bUseCameraRelativeGazeDisplay = true;
    UPROPERTY(Transient)
    class UCameraGazeComponent *GazeDisplay = nullptr;
    void UpdateGazeDisplay();
    float ComputeVergence(const FVector &L0, const FVector &LDir, const FVector &R0, const FVector &RDir) const;
#if USE_SRANIPAL_PLUGIN
    SRanipalEye_Core *SRanipal;               // SRanipalEye_Core.h
    SRanipalEye_Framework *SRanipalFramework; // SRanipalEye_Framework.h
    ViveSR::anipal::Eye::EyeData EyeData;     // SRanipal_Eyes_Enums.h
    bool bSRanipalEnabled;                    // Whether or not the framework has been loaded
#endif
#if PLATFORM_WINDOWS
    // Pimax PVR eye-tracking backend (alternative to SRanipal), enabled via
    // DReyeVRConfig.ini [Pimax] EnablePvrEyeTracking
    TUniquePtr<FPimaxPvrEyeSource> PvrEyeSource;
#endif
    bool bEnablePvrEyeTracking = false; // read from config regardless of platform
    void TickPimaxEyeTracker();         // poll Pimax PVR and fill EyeSensorData (no-op off Windows)
    struct DReyeVR::EyeTracker EyeSensorData;                           // data from eye tracker
    struct DReyeVR::FocusInfo FocusInfoData;                            // data from the focus computed from eye gaze
    std::chrono::time_point<std::chrono::system_clock> ChronoStartTime; // std::chrono time at BeginPlay

  private: // ego=vehicle variables
    void ComputeEgoVars();
    TWeakObjectPtr<class AEgoVehicle> Vehicle; // the DReyeVR EgoVehicle
    struct DReyeVR::EgoVariables EgoVars;      // data from vehicle that is getting tracked
    DReyeVR::ConfigFileData *RecordingCF = nullptr;

  private: // frame capture
    size_t ScreenshotCount = 0;
    void ConstructFrameCapture(); // needs to be called in the constructor
    void InitFrameCapture();      // needs to be called in BeginPlay
    class UTextureRenderTarget2D *CaptureRenderTarget = nullptr;
    class USceneCaptureComponent2D *FrameCap = nullptr;
    FString FrameCapLocation; // relative to game dir
    FString FrameCapFilename; // gets ${tick}.png suffix
    int FrameCapWidth;
    int FrameCapHeight;
    bool bCaptureFrameData;
    bool bRecordAllShaders;
    bool bRecordAllPoses;
    bool bCreatedDirectory = false;
    bool bFileFormatJPG = true;
    bool bFrameCapForceLinearGamma = true;

  private: // foveated rendering
    void TickFoveatedRender();
    void ConvertToEyeTrackerSpace(FVector &inVec) const;
    bool bEnableFovRender = false;
    bool bUseEyeTrackingVRS = true;
};
