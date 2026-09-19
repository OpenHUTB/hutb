#include "EgoVehicle.h"
#include "Math/NumericLimits.h" // TNumericLimits<float>::Max
#include <string>               // std::string, std::wstring

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#endif

namespace
{
float SeatAxisValue(float Input)
{
    constexpr float DeadZone = 0.15f;
    if (!FMath::IsFinite(Input) || FMath::Abs(Input) <= DeadZone)
        return 0.f;
    Input = FMath::Clamp(Input, -1.f, 1.f);
    return FMath::Sign(Input) * (FMath::Abs(Input) - DeadZone) / (1.f - DeadZone);
}
} // namespace

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSeatAxisInputTest, "HUTB.Pimax.SeatInput.DeadZone",
                                EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSeatAxisInputTest::RunTest(const FString &Parameters)
{
    TestEqual(TEXT("Released input stops movement"), SeatAxisValue(0.f), 0.f);
    TestEqual(TEXT("Small drift is ignored"), SeatAxisValue(0.1f), 0.f);
    TestEqual(TEXT("Negative drift is ignored"), SeatAxisValue(-0.15f), 0.f);
    TestEqual(TEXT("Full input gives full speed"), SeatAxisValue(1.f), 1.f);
    TestEqual(TEXT("Reverse input gives reverse speed"), SeatAxisValue(-1.f), -1.f);
    TestTrue(TEXT("Partial grip gives proportional speed"),
             FMath::IsNearlyEqual(SeatAxisValue(0.575f), 0.5f, KINDA_SMALL_NUMBER));
    TestEqual(TEXT("Out of range input is bounded"), SeatAxisValue(2.f), 1.f);
    TestEqual(TEXT("Negative out of range input is bounded"), SeatAxisValue(-2.f), -1.f);
    return true;
}
#endif

////////////////:INPUTS:////////////////
/// NOTE: Here we define all the Input functions for the EgoVehicle just to keep them
// from cluttering the EgoVehcile.cpp file

const DReyeVR::UserInputs &AEgoVehicle::GetVehicleInputs() const
{
    return VehicleInputs;
}

const FTransform &AEgoVehicle::GetCameraRootPose() const
{
    return CameraPoseOffset;
}

void AEgoVehicle::CameraFwd()
{
    // move by (1, 0, 0)
    CameraPositionAdjust(FVector::ForwardVector);
}

void AEgoVehicle::CameraBack()
{
    // move by (-1, 0, 0)
    CameraPositionAdjust(FVector::BackwardVector);
}

void AEgoVehicle::CameraLeft()
{
    // move by (0, -1, 0)
    CameraPositionAdjust(FVector::LeftVector);
}

void AEgoVehicle::CameraRight()
{
    // move by (0, 1, 0)
    CameraPositionAdjust(FVector::RightVector);
}

void AEgoVehicle::CameraUp()
{
    // move by (0, 0, 1)
    CameraPositionAdjust(FVector::UpVector);
}

void AEgoVehicle::CameraDown()
{
    // move by (0, 0, -1)
    CameraPositionAdjust(FVector::DownVector);
}

void AEgoVehicle::CameraMoveDepth(const float Input)
{
    CameraMoveAxis(Input, FVector::ForwardVector, 1, TEXT("depth"));
}

void AEgoVehicle::CameraMoveHorizontal(const float Input)
{
    CameraMoveAxis(Input, FVector::RightVector, 2, TEXT("horizontal"));
}

void AEgoVehicle::CameraMoveUp(const float Input)
{
    CameraMoveAxis(Input, FVector::UpVector, 4, TEXT("up"));
}

void AEgoVehicle::CameraMoveDown(const float Input)
{
    CameraMoveAxis(Input, FVector::DownVector, 8, TEXT("down"));
}

void AEgoVehicle::CameraMoveAxis(const float Input, const FVector &Direction, const uint8 DiagnosticBit,
                               const TCHAR *AxisName)
{
    const float Value = SeatAxisValue(Input);
    const bool bActive = Value != 0.f;
    if (bActive != ((ActiveSeatAxes & DiagnosticBit) != 0))
    {
        LOG("Seat input: %s %s (raw=%.3f, offset=%s)", AxisName, bActive ? TEXT("start") : TEXT("stop"),
            Input, *CameraPoseOffset.GetLocation().ToString());
        if (bActive)
            ActiveSeatAxes |= DiagnosticBit;
        else
            ActiveSeatAxes &= ~DiagnosticBit;
    }
    if (bActive && GetWorld() != nullptr && VRCameraRoot != nullptr)
        CameraPositionAdjust(Direction * Value * SeatMoveSpeedCmPerSecond * GetWorld()->GetDeltaSeconds());
}

void AEgoVehicle::CameraPositionAdjust(const FVector &Disp)
{
    if (Disp.Equals(FVector::ZeroVector, 0.0001f))
        return;
    // preserves adjustment even after changing view
    CameraPoseOffset.SetLocation(CameraPoseOffset.GetLocation() + Disp);
    VRCameraRoot->SetRelativeLocation(CameraPose.GetLocation() + CameraPoseOffset.GetLocation());
    /// TODO: account for rotation? scale?
}

void AEgoVehicle::CameraPositionAdjust(bool bForward, bool bRight, bool bBackwards, bool bLeft, bool bUp, bool bDown)
{
    // add the corresponding directions according to the adjustment booleans
    const FVector Disp = FVector::ForwardVector * bForward + FVector::RightVector * bRight +
                         FVector::BackwardVector * bBackwards + FVector::LeftVector * bLeft + FVector::UpVector * bUp +
                         FVector::DownVector * bDown;
    CameraPositionAdjust(Disp);
}

void AEgoVehicle::PressNextCameraView()
{
    LOG("Seat input: next-view press (ready=%d, poses=%d)", bCanPressNextCameraView, CameraPoseKeys.Num());
    if (!bCanPressNextCameraView)
        return;
    bCanPressNextCameraView = false;
    NextCameraView();
};
void AEgoVehicle::ReleaseNextCameraView()
{
    LOG("Seat input: next-view release");
    bCanPressNextCameraView = true;
};

void AEgoVehicle::PressPrevCameraView()
{
    LOG("Seat input: prev-view press (ready=%d, poses=%d)", bCanPressPrevCameraView, CameraPoseKeys.Num());
    if (!bCanPressPrevCameraView)
        return;
    bCanPressPrevCameraView = false;
    PrevCameraView();
};
void AEgoVehicle::ReleasePrevCameraView()
{
    LOG("Seat input: prev-view release");
    bCanPressPrevCameraView = true;
};

void AEgoVehicle::NextCameraView()
{
    if (CameraPoseKeys.Num() == 0)
        return;
    CurrentCameraTransformIdx = (CurrentCameraTransformIdx + 1) % (CameraPoseKeys.Num());
    const FString &Key = CameraPoseKeys[CurrentCameraTransformIdx];
    LOG("Switching manually to next camera view: \"%s\"", *Key);
    SetCameraRootPose(CurrentCameraTransformIdx);
}

void AEgoVehicle::PrevCameraView()
{
    if (CameraPoseKeys.Num() == 0)
        return;
    if (CurrentCameraTransformIdx == 0)
        CurrentCameraTransformIdx = CameraPoseKeys.Num();
    CurrentCameraTransformIdx--;
    const FString &Key = CameraPoseKeys[CurrentCameraTransformIdx];
    LOG("Switching manually to prev camera view: \"%s\"", *Key);
    SetCameraRootPose(CurrentCameraTransformIdx);
}

void AEgoVehicle::AddSteering(const float SteeringInput)
{
    float ScaledSteeringInput = this->ScaleSteeringInput * SteeringInput;
    // assign to input struct
    VehicleInputs.Steering += ScaledSteeringInput;
}

void AEgoVehicle::AddThrottle(const float ThrottleInput)
{
    float ScaledThrottleInput = this->ScaleThrottleInput * ThrottleInput;

    // apply new light state
    FVehicleLightState Lights = GetVehicleLightState();
    Lights.Reverse = false;
    Lights.Brake = false;
    SetVehicleLightState(Lights);

    // assign to input struct
    VehicleInputs.Throttle += ScaledThrottleInput;
}

void AEgoVehicle::AddBrake(const float BrakeInput)
{
    float ScaledBrakeInput = this->ScaleBrakeInput * BrakeInput;

    // apply new light state
    FVehicleLightState Lights = GetVehicleLightState();
    Lights.Reverse = false;
    Lights.Brake = true;
    SetVehicleLightState(Lights);

    // assign to input struct
    VehicleInputs.Brake += ScaledBrakeInput;
}

void AEgoVehicle::PressReverse()
{
    if (!bCanPressReverse)
        return;
    bCanPressReverse = false; // don't press again until release
    bReverse = !bReverse;

    // negate to toggle bw + (forwards) and - (backwards)
    // const int CurrentGear = this->GetVehicleMovementComponent()->GetTargetGear();
    // int NewGear = -1; // for when parked
    // if (CurrentGear != 0)
    // {
    //     NewGear = bReverse ? -1 * std::abs(CurrentGear) : std::abs(CurrentGear); // negative => backwards
    // }
    // this->GetVehicleMovementComponent()->SetTargetGear(NewGear, true); // UE4 control

    // apply new light state
    FVehicleLightState Lights = this->GetVehicleLightState();
    Lights.Reverse = this->bReverse;
    this->SetVehicleLightState(Lights);

    // LOG("Toggle Reverse");
    // assign to input struct
    VehicleInputs.ToggledReverse = true;

    PlayGearShiftSound();

    // call to parent
    SetReverse(bReverse);
}

void AEgoVehicle::ReleaseReverse()
{
    VehicleInputs.ToggledReverse = false;
    bCanPressReverse = true;
}

void AEgoVehicle::PressTurnSignalR()
{
    if (!bCanPressTurnSignalR)
        return;
    bCanPressTurnSignalR = false; // don't press again until release
    // store in local input container
    VehicleInputs.TurnSignalRight = true;

    if (bEnableTurnSignalAction)
    {
        // apply new light state
        FVehicleLightState Lights = this->GetVehicleLightState();
        Lights.RightBlinker = true;
        Lights.LeftBlinker = false;
        this->SetVehicleLightState(Lights);
        // play sound
        this->PlayTurnSignalSound();
    }
    RightSignalTimeToDie = TNumericLimits<float>::Max(); // wait until button released (+inf until then)
    LeftSignalTimeToDie = 0.f;                           // immediately stop left signal
}

void AEgoVehicle::ReleaseTurnSignalR()
{
    if (bCanPressTurnSignalR)
        return;
    VehicleInputs.TurnSignalRight = false;
    RightSignalTimeToDie = GetWorld()->GetTimeSeconds() + this->TurnSignalDuration; // reset counter
    bCanPressTurnSignalR = true;
}

void AEgoVehicle::PressTurnSignalL()
{
    if (!bCanPressTurnSignalL)
        return;
    bCanPressTurnSignalL = false; // don't press again until release
    // store in local input container
    VehicleInputs.TurnSignalLeft = true;

    if (bEnableTurnSignalAction)
    {
        // apply new light state
        FVehicleLightState Lights = this->GetVehicleLightState();
        Lights.RightBlinker = false;
        Lights.LeftBlinker = true;
        this->SetVehicleLightState(Lights);
        // play sound
        this->PlayTurnSignalSound();
    }
    RightSignalTimeToDie = 0.f;                         // immediately stop right signal
    LeftSignalTimeToDie = TNumericLimits<float>::Max(); // wait until button released (+inf until then)
}

void AEgoVehicle::ReleaseTurnSignalL()
{
    if (bCanPressTurnSignalL)
        return;
    VehicleInputs.TurnSignalLeft = false;
    LeftSignalTimeToDie = GetWorld()->GetTimeSeconds() + this->TurnSignalDuration; // reset counter
    bCanPressTurnSignalL = true;
}

void AEgoVehicle::TickVehicleInputs()
{
    FVehicleControl LastAppliedControl = GetVehicleControl();

    int bIncludeLast = static_cast<int>(GetAutopilotStatus());
    FVehicleControl ManualInputs;
    // only include LastAppliedControl when autopilot is running (bc it would have flushed earlier this tick)
    ManualInputs.Steer = VehicleInputs.Steering + bIncludeLast * LastAppliedControl.Steer;
    ManualInputs.Brake = VehicleInputs.Brake + bIncludeLast * LastAppliedControl.Brake;
    ManualInputs.Throttle = VehicleInputs.Throttle + bIncludeLast * LastAppliedControl.Throttle;
    ManualInputs.bReverse = bReverse;

    // apply inputs to this vehicle only when either one of the parameter is non-zero or autopilot is on
    if ((!FMath::IsNearlyEqual(ManualInputs.Steer, 0.f, 0.02f) ||
        !FMath::IsNearlyEqual(ManualInputs.Brake, 0.f, 0.02f) ||
        !FMath::IsNearlyEqual(ManualInputs.Throttle, 0.f, 0.02f)) ||
        GetAutopilotStatus())
    {
        this->ApplyVehicleControl(ManualInputs, EVehicleInputPriority::User);
        // send these inputs to the Carla (parent) vehicle
        FlushVehicleControl();
    }

    VehicleInputs = DReyeVR::UserInputs(); // clear inputs for this frame
}
