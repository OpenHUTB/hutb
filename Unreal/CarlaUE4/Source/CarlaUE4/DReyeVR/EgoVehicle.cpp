#include "EgoVehicle.h"
#include "Carla/Actor/ActorAttribute.h"             // FActorAttribute
#include "Carla/Actor/ActorRegistry.h"              // Register
#include "Carla/Game/CarlaStatics.h"                // GetCurrentEpisode
#include "Carla/Vehicle/CarlaWheeledVehicleState.h" // ECarlaWheeledVehicleState
#include "DReyeVRPawn.h"                            // ADReyeVRPawn
#include "DrawDebugHelpers.h"                       // Debug Line/Sphere
#include "Engine/EngineTypes.h"                     // EBlendMode
#include "Engine/World.h"                           // GetWorld
#include "GameFramework/Actor.h"                    // Destroy
#include "Kismet/KismetSystemLibrary.h"             // PrintString, QuitGame
#include "Math/Rotator.h"                           // RotateVector, Clamp
#include "Math/UnrealMathUtility.h"                 // Clamp

#include <algorithm>

// Sets default values
AEgoVehicle::AEgoVehicle(const FObjectInitializer &ObjectInitializer) : Super(ObjectInitializer)
{
    LOG("Constructing Ego Vehicle: %s", *FString(this->GetName()));

    ReadConfigVariables();

    // this actor ticks AFTER the physics simulation is done
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.bStartWithTickEnabled = true;
    PrimaryActorTick.bAllowTickOnDedicatedServer = true;
    PrimaryActorTick.TickGroup = TG_PostPhysics;
    AIControllerClass = AWheeledVehicleAIController::StaticClass();

    // Set up the root position to be the this mesh
    SetRootComponent(GetMesh());

    // Initialize the camera components
    ConstructCameraRoot();

    // Initialize audio components
    ConstructEgoSounds();

    // Initialize mirrors
    ConstructMirrors();

    // Initialize text render components
    ConstructDashText();

    // Initialize the steering wheel
    ConstructSteeringWheel();

    LOG("Finished constructing %s", *FString(this->GetName()));
}

void AEgoVehicle::ReadConfigVariables()
{
    // this matches the BP_XYZ (XYZ) part of the blueprint, or "Vehicle" if just an EgoVehicle
    VehicleType = CleanNameForDReyeVR(GetClass()->GetName());
    LOG("Initializing EgoVehicle: \"%s\"", *VehicleType);
    if (!VehicleType.Equals("Vehicle", ESearchCase::CaseSensitive))
    {
        auto NewParams = ConfigFile(FPaths::Combine(CarlaUE4Path, TEXT("Config/EgoVehicles"), VehicleType + ".ini"));
        if (NewParams.bIsValid())
            VehicleParams = NewParams;
    }

    // if the VehicleParams is invalid, use a default
    if (!VehicleParams.bIsValid())
        VehicleParams = ConfigFile(FPaths::Combine(CarlaUE4Path, TEXT("Config/EgoVehicles/TeslaM3.ini")), false);
    ensure(VehicleParams.bIsValid());

    GeneralParams.Get("EgoVehicle", "EnableTurnSignalAction", bEnableTurnSignalAction);
    GeneralParams.Get("EgoVehicle", "TurnSignalDuration", TurnSignalDuration);
    // mirrors
    auto InitMirrorParams = [this](const FString &Name, struct MirrorParams &Params) {
        Params.Name = Name;
        VehicleParams.Get("Mirrors", Params.Name + "MirrorEnabled", Params.Enabled);
        VehicleParams.Get("Mirrors", Params.Name + "MirrorTransform", Params.MirrorTransform);
        VehicleParams.Get("Mirrors", Params.Name + "ReflectionTransform", Params.ReflectionTransform);
        VehicleParams.Get("Mirrors", Params.Name + "ScreenPercentage", Params.ScreenPercentage);
    };
    InitMirrorParams("Rear", RearMirrorParams);
    InitMirrorParams("Left", LeftMirrorParams);
    InitMirrorParams("Right", RightMirrorParams);
    // rear mirror chassis
    VehicleParams.Get("Mirrors", "RearMirrorChassisTransform", RearMirrorChassisTransform);
    // steering wheel
    VehicleParams.Get("SteeringWheel", "MaxSteerAngleDeg", MaxSteerAngleDeg);
    VehicleParams.Get("SteeringWheel", "SteeringScale", SteeringAnimScale);
    // other/cosmetic
    GeneralParams.Get("EgoVehicle", "DrawDebugEditor", bDrawDebugEditor);
    // inputs
    GeneralParams.Get("VehicleInputs", "ScaleSteeringDamping", ScaleSteeringInput);
    GeneralParams.Get("VehicleInputs", "ScaleThrottleInput", ScaleThrottleInput);
    GeneralParams.Get("VehicleInputs", "ScaleBrakeInput", ScaleBrakeInput);
    GeneralParams.Get("CameraParams", "SeatMoveSpeedCmPerSecond", SeatMoveSpeedCmPerSecond);
    // replay
    GeneralParams.Get("Replayer", "CameraFollowHMD", bCameraFollowHMD);
}

void AEgoVehicle::BeginPlay()
{
    // Called when the game starts or when spawned
    Super::BeginPlay();

    // Get information about the world
    World = GetWorld();
    ensure(World != nullptr);

    // initialize
    InitAIPlayer();

    // Bug-workaround for initial delay on throttle; see https://github.com/carla-simulator/carla/issues/1640
    this->GetVehicleMovementComponent()->SetTargetGear(1, true);

    // get the GameMode script
    SetGame(Cast<ADReyeVRGameMode>(UGameplayStatics::GetGameMode(World)));

    BeginThirdPersonCameraInit();

    LOG("Initialized DReyeVR EgoVehicle");
}

void AEgoVehicle::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    // https://docs.unrealengine.com/4.27/en-US/API/Runtime/Engine/Engine/EEndPlayReason__Type/
    if (EndPlayReason == EEndPlayReason::Destroyed)
    {
        LOG("DReyeVR EgoVehicle is being destroyed! You'll need to spawn another one!");
        if (GetGame())
        {
            GetGame()->PossessSpectator();
        }
    }

    if (this->Pawn)
    {
        this->Pawn->SetEgoVehicle(nullptr);
    }
}

void AEgoVehicle::BeginDestroy()
{
    Super::BeginDestroy();

    // destroy all spawned entities
    if (EgoSensor.IsValid())
        EgoSensor.Get()->Destroy();

    DestroySteeringWheel();

    LOG("EgoVehicle has been destroyed");
}

// Called every frame
void AEgoVehicle::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

    // Get the current data from the AEgoSensor and use it
    UpdateSensor(DeltaSeconds);

    // Update the positions based off replay data
    ReplayTick();

    // Draw debug lines on editor
    DebugLines();

    // Render EgoVehicle dashboard
    UpdateDash();

    // Update the steering wheel to be responsive to user input
    TickSteeringWheel(DeltaSeconds);

    // Ensure appropriate autopilot functionality is accessible from EgoVehicle
    TickAutopilot();

    // Update the world level
    TickGame(DeltaSeconds);

    // Tick vehicle controls
    TickVehicleInputs();
}

/// ========================================== ///
/// ----------------:CAMERA:------------------ ///
/// ========================================== ///

template <typename T> T *AEgoVehicle::CreateEgoObject(const FString &Name, const FString &Suffix)
{
    // create default subobject with this name and (optionally) add a suffix
    // https://docs.unrealengine.com/4.26/en-US/API/Runtime/CoreUObject/UObject/UObject/CreateDefaultSubobject/2/
    // see also: https://dev.epicgames.com/community/snippets/0bk/actor-component-creation
    // if the blueprint gets corrupted (ex. object details no longer visible), reparent to BaseVehiclePawn then back
    T *Ret = UObject::CreateDefaultSubobject<T>(FName(*(Name + Suffix)));
    // disabling tick for these objects by default
    Ret->SetComponentTickEnabled(false);
    Ret->PrimaryComponentTick.bCanEverTick = false;
    Ret->PrimaryComponentTick.bStartWithTickEnabled = false;
    return Ret;
}

void AEgoVehicle::ConstructCameraRoot()
{
    // Spawn the RootComponent and Camera for the VR camera
    VRCameraRoot = CreateEgoObject<USceneComponent>("VRCameraRoot");
    check(VRCameraRoot != nullptr);
    VRCameraRoot->SetupAttachment(GetRootComponent()); // The vehicle blueprint itself
    VRCameraRoot->SetRelativeLocation(FVector::ZeroVector);
    VRCameraRoot->SetRelativeRotation(FRotator::ZeroRotator);

    // add first-person driver's seat
    CameraTransforms.Add("DriversSeat", VehicleParams.Get<FTransform>("CameraPose", "DriversSeat"));

    SetCameraRootPose("DriversSeat");
}

void AEgoVehicle::BeginThirdPersonCameraInit()
{
    // add third-person views

    std::vector<FString> CameraPoses = {
        "ThirdPerson",  // 2nd
        "BirdsEyeView", // 3rd
        "Front",        // 4th
    };
    UBoxComponent *Bounds = this->GetVehicleBoundingBox();
    ensure(Bounds != nullptr);
    if (Bounds != nullptr)
    {
        const FVector BoundingBoxSize = Bounds->GetScaledBoxExtent();
        LOG("Calculated EgoVehicle bounding box: %s", *BoundingBoxSize.ToString());
        for (FString &Key : CameraPoses)
        {
            FTransform Transform = GeneralParams.Get<FTransform>("CameraPose", Key);
            Transform.SetLocation(Transform.GetLocation() * BoundingBoxSize); // scale by bounding box
            CameraTransforms.Add(Key, Transform);
        }
    }

    CameraTransforms.GenerateKeyArray(CameraPoseKeys);
    ensure(CameraPoseKeys.Num() > 0);

    // assign the starting camera root pose to the given starting pose
    FString StartingPose = GeneralParams.Get<FString>("CameraPose", "StartingPose");
    SetCameraRootPose(StartingPose);
}

void AEgoVehicle::SetCameraRootPose(const FString &CameraPoseName)
{
    FTransform NewCameraTransform;
    // given a string (key), index into the map to obtain the FTransform
    if (CameraTransforms.Contains(CameraPoseName))
    {
        NewCameraTransform = CameraTransforms[CameraPoseName];
    }
    else
    {
        LOG_WARN("Unable to find camera pose named \"%s\". Defaulting to driver's seat!", *CameraPoseName);
        NewCameraTransform = CameraTransforms["DriversSeat"];
    }
    SetCameraRootPose(NewCameraTransform);
}

size_t AEgoVehicle::GetNumCameraPoses() const
{
    return CameraTransforms.Num();
}

void AEgoVehicle::SetCameraRootPose(size_t CameraPoseIdx)
{
    if (CameraPoseKeys.Num() == 0)
        return;
    // allow setting the camera root by indexing into CameraTransforms array
    CameraPoseIdx = std::min(CameraPoseIdx, static_cast<size_t>(CameraPoseKeys.Num() - 1));
    const auto &Key = CameraPoseKeys[CameraPoseIdx];
    const FTransform *NewPose = CameraTransforms.Find(Key);
    if (NewPose == nullptr)
    {
        LOG_ERROR("Unable to find camera pose \"%s\"", *Key);
        return;
    }
    SetCameraRootPose(*NewPose);
}

void AEgoVehicle::SetCameraRootPose(const FTransform &CameraPoseTransform)
{
    // sets the base posision of the Camera root (where the camera is at "rest")
    this->CameraPose = CameraPoseTransform;
    // LOG("Setting camera pose to: %s", *(CameraPose + CameraPoseOffset).ToString());

    // First, set the root of the camera to the driver's seat head pos
    VRCameraRoot->SetRelativeLocation(CameraPose.GetLocation() + CameraPoseOffset.GetLocation());
    VRCameraRoot->SetRelativeRotation(CameraPose.Rotator() + CameraPoseOffset.Rotator());
}

void AEgoVehicle::SetPawn(ADReyeVRPawn *PawnIn)
{
    ensure(VRCameraRoot != nullptr);
    this->Pawn = PawnIn;
    ensure(Pawn != nullptr);
    this->FirstPersonCam = Pawn->GetCamera();
    ensure(FirstPersonCam != nullptr);
    FAttachmentTransformRules F(EAttachmentRule::KeepRelative, false);
    Pawn->AttachToComponent(VRCameraRoot, F);
    FirstPersonCam->AttachToComponent(VRCameraRoot, F);
    // Then set the actual camera to be at its origin (attached to VRCameraRoot)
    FirstPersonCam->SetRelativeLocation(FVector::ZeroVector);
    FirstPersonCam->SetRelativeRotation(FRotator::ZeroRotator);
}

const FString &AEgoVehicle::GetVehicleType() const
{
    return VehicleType;
}
const UCameraComponent *AEgoVehicle::GetCamera() const
{
    ensure(FirstPersonCam != nullptr);
    return FirstPersonCam;
}
UCameraComponent *AEgoVehicle::GetCamera()
{
    ensure(FirstPersonCam != nullptr);
    return FirstPersonCam;
}
FVector AEgoVehicle::GetCameraOffset() const
{
    return VRCameraRoot->GetComponentLocation();
}
FVector AEgoVehicle::GetCameraPosn() const
{
    return GetCamera() ? GetCamera()->GetComponentLocation() : FVector::ZeroVector;
}
FVector AEgoVehicle::GetNextCameraPosn(const float DeltaSeconds) const
{
    // usually only need this is tick before physics
    return GetCameraPosn() + DeltaSeconds * GetVelocity();
}
FRotator AEgoVehicle::GetCameraRot() const
{
    return GetCamera() ? GetCamera()->GetComponentRotation() : FRotator::ZeroRotator;
}
class AEgoSensor *AEgoVehicle::GetSensor()
{
    // tries to return the EgoSensor pointer if it is safe to do so, else re-spawn it
    return SafePtrGet<AEgoSensor>("EgoSensor", EgoSensor, [&](void) { this->InitSensor(); });
}

const class AEgoSensor *AEgoVehicle::GetSensor() const
{
    // tries to return the EgoSensor pointer if it is safe to do so, else re-spawn it
    return const_cast<AEgoSensor *>(const_cast<AEgoVehicle *>(this)->GetSensor());
}

const struct ConfigFile &AEgoVehicle::GetVehicleParams() const
{
    return VehicleParams;
}

/// ========================================== ///
/// ---------------:AIPLAYER:----------------- ///
/// ========================================== ///

void AEgoVehicle::InitAIPlayer()
{
    this->SpawnDefaultController(); // spawns default (AI) controller and gets possessed by it
    auto PlayerController = GetController();
    ensure(PlayerController != nullptr);
    AI_Player = Cast<AWheeledVehicleAIController>(PlayerController);
    check(AI_Player != nullptr);
    SetAutopilot(false); // initially no autopilot enabled
}

void AEgoVehicle::SetAutopilot(const bool AutopilotOn)
{
    if (!AI_Player)
        return;
    bAutopilotEnabled = AutopilotOn;
    AI_Player->SetAutopilot(bAutopilotEnabled);
    AI_Player->SetStickyControl(bAutopilotEnabled);
    // AI_Player->SetActorTickEnabled(bAutopilotEnabled); // want the controller to always tick!
}

bool AEgoVehicle::GetAutopilotStatus() const
{
    return bAutopilotEnabled;
}

void AEgoVehicle::TickAutopilot()
{
    ensure(AI_Player != nullptr);
    if (AI_Player != nullptr)
    {
        bAutopilotEnabled = AI_Player->IsAutopilotEnabled();
        TickAutopilotIndicator(bAutopilotEnabled);
    }
}

/// ========================================== ///
/// ----------------:SENSOR:------------------ ///
/// ========================================== ///

void AEgoVehicle::InitSensor()
{
    // update the world on refresh (ex. --reloadWorld)
    World = GetWorld();
    check(World != nullptr);
    // Spawn the EyeTracker Carla sensor and attach to Ego-Vehicle:
    {
        UCarlaEpisode *Episode = UCarlaStatics::GetCurrentEpisode(World);
        check(Episode != nullptr);
        FActorDefinition EgoSensorDef = FindDefnInRegistry(Episode, AEgoSensor::StaticClass());
        FActorDescription Description;
        { // create a Description from the Definition to spawn the actor
            Description.UId = EgoSensorDef.UId;
            Description.Id = EgoSensorDef.Id;
            Description.Class = EgoSensorDef.Class;
            // add all the attributes from the definition to the description
            for (FActorAttribute A : EgoSensorDef.Attributes)
            {
                Description.Variations.Add(A.Id, std::move(A));
            }
        }
        // calls Episode::SpawnActor => SpawnActorWithInfo => ActorDispatcher->SpawnActor => SpawnFunctions[UId]
        FTransform SpawnPt = FTransform(FRotator::ZeroRotator, GetCameraPosn(), FVector::OneVector);
        EgoSensor = static_cast<AEgoSensor *>(Episode->SpawnActor(SpawnPt, Description));
    }
    check(EgoSensor.IsValid());
    // Attach the EgoSensor as a child to the EgoVehicle
    EgoSensor.Get()->SetOwner(this);
    EgoSensor.Get()->AttachToActor(this, FAttachmentTransformRules::KeepRelativeTransform);
    EgoSensor.Get()->SetActorTransform(FTransform::Identity, false, nullptr, ETeleportType::TeleportPhysics);
    EgoSensor.Get()->SetEgoVehicle(this);
}

void AEgoVehicle::ReplayTick()
{
    if (!EgoSensor.IsValid())
        return;
    const bool bIsReplaying = EgoSensor.Get()->IsReplaying();
    // need to enable/disable VehicleMesh simulation
    class USkeletalMeshComponent *VehicleMesh = GetMesh();
    if (VehicleMesh)
        VehicleMesh->SetSimulatePhysics(!bIsReplaying); // disable physics when replaying (teleporting)
    if (FirstPersonCam)
        FirstPersonCam->bLockToHmd = !bIsReplaying; // only lock orientation and position to HMD when not replaying

    // perform all sensor updates that occur when replaying
    if (bIsReplaying)
    {
        // this gets reached when the simulator is replaying data from a carla log
        const DReyeVR::AggregateData *Replay = EgoSensor.Get()->GetData();

        // include positional update here, else there is lag/jitter between the camera and the vehicle
        // since the Carla Replayer tick differs from the EgoVehicle tick
        const FTransform ReplayTransform(Replay->GetVehicleRotation(), // FRotator (Rotation)
                                         Replay->GetVehicleLocation(), // FVector (Location)
                                         FVector::OneVector);          // FVector (Scale3D)
        // see https://docs.unrealengine.com/4.26/en-US/API/Runtime/Engine/Engine/ETeleportType/
        SetActorTransform(ReplayTransform, false, nullptr, ETeleportType::TeleportPhysics);

        // set the camera reenactment orientation
        {
            const FTransform CameraOrientation =
                bCameraFollowHMD // follow HMD reenacts all the head movmeents that were recorded
                    ? FTransform(Replay->GetCameraRotation(), Replay->GetCameraLocation(), FVector::OneVector)
                    : FTransform::Identity; // otherwise just point forward (neutral position)
            FirstPersonCam->SetRelativeTransform(CameraOrientation, false, nullptr, ETeleportType::TeleportPhysics);
        }

        // overwrite vehicle inputs to use the replay data
        VehicleInputs = Replay->GetUserInputs();
    }
}

void AEgoVehicle::UpdateSensor(const float DeltaSeconds)
{
    if (!EgoSensor.IsValid()) // Spawn and attach the EgoSensor
    {
        // unfortunately World->SpawnActor *sometimes* fails if used in BeginPlay so
        // calling it once in the tick is fine to avoid this crash.
        InitSensor();
    }

    if (!EgoSensor.IsValid())
    {
        LOG_WARN("EgoSensor initialization failed!");
        return;
    }

    // Explicitly update the EgoSensor here, synchronized with EgoVehicle tick
    EgoSensor.Get()->ManualTick(DeltaSeconds); // Ensures we always get the latest data
}

/// ========================================== ///
/// ----------------:MIRROR:------------------ ///
/// ========================================== ///

void AEgoVehicle::MirrorParams::Initialize(class UStaticMeshComponent *MirrorSM,
                                           class UPlanarReflectionComponent *Reflection,
                                           class USkeletalMeshComponent *VehicleMesh)
{
    check(MirrorSM != nullptr);
    MirrorSM->SetupAttachment(VehicleMesh);
    MirrorSM->SetRelativeLocation(MirrorTransform.GetLocation());
    MirrorSM->SetRelativeRotation(MirrorTransform.Rotator());
    MirrorSM->SetRelativeScale3D(MirrorTransform.GetScale3D());
    MirrorSM->SetGenerateOverlapEvents(false); // don't collide with itself
    MirrorSM->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    MirrorSM->SetVisibility(Enabled);

    check(Reflection != nullptr);
    Reflection->SetupAttachment(MirrorSM);
    Reflection->SetRelativeLocation(ReflectionTransform.GetLocation());
    Reflection->SetRelativeRotation(ReflectionTransform.Rotator());
    Reflection->SetRelativeScale3D(ReflectionTransform.GetScale3D());
    Reflection->NormalDistortionStrength = 0.0f;
    Reflection->PrefilterRoughness = 0.0f;
    Reflection->DistanceFromPlaneFadeoutStart = 1500.f;
    Reflection->DistanceFromPlaneFadeoutEnd = 0.f;
    Reflection->AngleFromPlaneFadeStart = 0.f;
    Reflection->AngleFromPlaneFadeEnd = 90.f;
    Reflection->PrefilterRoughnessDistance = 10000.f;
    Reflection->ScreenPercentage = ScreenPercentage; // change this to reduce quality & improve performance
    Reflection->bShowPreviewPlane = false;
    Reflection->HideComponent(VehicleMesh);
    Reflection->SetVisibility(Enabled);
    /// TODO: use USceneCaptureComponent::ShowFlags to define what gets rendered in the mirror
    // https://docs.unrealengine.com/4.27/en-US/API/Runtime/Engine/FEngineShowFlags/
}

void AEgoVehicle::ConstructMirrors()
{

    class USkeletalMeshComponent *VehicleMesh = GetMesh();
    /// Rear mirror
    {
        // the rear mirror is interesting bc we have 3 components: the mirror (specular), the mirror chassis
        // (what holds the mirror, typically lambertian), and the planar reflection (invisible but "reflection")
        // other mirrors currently only use the mirror and planar reflection, as we didn't cut out the chassis for them
        RearMirrorSM = CreateEgoObject<UStaticMeshComponent>(RearMirrorParams.Name + "MirrorSM");
        RearReflection = CreateEgoObject<UPlanarReflectionComponent>(RearMirrorParams.Name + "Refl");
        RearMirrorChassisSM = CreateEgoObject<UStaticMeshComponent>(RearMirrorParams.Name + "MirrorChassisSM");

        RearMirrorParams.Initialize(RearMirrorSM, RearReflection, VehicleMesh);
        // also add the chassis for this mirror
        RearMirrorChassisSM->SetupAttachment(VehicleMesh);
        RearMirrorChassisSM->SetRelativeLocation(RearMirrorChassisTransform.GetLocation());
        RearMirrorChassisSM->SetRelativeRotation(RearMirrorChassisTransform.Rotator());
        RearMirrorChassisSM->SetRelativeScale3D(RearMirrorChassisTransform.GetScale3D());
        RearMirrorChassisSM->SetGenerateOverlapEvents(false); // don't collide with itself
        RearMirrorChassisSM->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        RearMirrorChassisSM->SetVisibility(RearMirrorParams.Enabled);
        RearMirrorSM->SetupAttachment(RearMirrorChassisSM);
        RearReflection->HideComponent(RearMirrorChassisSM); // don't show this in the reflection
    }
    /// Left mirror
    {
        LeftMirrorSM = CreateEgoObject<UStaticMeshComponent>(LeftMirrorParams.Name + "MirrorSM");
        LeftReflection = CreateEgoObject<UPlanarReflectionComponent>(LeftMirrorParams.Name + "Refl");
        LeftMirrorParams.Initialize(LeftMirrorSM, LeftReflection, VehicleMesh);
    }
    /// Right mirror
    {
        RightMirrorSM = CreateEgoObject<UStaticMeshComponent>(RightMirrorParams.Name + "MirrorSM");
        RightReflection = CreateEgoObject<UPlanarReflectionComponent>(RightMirrorParams.Name + "Refl");
        RightMirrorParams.Initialize(RightMirrorSM, RightReflection, VehicleMesh);
    }
}

/// ========================================== ///
/// ----------------:SOUNDS:------------------ ///
/// ========================================== ///

template <typename T> bool FindSound(const FString &Section, const FString &Variable, UAudioComponent *Out)
{
    if (Out != nullptr) // TODO: check that the key is present
    {
        const FString PathStr = GeneralParams.Get<FString>(Section, Variable);
        if (!PathStr.IsEmpty())
        {
            ConstructorHelpers::FObjectFinder<T> FoundSound(*PathStr);
            if (FoundSound.Succeeded())
            {
                Out->SetSound(FoundSound.Object);
                return true;
            }
        }
    }
    return false;
}

void AEgoVehicle::ConstructEgoSounds()
{
    // shouldn't override this method in ACarlaWHeeledVehicle because it will be
    // used in the constructor and calling virtual methods in constructor is bad:
    // https://stackoverflow.com/questions/962132/calling-virtual-functions-inside-constructors

    // Initialize ego-centric audio components
    {
        if (EngineRevSound != nullptr)
        {
            EngineRevSound->DestroyComponent(); // from the parent class (default sound)
            EngineRevSound = nullptr;
        }
        EgoEngineRevSound = CreateEgoObject<UAudioComponent>("EgoEngineRevSound");
        FindSound<USoundCue>("Sound", "DefaultEngineRev", EgoEngineRevSound);
        EgoEngineRevSound->SetupAttachment(GetRootComponent()); // attach to self
        EgoEngineRevSound->bAutoActivate = true;                // start playing on begin
        EngineLocnInVehicle = VehicleParams.Get<FVector>("Sounds", "EngineLocn");
        EgoEngineRevSound->SetRelativeLocation(EngineLocnInVehicle); // location of "engine" in vehicle (3D sound)
        EgoEngineRevSound->SetFloatParameter(FName("RPM"), 0.f);     // initially idle
        EgoEngineRevSound->bAutoDestroy = false;                     // No automatic destroy, persist along with vehicle
        check(EgoEngineRevSound != nullptr);
    }

    {
        if (CrashSound != nullptr)
        {
            CrashSound->DestroyComponent(); // from the parent class (default sound)
            CrashSound = nullptr;
        }
        EgoCrashSound = CreateEgoObject<UAudioComponent>("EgoCarCrash");
        FindSound<USoundCue>("Sound", "DefaultCrashSound", EgoCrashSound);
        EgoCrashSound->SetupAttachment(GetRootComponent());
        EgoCrashSound->bAutoActivate = false;
        EgoCrashSound->bAutoDestroy = false;
        check(EgoCrashSound != nullptr);
    }

    {
        GearShiftSound = CreateEgoObject<UAudioComponent>("GearShift");
        FindSound<USoundWave>("Sound", "DefaultGearShiftSound", GearShiftSound);
        GearShiftSound->SetupAttachment(GetRootComponent());
        GearShiftSound->bAutoActivate = false;
        check(GearShiftSound != nullptr);
    }

    {
        TurnSignalSound = CreateEgoObject<UAudioComponent>("TurnSignal");
        FindSound<USoundWave>("Sound", "DefaultTurnSignalSound", TurnSignalSound);
        TurnSignalSound->SetupAttachment(GetRootComponent());
        TurnSignalSound->bAutoActivate = false;
        check(TurnSignalSound != nullptr);
    }

    ConstructEgoCollisionHandler();
}

void AEgoVehicle::PlayGearShiftSound(const float DelayBeforePlay) const
{
    if (GearShiftSound != nullptr)
        GearShiftSound->Play(DelayBeforePlay);
}

void AEgoVehicle::PlayTurnSignalSound(const float DelayBeforePlay) const
{
    if (TurnSignalSound != nullptr)
        TurnSignalSound->Play(DelayBeforePlay);
}

void AEgoVehicle::SetVolume(const float VolumeIn)
{
    if (EgoEngineRevSound)
        EgoEngineRevSound->SetVolumeMultiplier(VolumeIn);
    if (EgoCrashSound)
        EgoCrashSound->SetVolumeMultiplier(VolumeIn);
    if (GearShiftSound)
        GearShiftSound->SetVolumeMultiplier(VolumeIn);
    if (TurnSignalSound)
        TurnSignalSound->SetVolumeMultiplier(VolumeIn);
    Super::SetVolume(VolumeIn);
}

void AEgoVehicle::TickSounds(float DeltaSeconds)
{
    // Respect the global vehicle volume param
    SetVolume(ACarlaWheeledVehicle::Volume);

    if (EgoEngineRevSound)
    {
        if (!EgoEngineRevSound->IsPlaying())
            EgoEngineRevSound->Play(); // turn on the engine sound if not already on
        float RPM = FMath::Clamp(GetVehicleMovementComponent()->GetEngineRotationSpeed(), 0.f, 5650.0f);
        EgoEngineRevSound->SetFloatParameter(FName("RPM"), RPM);
    }

    // add other sounds that need tick-level granularity here...
}

void AEgoVehicle::ConstructEgoCollisionHandler()
{
    // using Carla's GetVehicleBoundingBox function
    UBoxComponent *Bounds = this->GetVehicleBoundingBox();
    if (Bounds != nullptr)
    {
        Bounds->SetGenerateOverlapEvents(true);
        Bounds->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
        Bounds->SetCollisionProfileName(TEXT("DReyeVRTrigger"));
        Bounds->OnComponentBeginOverlap.AddDynamic(this, &AEgoVehicle::OnEgoOverlapBegin);
    }
}

void AEgoVehicle::OnEgoOverlapBegin(UPrimitiveComponent *OverlappedComp, AActor *OtherActor,
                                    UPrimitiveComponent *OtherComp, int32 OtherBodyIndex, bool bFromSweep,
                                    const FHitResult &SweepResult)
{
    if (OtherActor == nullptr || OtherActor == this)
        return;

    // can be more flexible, such as having collisions with static props or people too
    if (EnableCollisionForActor(OtherActor))
    {
        // move the sound 1m in the direction of the collision
        FVector SoundEmitLocation = 100.f * (this->GetActorLocation() - OtherActor->GetActorLocation()).GetSafeNormal();
        SoundEmitLocation.Z = 75.f; // Make the sound emitted not at the ground (75cm off ground)
        if (EgoCrashSound != nullptr)
        {
            EgoCrashSound->SetRelativeLocation(SoundEmitLocation);
            EgoCrashSound->Play(0.f);
            // have at least 0.5s of buffer between collision audio
            CollisionCooldownTime = GetWorld()->GetTimeSeconds() + 0.5f;
        }
    }
}

/// ========================================== ///
/// -----------------:DASH:------------------- ///
/// ========================================== ///

void AEgoVehicle::ConstructDashText() // dashboard text (speedometer, turn signals, gear shifter)
{
    // Create speedometer
    if (VehicleParams.Get<bool>("Dashboard", "SpeedometerEnabled"))
    {
        Speedometer = CreateEgoObject<UTextRenderComponent>("Speedometer");
        Speedometer->AttachToComponent(GetRootComponent(), FAttachmentTransformRules::KeepRelativeTransform);
        Speedometer->SetRelativeTransform(VehicleParams.Get<FTransform>("Dashboard", "SpeedometerTransform"));
        Speedometer->SetTextRenderColor(FColor::Red);
        Speedometer->SetText(FText::FromString("0"));
        Speedometer->SetXScale(1.f);
        Speedometer->SetYScale(1.f);
        Speedometer->SetWorldSize(10); // scale the font with this
        Speedometer->SetVerticalAlignment(EVerticalTextAligment::EVRTA_TextCenter);
        Speedometer->SetHorizontalAlignment(EHorizTextAligment::EHTA_Center);
        SpeedometerScale = CmPerSecondToXPerHour(GeneralParams.Get<bool>("EgoVehicle", "SpeedometerInMPH"));
        check(Speedometer != nullptr);
    }

    // Create turn signals
    if (VehicleParams.Get<bool>("Dashboard", "TurnSignalsEnabled"))
    {
        TurnSignals = CreateEgoObject<UTextRenderComponent>("TurnSignals");
        TurnSignals->AttachToComponent(GetRootComponent(), FAttachmentTransformRules::KeepRelativeTransform);
        TurnSignals->SetRelativeTransform(VehicleParams.Get<FTransform>("Dashboard", "TurnSignalsTransform"));
        TurnSignals->SetTextRenderColor(FColor::Red);
        TurnSignals->SetText(FText::FromString(""));
        TurnSignals->SetXScale(1.f);
        TurnSignals->SetYScale(1.f);
        TurnSignals->SetWorldSize(10); // scale the font with this
        TurnSignals->SetVerticalAlignment(EVerticalTextAligment::EVRTA_TextCenter);
        TurnSignals->SetHorizontalAlignment(EHorizTextAligment::EHTA_Center);
        check(TurnSignals != nullptr);
    }

    // Create gear shifter
    if (VehicleParams.Get<bool>("Dashboard", "GearShifterEnabled"))
    {
        GearShifter = CreateEgoObject<UTextRenderComponent>("GearShifter");
        GearShifter->AttachToComponent(GetRootComponent(), FAttachmentTransformRules::KeepRelativeTransform);
        GearShifter->SetRelativeTransform(VehicleParams.Get<FTransform>("Dashboard", "GearShifterTransform"));
        GearShifter->SetTextRenderColor(FColor::Red);
        GearShifter->SetText(FText::FromString("D"));
        GearShifter->SetXScale(1.f);
        GearShifter->SetYScale(1.f);
        GearShifter->SetWorldSize(10); // scale the font with this
        GearShifter->SetVerticalAlignment(EVerticalTextAligment::EVRTA_TextCenter);
        GearShifter->SetHorizontalAlignment(EHorizTextAligment::EHTA_Center);
        check(GearShifter != nullptr);
    }
}

void AEgoVehicle::UpdateDash()
{
    // Draw text components
    float XPH; // miles-per-hour or km-per-hour
    if (EgoSensor.IsValid() && EgoSensor.Get()->IsReplaying())
    {
        const DReyeVR::AggregateData *Replay = EgoSensor.Get()->GetData();
        XPH = Replay->GetVehicleVelocity() * SpeedometerScale; // FwdSpeed is in cm/s
        const auto &ReplayInputs = Replay->GetUserInputs();
        if (ReplayInputs.ToggledReverse)
            PressReverse();
        else
            ReleaseReverse();

        if (bEnableTurnSignalAction)
        {
            if (ReplayInputs.TurnSignalLeft)
                PressTurnSignalL();
            else
                ReleaseTurnSignalL();

            if (ReplayInputs.TurnSignalRight)
                PressTurnSignalR();
            else
                ReleaseTurnSignalR();
        }
    }
    else
    {
        XPH = GetVehicleForwardSpeed() * SpeedometerScale; // FwdSpeed is in cm/s
    }

    if (Speedometer != nullptr)
    {
        const FString Data = FString::FromInt(int(FMath::RoundHalfFromZero(XPH)));
        Speedometer->SetText(FText::FromString(Data));
    }

    if (bEnableTurnSignalAction && TurnSignals != nullptr)
    {
        // Draw the signals
        float Now = GetWorld()->GetTimeSeconds();
        const float StartTime = std::max(RightSignalTimeToDie, LeftSignalTimeToDie) - TurnSignalDuration;
        FString TurnSignalStr = "";
        constexpr static float TurnSignalBlinkRate = 0.4f; // rate of blinking
        if (std::fmodf(Now - StartTime, TurnSignalBlinkRate * 2) < TurnSignalBlinkRate)
        {
            if (Now < RightSignalTimeToDie)
                TurnSignalStr = ">>>";
            else if (Now < LeftSignalTimeToDie)
                TurnSignalStr = "<<<";
        }
        TurnSignals->SetText(FText::FromString(TurnSignalStr));
    }

    if (GearShifter != nullptr)
    {
        // Draw the gear shifter
        GearShifter->SetText(bReverse ? FText::FromString("R") : FText::FromString("D"));
    }
}

/// ========================================== ///
/// -----------------:WHEEL:------------------ ///
/// ========================================== ///

void AEgoVehicle::ConstructSteeringWheel()
{
    const bool bEnableSteeringWheel = VehicleParams.Get<bool>("SteeringWheel", "Enabled");
    SteeringWheel = CreateEgoObject<UStaticMeshComponent>("SteeringWheel");
    SteeringWheel->SetupAttachment(GetRootComponent()); // The vehicle blueprint itself
    SteeringWheel->SetRelativeLocation(VehicleParams.Get<FVector>("SteeringWheel", "InitLocation"));
    SteeringWheel->SetRelativeRotation(VehicleParams.Get<FRotator>("SteeringWheel", "InitRotation"));
    SteeringWheel->SetGenerateOverlapEvents(false); // don't collide with itself
    SteeringWheel->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    SteeringWheel->SetVisibility(bEnableSteeringWheel);
}

void AEgoVehicle::InitAutopilotIndicator()
{
    bool bEnableAutopilotIndicator = GeneralParams.Get<bool>("WheelFace", "EnableAutopilotIndicator");
    if (SteeringWheel == nullptr || World == nullptr || bEnableAutopilotIndicator == false)
        return;
    AutopilotIndicator = ADReyeVRCustomActor::CreateNew(SM_SPHERE, MAT_OPAQUE, World, "AI_Indicator");
    const FVector AutopilotIndicatorLocation = GeneralParams.Get<FVector>("WheelFace", "AutopilotIndicatorLoc");
    const float AutopilotIndicatorSize = GeneralParams.Get<float>("WheelFace", "AutopilotIndicatorSize");
    AutopilotIndicator->SetActorLocation(AutopilotIndicatorLocation);
    check(AutopilotIndicator != nullptr);
    AutopilotIndicator->Activate();
    AutopilotIndicator->SetActorScale3D(AutopilotIndicatorSize * FVector::OneVector);
    AutopilotIndicator->AttachToComponent(SteeringWheel, FAttachmentTransformRules::KeepRelativeTransform);
    AutopilotIndicator->MaterialParams.BaseColor = ButtonNeutralCol; // close to off
    AutopilotIndicator->MaterialParams.Emissive = ButtonNeutralCol;  // close to off
    AutopilotIndicator->UpdateMaterial();
    AutopilotIndicator->SetActorTickEnabled(false);      // don't tick these actors (for performance)
    AutopilotIndicator->SetActorRecordEnabled(false);    // don't need to record these actors either
    AutopilotIndicator->GetMesh()->SetCastShadow(false); // don't want shadows (looks weird)
    bInitializedAutopilotIndicator = true;
}

void AEgoVehicle::InitWheelButtons()
{
    bool bEnableWheelFaceButtons = GeneralParams.Get<bool>("WheelFace", "EnableWheelButtons");
    if (SteeringWheel == nullptr || World == nullptr || bEnableWheelFaceButtons == false)
        return;
    // left buttons (dpad)
    Button_DPad_Up = ADReyeVRCustomActor::CreateNew(SM_CONE, MAT_OPAQUE, World, "DPad_Up");       // top on left
    Button_DPad_Right = ADReyeVRCustomActor::CreateNew(SM_CONE, MAT_OPAQUE, World, "DPad_Right"); // right on left
    Button_DPad_Down = ADReyeVRCustomActor::CreateNew(SM_CONE, MAT_OPAQUE, World, "DPad_Down");   // bottom on left
    Button_DPad_Left = ADReyeVRCustomActor::CreateNew(SM_CONE, MAT_OPAQUE, World, "DPad_Left");   // left on left
    // right buttons (ABXY)
    Button_ABXY_Y = ADReyeVRCustomActor::CreateNew(SM_SPHERE, MAT_OPAQUE, World, "ABXY_Y"); // top on right
    Button_ABXY_B = ADReyeVRCustomActor::CreateNew(SM_SPHERE, MAT_OPAQUE, World, "ABXY_B"); // right on right
    Button_ABXY_A = ADReyeVRCustomActor::CreateNew(SM_SPHERE, MAT_OPAQUE, World, "ABXY_A"); // bottom on right
    Button_ABXY_X = ADReyeVRCustomActor::CreateNew(SM_SPHERE, MAT_OPAQUE, World, "ABXY_X"); // left on right

    const FRotator PointLeft(0.f, 0.f, -90.f);
    const FRotator PointRight(0.f, 0.f, 90.f);
    const FRotator PointUp(0.f, 0.f, 0.f);
    const FRotator PointDown(0.f, 0.f, 180.f);

    const FVector LeftCenter = GeneralParams.Get<FVector>("WheelFace", "ABXYLocation");
    const FVector RightCenter = GeneralParams.Get<FVector>("WheelFace", "DpadLocation");

    // increase to separate the buttons more
    const float ButtonDist = GeneralParams.Get<float>("WheelFace", "QuadButtonSpread");

    Button_DPad_Up->SetActorLocation(LeftCenter + ButtonDist * FVector::UpVector);
    Button_DPad_Up->SetActorRotation(PointUp);
    Button_DPad_Right->SetActorLocation(LeftCenter + ButtonDist * FVector::RightVector);
    Button_DPad_Right->SetActorRotation(PointRight);
    Button_DPad_Down->SetActorLocation(LeftCenter + ButtonDist * FVector::DownVector);
    Button_DPad_Down->SetActorRotation(PointDown);
    Button_DPad_Left->SetActorLocation(LeftCenter + ButtonDist * FVector::LeftVector);
    Button_DPad_Left->SetActorRotation(PointLeft);

    // (spheres don't need rotation)
    Button_ABXY_Y->SetActorLocation(RightCenter + ButtonDist * FVector::UpVector);
    Button_ABXY_B->SetActorLocation(RightCenter + ButtonDist * FVector::RightVector);
    Button_ABXY_A->SetActorLocation(RightCenter + ButtonDist * FVector::DownVector);
    Button_ABXY_X->SetActorLocation(RightCenter + ButtonDist * FVector::LeftVector);

    // for applying the same properties on these actors
    auto WheelButtons = {Button_ABXY_A,  Button_ABXY_B,    Button_ABXY_X,    Button_ABXY_Y,
                         Button_DPad_Up, Button_DPad_Down, Button_DPad_Left, Button_DPad_Right};
    for (auto *Button : WheelButtons)
    {
        check(Button != nullptr);
        Button->Activate();
        Button->SetActorScale3D(0.015f * FVector::OneVector);
        Button->AttachToComponent(SteeringWheel, FAttachmentTransformRules::KeepRelativeTransform);
        Button->MaterialParams.BaseColor = ButtonNeutralCol;
        Button->MaterialParams.Emissive = ButtonNeutralCol;
        Button->UpdateMaterial();
        Button->SetActorTickEnabled(false);      // don't tick these actors (for performance)
        Button->SetActorRecordEnabled(false);    // don't need to record these actors either
        Button->GetMesh()->SetCastShadow(false); // don't want shadows (looks weird)
    }
    bInitializedButtons = true;
}

void AEgoVehicle::UpdateWheelButton(ADReyeVRCustomActor *Button, bool bEnabled)
{
    if (Button == nullptr)
        return;
    Button->MaterialParams.BaseColor = bEnabled ? ButtonPressedCol : ButtonNeutralCol;
    Button->MaterialParams.Emissive = bEnabled ? ButtonPressedCol : ButtonNeutralCol;
    Button->UpdateMaterial();
}

void AEgoVehicle::TickAutopilotIndicator(bool bAutopilotEnabled)
{
    if (AutopilotIndicator == nullptr)
        return;
    const FLinearColor On = FLinearColor(0.537, 0.812, 0.941);         // baby blue
    const FLinearColor Off = 0.1f * FLinearColor(0.537, 0.812, 0.941); // baby blue (off)
    AutopilotIndicator->MaterialParams.BaseColor = bAutopilotEnabled ? On : Off;
    AutopilotIndicator->MaterialParams.Emissive = 500.f * (bAutopilotEnabled ? On : Off);
    AutopilotIndicator->UpdateMaterial();
}

void AEgoVehicle::DestroySteeringWheel()
{
    auto WheelButtons = {Button_ABXY_A,  Button_ABXY_B,    Button_ABXY_X,    Button_ABXY_Y,
                         Button_DPad_Up, Button_DPad_Down, Button_DPad_Left, Button_DPad_Right};
    for (auto *Button : WheelButtons)
    {
        if (Button)
        {
            Button->Deactivate();
            Button->Destroy();
        }
    }
}

void AEgoVehicle::TickSteeringWheel(const float DeltaTime)
{
    if (!SteeringWheel)
        return;
    if (!bInitializedButtons)
        InitWheelButtons();
    if (!bInitializedAutopilotIndicator)
        InitAutopilotIndicator();
    const FRotator CurrentRotation = SteeringWheel->GetRelativeRotation();
    FRotator NewRotation = CurrentRotation;
    if (Pawn && Pawn->GetIsLogiConnected() && !GetAutopilotStatus())
    {
        // make the virtual wheel rotation follow the physical steering wheel
        const float RawSteering = GetVehicleInputs().Steering; // this is scaled in SetSteering
        const float TargetAngle = (RawSteering / ScaleSteeringInput) * SteeringAnimScale;
        NewRotation.Roll = TargetAngle;
    }
    else if (GetMesh() && Cast<UVehicleAnimInstance>(GetMesh()->GetAnimInstance()) != nullptr)
    {
        // use the animation (front wheels) steer angle for the steering wheel
        float WheelAngleDeg = GetWheelSteerAngle(EVehicleWheelLocation::Front_Wheel);
        // float MaxWheelAngle = GetMaximumSteerAngle();
        float DeltaAngle = 10.f * (2.0f * WheelAngleDeg - CurrentRotation.Roll);

        // create the new rotation using the deltas
        NewRotation += DeltaTime * FRotator(0.f, 0.f, DeltaAngle);

        // Clamp the roll amount so the wheel can't spin infinitely
        NewRotation.Roll = FMath::Clamp(NewRotation.Roll, -MaxSteerAngleDeg, MaxSteerAngleDeg);
    }
    SteeringWheel->SetRelativeRotation(NewRotation);
}

/// ========================================== ///
/// -----------------:LEVEL:------------------ ///
/// ========================================== ///

void AEgoVehicle::SetGame(ADReyeVRGameMode *Game)
{
    DReyeVRGame = Game;
    check(DReyeVRGame != nullptr);
    check(DReyeVRGame->GetEgoVehicle() == this);

    DReyeVRGame->GetPawn()->BeginEgoVehicle(this, World);
    LOG("Successfully assigned GameMode & controller pawn");
}

ADReyeVRGameMode *AEgoVehicle::GetGame()
{
    return DReyeVRGame;
}

void AEgoVehicle::TickGame(float DeltaSeconds)
{
    if (this->DReyeVRGame != nullptr)
        DReyeVRGame->Tick(DeltaSeconds);
}

/// ========================================== ///
/// ---------------:COSMETIC:----------------- ///
/// ========================================== ///

void AEgoVehicle::DebugLines() const
{
#if WITH_EDITOR

    if (bDrawDebugEditor && EgoSensor.IsValid())
    {
        // Calculate gaze data (in world space) using eye tracker data
        const DReyeVR::AggregateData *Data = EgoSensor.Get()->GetData();
        // Compute World positions and orientations
        const FRotator &WorldRot = FirstPersonCam->GetComponentRotation();
        const FVector &WorldPos = FirstPersonCam->GetComponentLocation();

        // First get the gaze origin and direction and vergence from the EyeTracker Sensor
        const float RayLength = FMath::Max(1.f, Data->GetGazeVergence() / 100.f); // vergence to m (from cm)
        const float VRMeterScale = 100.f;

        // Both eyes
        const FVector CombinedGaze = RayLength * VRMeterScale * Data->GetGazeDir();
        const FVector CombinedOrigin = WorldPos + WorldRot.RotateVector(Data->GetGazeOrigin());

        // Left eye
        const FVector LeftGaze = RayLength * VRMeterScale * Data->GetGazeDir(DReyeVR::Gaze::LEFT);
        const FVector LeftOrigin = WorldPos + WorldRot.RotateVector(Data->GetGazeOrigin(DReyeVR::Gaze::LEFT));

        // Right eye
        const FVector RightGaze = RayLength * VRMeterScale * Data->GetGazeDir(DReyeVR::Gaze::RIGHT);
        const FVector RightOrigin = WorldPos + WorldRot.RotateVector(Data->GetGazeOrigin(DReyeVR::Gaze::RIGHT));

        // Use Absolute Ray Position to draw debug information
        // Rotate and add the gaze ray to the origin
        DrawDebugSphere(World, CombinedOrigin + WorldRot.RotateVector(CombinedGaze), 4.0f, 12, FColor::Blue);

        // Draw individual rays for left and right eye
        DrawDebugLine(World,
                      LeftOrigin,                                        // start line
                      LeftOrigin + 10 * WorldRot.RotateVector(LeftGaze), // end line
                      FColor::Green, false, -1, 0, 1);

        DrawDebugLine(World,
                      RightOrigin,                                         // start line
                      RightOrigin + 10 * WorldRot.RotateVector(RightGaze), // end line
                      FColor::Yellow, false, -1, 0, 1);
    }
#endif
}
