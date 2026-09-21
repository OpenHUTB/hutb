#pragma once

#include "Camera/CameraComponent.h"                   // UCameraComponent
#include "Carla/Actor/DReyeVRCustomActor.h"           // ADReyeVRCustomActor
#include "Carla/Game/CarlaEpisode.h"                  // CarlaEpisode
#include "Carla/Sensor/DReyeVRData.h"                 // DReyeVR namespace
#include "Carla/Vehicle/CarlaWheeledVehicle.h"        // ACarlaWheeledVehicle
#include "Carla/Vehicle/WheeledVehicleAIController.h" // AWheeledVehicleAIController
#include "Components/AudioComponent.h"                // UAudioComponent
#include "Components/InputComponent.h"                // InputComponent
#include "Components/PlanarReflectionComponent.h"     // Planar Reflection
#include "Components/SceneComponent.h"                // USceneComponent
#include "CoreMinimal.h"                              // Unreal functions
#include "DReyeVRGameMode.h"                          // ADReyeVRGameMode
#include "DReyeVRUtils.h"                             // GeneralParams.Get
#include "EgoSensor.h"                                // AEgoSensor
#include "FlatHUD.h"                                  // ADReyeVRHUD
#include "ImageUtils.h"                               // CreateTexture2D
#include "WheeledVehicle.h"                           // VehicleMovementComponent
#include <stdio.h>
#include <vector>

#include "EgoVehicle.generated.h"

class ADReyeVRGameMode;
class ADReyeVRPawn;

UCLASS()
class CARLAUE4_API AEgoVehicle : public ACarlaWheeledVehicle
{
    GENERATED_BODY()

    friend class ADReyeVRPawn;

  public:
    // Sets default values for this pawn's properties
    AEgoVehicle(const FObjectInitializer &ObjectInitializer);

    void ReadConfigVariables();

    virtual void Tick(float DeltaTime) override; // called automatically

    // Setters from external classes
    void SetGame(ADReyeVRGameMode *Game);
    ADReyeVRGameMode *GetGame();
    void SetPawn(ADReyeVRPawn *Pawn);
    void SetVolume(const float VolumeIn);

    // Getters
    const FString &GetVehicleType() const;
    FVector GetCameraOffset() const;
    FVector GetCameraPosn() const;
    FVector GetNextCameraPosn(const float DeltaSeconds) const;
    FRotator GetCameraRot() const;
    const UCameraComponent *GetCamera() const;
    UCameraComponent *GetCamera();
    const DReyeVR::UserInputs &GetVehicleInputs() const;
    const class AEgoSensor *GetSensor() const;
    const struct ConfigFile &GetVehicleParams() const;

    // autopilot API
    void SetAutopilot(const bool AutopilotOn);
    bool GetAutopilotStatus() const;
    /// TODO: add custom routes for autopilot

    // Play sounds
    void PlayGearShiftSound(const float DelayBeforePlay = 0.f) const;
    void PlayTurnSignalSound(const float DelayBeforePlay = 0.f) const;

    // Camera view
    size_t GetNumCameraPoses() const;                // how many diff poses?
    void SetCameraRootPose(const FTransform &Pose);  // give arbitrary FTransform
    void SetCameraRootPose(const FString &PoseName); // index into named FTransform
    void SetCameraRootPose(size_t PoseIdx);          // index into ordered FTransform
    const FTransform &GetCameraRootPose() const;
    void BeginThirdPersonCameraInit();
    void NextCameraView();
    void PrevCameraView();

  protected:
    // Called when the game starts (spawned) or ends (destroyed)
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual void BeginDestroy() override;

    // custom configuration file for vehicle-specific parameters
    struct ConfigFile VehicleParams;

    // World variables
    class UWorld *World;

  private:
    template <typename T> T *CreateEgoObject(const FString &Name, const FString &Suffix = "");
    FString VehicleType; // initially empty (set in GetVehicleType())

  private: // camera
    UPROPERTY(Category = Camera, EditAnywhere, BlueprintReadWrite, meta = (AllowPrivateAccess = "true"))
    class USceneComponent *VRCameraRoot;
    UPROPERTY(Category = Camera, EditAnywhere, BlueprintReadWrite, meta = (AllowPrivateAccess = "true"))
    class UCameraComponent *FirstPersonCam;
    void ConstructCameraRoot();                 // needs to be called in the constructor
    FTransform CameraPose, CameraPoseOffset;    // camera pose (location & rotation) and manual offset
    TMap<FString, FTransform> CameraTransforms; // collection of named transforms from params
    TArray<FString> CameraPoseKeys;             // to iterate through them
    size_t CurrentCameraTransformIdx = 0;       // to index in CameraPoseKeys which indexes into CameraTransforms
    bool bCameraFollowHMD = true; // disable this (in params) to replay without following the player's HMD (replay-only)

  private: // sensor
    class AEgoSensor *GetSensor();
    void ReplayTick();
    void InitSensor();
    // custom sensor helper that holds logic for extracting useful data
    TWeakObjectPtr<class AEgoSensor> EgoSensor; // EgoVehicle should have exclusive access (TODO: unique ptr?)
    void UpdateSensor(const float DeltaTime);

  private: // pawn
    class ADReyeVRPawn *Pawn = nullptr;

  private: // mirrors
    void ConstructMirrors();
    struct MirrorParams
    {
        bool Enabled;
        FTransform MirrorTransform, ReflectionTransform;
        float ScreenPercentage;
        FString Name;
        void Initialize(class UStaticMeshComponent *SM, class UPlanarReflectionComponent *Reflection,
                        class USkeletalMeshComponent *VehicleMesh);
    };
    struct MirrorParams RearMirrorParams, LeftMirrorParams, RightMirrorParams;
    UPROPERTY(Category = Mirrors, EditAnywhere, BlueprintReadWrite, meta = (AllowPrivateAccess = "true"))
    class UStaticMeshComponent *RightMirrorSM;
    UPROPERTY(Category = Mirrors, EditAnywhere, BlueprintReadWrite, meta = (AllowPrivateAccess = "true"))
    class UPlanarReflectionComponent *RightReflection;
    UPROPERTY(Category = Mirrors, EditAnywhere, BlueprintReadWrite, meta = (AllowPrivateAccess = "true"))
    class UStaticMeshComponent *LeftMirrorSM;
    UPROPERTY(Category = Mirrors, EditAnywhere, BlueprintReadWrite, meta = (AllowPrivateAccess = "true"))
    class UPlanarReflectionComponent *LeftReflection;
    UPROPERTY(Category = Mirrors, EditAnywhere, BlueprintReadWrite, meta = (AllowPrivateAccess = "true"))
    class UStaticMeshComponent *RearMirrorSM;
    UPROPERTY(Category = Mirrors, EditAnywhere, BlueprintReadWrite, meta = (AllowPrivateAccess = "true"))
    class UPlanarReflectionComponent *RearReflection;
    // rear mirror chassis (dynamic)
    UPROPERTY(Category = Mirrors, EditAnywhere, BlueprintReadWrite, meta = (AllowPrivateAccess = "true"))
    class UStaticMeshComponent *RearMirrorChassisSM;
    FTransform RearMirrorChassisTransform;

  private: // AI controller
    class AWheeledVehicleAIController *AI_Player = nullptr;
    void InitAIPlayer();
    bool bAutopilotEnabled = false;
    void TickAutopilot();

  private: // inputs
    /// NOTE: since there are so many functions here, they are defined in EgoInputs.cpp
    struct DReyeVR::UserInputs VehicleInputs; // struct for user inputs
    // Vehicle control functions (additive for multiple input modalities (kbd/logi))
    void AddSteering(float SteeringInput);
    void AddThrottle(float ThrottleInput);
    void AddBrake(float BrakeInput);
    void TickVehicleInputs();
    bool bReverse;

    // "button presses" should have both a "Press" and "Release" function
    // And, if using the logitech plugin, should also have an "is rising edge" bool so they can only
    // be pressed after being released (cant double press w/ no release)
    // Reverse toggle
    void PressReverse();
    void ReleaseReverse();
    bool bCanPressReverse = true;
    // turn signals
    bool bEnableTurnSignalAction = true; // tune with "EnableTurnSignalAction" in config
    // left turn signal
    void PressTurnSignalL();
    void ReleaseTurnSignalL();
    float LeftSignalTimeToDie; // how long until the blinkers go out
    bool bCanPressTurnSignalL = true;
    // right turn signal
    void PressTurnSignalR();
    void ReleaseTurnSignalR();
    float RightSignalTimeToDie; // how long until the blinkers go out
    bool bCanPressTurnSignalR = true;

    // Camera control functions (offset by some amount)
    void CameraPositionAdjust(const FVector &Disp);
    void CameraFwd();
    void CameraBack();
    void CameraLeft();
    void CameraRight();
    void CameraUp();
    void CameraDown();
    void CameraMoveDepth(float Input);
    void CameraMoveHorizontal(float Input);
    void CameraPositionAdjust(bool bForward, bool bRight, bool bBackwards, bool bLeft, bool bUp, bool bDown);

    // changing camera views
    void PressNextCameraView();
    void ReleaseNextCameraView();
    bool bCanPressNextCameraView = true;
    void PressPrevCameraView();
    void ReleasePrevCameraView();
    bool bCanPressPrevCameraView = true;

    // Vehicle parameters
    float ScaleSteeringInput;
    float ScaleThrottleInput;
    float ScaleBrakeInput;
    float SeatMoveSpeedCmPerSecond = 75.f;

  private: // sounds
    UPROPERTY(Category = "Audio", EditAnywhere, BlueprintReadWrite, meta = (AllowPrivateAccess = "true"))
    class UAudioComponent *GearShiftSound; // nice for toggling reverse
    UPROPERTY(Category = "Audio", EditAnywhere, BlueprintReadWrite, meta = (AllowPrivateAccess = "true"))
    class UAudioComponent *TurnSignalSound; // good for turn signals
    UPROPERTY(Category = "Audio", EditAnywhere, BlueprintReadWrite, meta = (AllowPrivateAccess = "true"))
    class UAudioComponent *EgoEngineRevSound; // driver feedback on throttle
    UPROPERTY(Category = "Audio", EditAnywhere, BlueprintReadWrite, meta = (AllowPrivateAccess = "true"))
    class UAudioComponent *EgoCrashSound; // crashing with another actor
    void ConstructEgoSounds();            // needs to be called in the constructor
    virtual void TickSounds(float DeltaSeconds) override;

    // manually overriding these from ACarlaWheeledVehicle
    void ConstructEgoCollisionHandler(); // needs to be called in the constructor
    UFUNCTION()
    void OnEgoOverlapBegin(UPrimitiveComponent *OverlappedComp, AActor *OtherActor, UPrimitiveComponent *OtherComp,
                           int32 OtherBodyIndex, bool bFromSweep, const FHitResult &SweepResult);

  private: // gamemode/level
    void TickGame(float DeltaSeconds);
    class ADReyeVRGameMode *DReyeVRGame;

  private: // dashboard
    // Text Render components (Like the HUD but part of the mesh and works in VR)
    void ConstructDashText(); // needs to be called in the constructor
    UPROPERTY(Category = "Dash", EditDefaultsOnly, BlueprintReadWrite, meta = (AllowPrivateAccess = "true"))
    class UTextRenderComponent *Speedometer;
    UPROPERTY(Category = "Dash", EditDefaultsOnly, BlueprintReadWrite, meta = (AllowPrivateAccess = "true"))
    class UTextRenderComponent *TurnSignals;
    float TurnSignalDuration; // time in seconds
    UPROPERTY(Category = "Dash", EditDefaultsOnly, BlueprintReadWrite, meta = (AllowPrivateAccess = "true"))
    class UTextRenderComponent *GearShifter;
    void UpdateDash();
    float SpeedometerScale = CmPerSecondToXPerHour(true); // scale from CM/s to MPH or KPH (default MPH)

  private: // steering wheel
    UPROPERTY(Category = Steering, EditDefaultsOnly, BlueprintReadWrite, meta = (AllowPrivateAccess = "true"))
    class UStaticMeshComponent *SteeringWheel;
    void ConstructSteeringWheel(); // needs to be called in the constructor
    void DestroySteeringWheel();
    void TickSteeringWheel(const float DeltaTime);
    float MaxSteerAngleDeg;
    float MaxSteerVelocity;
    float SteeringAnimScale;
    bool bWheelFollowAutopilot = true; // disable steering during autopilot and follow AI
    // wheel face buttons
    void InitWheelButtons();
    void UpdateWheelButton(ADReyeVRCustomActor *Button, bool bEnabled);
    class ADReyeVRCustomActor *Button_ABXY_A, *Button_ABXY_B, *Button_ABXY_X, *Button_ABXY_Y;
    class ADReyeVRCustomActor *Button_DPad_Up, *Button_DPad_Down, *Button_DPad_Left, *Button_DPad_Right;
    bool bInitializedButtons = false;
    const FLinearColor ButtonNeutralCol = 0.2f * FLinearColor::White;
    const FLinearColor ButtonPressedCol = 1.5f * FLinearColor::White;
    // wheel face autopilot indicator
    void InitAutopilotIndicator();
    void TickAutopilotIndicator(bool);
    class ADReyeVRCustomActor *AutopilotIndicator;
    bool bInitializedAutopilotIndicator = false;

  private: // other
    void DebugLines() const;
    bool bDrawDebugEditor = false;
};
