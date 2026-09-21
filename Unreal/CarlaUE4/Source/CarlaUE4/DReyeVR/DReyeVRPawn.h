#pragma once

#include "Camera/CameraComponent.h" // UCameraComponent
#include "Engine/Scene.h"           // FPostProcessSettings
#include "GameFramework/Pawn.h"     // CreatePlayerInputComponent

#ifndef _WIN32
// can only use LogitechWheel plugin on Windows! :(
#undef USE_LOGITECH_PLUGIN
#define USE_LOGITECH_PLUGIN false
#endif

#if USE_LOGITECH_PLUGIN
#include "ILogitechWheelPlugin.h" // LogitechWheel plugin for hardware integration & force feedback
#endif

#include "DReyeVRPawn.generated.h"

class AEgoVehicle;

UCLASS()
class ADReyeVRPawn : public APawn
{
    GENERATED_BODY()

  public:
    ADReyeVRPawn(const FObjectInitializer &ObjectInitializer);

    virtual void SetupPlayerInputComponent(UInputComponent *PlayerInputComponent) override;
    virtual void Tick(float DeltaSeconds) override;

    void BeginPlayer(APlayerController *PlayerIn);
    void BeginEgoVehicle(AEgoVehicle *Vehicle, UWorld *World);

    void SetEgoVehicle(AEgoVehicle *Vehicle)
    {
        EgoVehicle = Vehicle;
    }

    APlayerController *GetPlayer()
    {
        return Player;
    }

    UCameraComponent *GetCamera()
    {
        return FirstPersonCam;
    }

    const UCameraComponent *GetCamera() const
    {
        return FirstPersonCam;
    }

    bool GetIsLogiConnected() const
    {
        return bIsLogiConnected;
    }

  protected:
    virtual void BeginPlay() override;
    virtual void BeginDestroy() override;
    void ReadConfigVariables();

    class UWorld *World = nullptr;
    class AEgoVehicle *EgoVehicle = nullptr;

  private:
    ////////////////:CAMERA:////////////////
    UPROPERTY(Category = Camera, EditAnywhere, BlueprintReadWrite, meta = (AllowPrivateAccess = "true"))
    class UCameraComponent *FirstPersonCam;
    void ConstructCamera();
    float FieldOfView = 90.f; // in degrees
    void NextShader();
    void PrevShader();
    size_t CurrentShaderIdx = 0; // 0th shader is rgb (camera)

    void TickSpectatorScreen(float DeltaSeconds); // to render the spectator screen (VR) or flat-screen hud (non-VR)
    void DrawSpectatorScreen();
    void DrawFlatHUD(float DeltaSeconds);

    ////////////////:STEAMVR:////////////////
    void InitSteamVR();         // Initialize the Head Mounted Display
    void InitSpectator();       // Initialize the VR spectator
    void TickSteamVR();         // Ensure SteamVR is active on every tick
    void InitReticleTexture();  // initializes the spectator-reticle texture
    UTexture2D *ReticleTexture; // UE4 texture for eye reticle
    float HUDScaleVR;           // How much to scale the HUD in VR

    ////////////////:FLATHUD:////////////////
    // (Flat) HUD (NOTE: ONLY FOR NON VR)
    void InitFlatHUD();
    UPROPERTY(Category = HUD, EditDefaultsOnly, BlueprintReadWrite, meta = (AllowPrivateAccess = "true"))
    class ADReyeVRHUD *FlatHUD;
    FVector2D ReticlePos;                // 2D reticle position from eye gaze
    int ReticleSize = 100;               // diameter of reticle (line thickness is 10% of this)
    bool bDrawFlatHud = true;            // whether to draw the flat hud at all (default true, but false in VR)
    bool bDrawFPSCounter = true;         // draw FPS counter in top left corner
    bool bDrawGaze = false;              // whether or not to draw a line for gaze-ray on HUD
    bool bDrawSpectatorReticle = true;   // Reticle used in the VR-spectator mode
    bool bDrawFlatReticle = true;        // Reticle used in the flat mode (uses HUD) (ONLY in non-vr mode)
    bool bEnableSpectatorScreen = false; // don't spent time rendering the spectator screen
    bool bRectangularReticle = false;    // draw the reticle texture on the HUD & Spectator (NOT RECOMMENDED)

    ////////////////:INPUTS:////////////////
    void MouseLookUp(const float mY_Input);
    void MouseTurn(const float mX_Input);
    bool InvertMouseY;
    float ScaleMouseY;
    float ScaleMouseX;

    // keyboard mechanisms to access Axis vehicle control (steering, throttle, brake)
    void SetBrakeKbd(const float in);
    void SetSteeringKbd(const float in);
    void SetThrottleKbd(const float in);
    // most of the time, the participant will use the logi for inputs, but if needed the experimenter
    // can use the keyboard to reposition/takeover without input conflict
    bool bOverrideInputsWithKbd = true; // keyboard > logi priority for inputs

    void SetupEgoVehicleInputComponent(UInputComponent *PlayerInputComponent, AEgoVehicle *EV);
    UInputComponent *InputComponent = nullptr;
    APlayerController *Player = nullptr;

    ////////////////:LOGI:////////////////
    void InitLogiWheel();
    void TickLogiWheel();
    void DestroyLogiWheel(bool DestroyModule);
    bool bLogLogitechWheel = false;
    float LogiThresh = 0.02f;      // threshold for change needed to overtake AI controls
    int SaturationPercentage = 30; // "Level of saturation... comparable to a magnitude"
    int WheelDeviceIdx = 0;        // usually leaving as 0 is fine, only use 1 if 0 is taken
#if USE_LOGITECH_PLUGIN
    struct DIJOYSTATE2 *Old = nullptr; // global "old" struct for the last state
    void LogLogitechPluginStruct(const struct DIJOYSTATE2 *Now);
    void LogitechWheelUpdate();                              // for logitech wheel integration
    void ManageButtonPresses(const DIJOYSTATE2 &WheelState); // for managing button presses
    void ApplyForceFeedback() const;                         // for logitech wheel integration
    float WheelRotationLast, AccelerationPedalLast, BrakePedalLast;
#endif
    bool bIsLogiConnected = false; // check if Logi device is connected (on BeginPlay)
    double NextWheelConnectAttempt = 0.0;
    double NextWheelInputLog = 0.0;
    bool bWheelErrorShown = false;
    int32 WheelProbeCount = 0;
    bool bIsHMDConnected = false;  // checks for HMD connection on BeginPlay
    // default logi plugin behaviour is to set things to 0.5 for some reason
    // "Pedals will output a value of 0.5 until the wheel/pedals receive any kind of input."
    // https://github.com/HARPLab/LogitechWheelPlugin
    bool bPedalsDefaulting = true;
};
