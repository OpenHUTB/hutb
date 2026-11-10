#include "HandPose.h"

#if PLATFORM_WINDOWS
#include "openvr.h"
#endif

namespace
{
/// Non-finite values would poison the whole column, so they become empty instead.
FString HandNumber6(float Value) { return FMath::IsFinite(Value) ? FString::Printf(TEXT("%.6f"), Value) : FString(); }
FString HandNumber3(float Value) { return FMath::IsFinite(Value) ? FString::Printf(TEXT("%.3f"), Value) : FString(); }

/// Nine fields, always: valid,x,y,z,pitch,yaw,roll,trigger,grip.
void AppendHand(TArray<FString> &Row, const FDReyeVRHandPose &Hand)
{
    Row.Add(Hand.bValid ? TEXT("1") : TEXT("0"));
    if (Hand.bValid)
    {
        Row.Add(HandNumber6(Hand.Location.X));
        Row.Add(HandNumber6(Hand.Location.Y));
        Row.Add(HandNumber6(Hand.Location.Z));
        Row.Add(HandNumber6(Hand.Rotation.Pitch));
        Row.Add(HandNumber6(Hand.Rotation.Yaw));
        Row.Add(HandNumber6(Hand.Rotation.Roll));
        // -1 means "the runtime did not report this axis"; values below -0.5 are never real.
        Row.Add(Hand.Trigger >= -0.5f ? HandNumber3(Hand.Trigger) : FString());
        Row.Add(Hand.Grip >= -0.5f ? HandNumber3(Hand.Grip) : FString());
    }
    else
    {
        for (int32 Field = 0; Field < 8; ++Field) Row.Add(TEXT(""));
    }
}
} // namespace

FRotator OpenVRMatrixToRotator(const float M[3][4])
{
    // OpenVR HmdMatrix34_t is right-handed (X right, Y up, Z back / -Z forward) with
    // column-vector basis: R = Ry(yaw) * Rx(pitch) * Rz(roll). Putting yaw around +Y
    // first avoids gimbal lock across arbitrary room facing directions.
    const float SinPitch = FMath::Clamp(-M[1][2], -1.f, 1.f);
    const float PitchRad = FMath::Asin(SinPitch);
    const float CosPitch = FMath::Cos(PitchRad);
    float YawRad = 0.f;
    float RollRad = 0.f;
    if (CosPitch > 1e-6f)
    {
        YawRad = FMath::Atan2(M[0][2], M[2][2]);
        RollRad = FMath::Atan2(M[1][0], M[1][1]);
    }
    else
    {
        YawRad = FMath::Atan2(-M[2][0], M[0][0]);
    }
    return FRotator(FMath::RadiansToDegrees(PitchRad),
                    FMath::RadiansToDegrees(YawRad),
                    FMath::RadiansToDegrees(RollRad));
}

void AppendHandPoseColumns(TArray<FString> &Row, const FDReyeVRHandPosePair &Poses)
{
    AppendHand(Row, Poses.Left);
    AppendHand(Row, Poses.Right);
}

bool FDReyeVRAutoCalibrator::Tick(double DeltaSeconds, const FVector &HeadPosCm, const FRotator &HeadRot,
                                  float NormalizedSteer)
{
    if (bCalibrated || DeltaSeconds <= 0.0 || !FMath::IsFinite(NormalizedSteer)) return false;
    // Cap per-tick accumulation so a single map-load stall frame cannot masquerade as 5 s of stillness.
    const double StepDt = FMath::Min(DeltaSeconds, 0.1);
    const FQuat CurQuat = HeadRot.Quaternion();

    if (!bHasAnchor)
    {
        AnchorPosCm = HeadPosCm;
        AnchorQuat = CurQuat;
        SteerRest = PrevSteer = NormalizedSteer;
        StillSeconds = 0.0;
        bHasAnchor = true;
        return false;
    }

    const float PosDriftCm = FVector::Dist(HeadPosCm, AnchorPosCm);
    const float AngleDriftDeg = FMath::RadiansToDegrees(AnchorQuat.AngularDistance(CurQuat));
    if (PosDriftCm > Config.MaxHeadPosDriftCm || AngleDriftDeg > Config.MaxHeadAngleDriftDeg)
    {
        AnchorPosCm = HeadPosCm;
        AnchorQuat = CurQuat;
        SteerRest = PrevSteer = NormalizedSteer;
        StillSeconds = 0.0;
        return false;
    }

    const double SteerRate = FMath::Abs(NormalizedSteer - PrevSteer) / StepDt;
    PrevSteer = NormalizedSteer;
    StillSeconds += StepDt;

    if (StillSeconds < Config.RequiredStillSeconds)
    {
        if (SteerRate < Config.QuietSteerRatePerSec)
        {
            SteerRest = NormalizedSteer;
        }
        return false;
    }

    if (FMath::Abs(NormalizedSteer - SteerRest) >= Config.SteerStartDelta)
    {
        bCalibrated = true;
        return true;
    }
    return false;
}

bool SampleHandPoses(FDReyeVRHandPosePair &Out)
{
    Out = FDReyeVRHandPosePair();
#if PLATFORM_WINDOWS
    vr::IVRSystem *Sys = vr::VRSystem();
    if (!Sys) return false;

    const vr::TrackedDeviceIndex_t Indices[2] = {
        Sys->GetTrackedDeviceIndexForControllerRole(vr::TrackedControllerRole_LeftHand),
        Sys->GetTrackedDeviceIndexForControllerRole(vr::TrackedControllerRole_RightHand)};
    FDReyeVRHandPose *Hands[2] = {&Out.Left, &Out.Right};

    for (int32 Side = 0; Side < 2; ++Side)
    {
        const vr::TrackedDeviceIndex_t Index = Indices[Side];
        if (Index == vr::k_unTrackedDeviceIndexInvalid) continue;

        vr::VRControllerState_t State;
        FMemory::Memzero(&State, sizeof(State));
        vr::TrackedDevicePose_t Pose;
        FMemory::Memzero(&Pose, sizeof(Pose));
        if (!Sys->GetControllerStateWithPose(vr::TrackingUniverseStanding, Index, &State,
                                            static_cast<uint32>(sizeof(State)), &Pose))
            continue;

        FDReyeVRHandPose &Hand = *Hands[Side];
        Hand.bValid = Pose.bDeviceIsConnected && Pose.bPoseIsValid;
        if (!Hand.bValid) continue;

        const auto &M = Pose.mDeviceToAbsoluteTracking;
        Hand.Location = FVector(static_cast<float>(M.m[0][3]), static_cast<float>(M.m[1][3]),
                                static_cast<float>(M.m[2][3]));
        // OpenVR is y-up and right-handed; keep the tracker's own frame on purpose.
        // Do NOT convert to UE's left-handed z-up FMatrix::Rotator() here: decompose
        // OpenVR's column-basis rotation directly as intrinsic Y(yaw)-X(pitch)-Z(roll).
        Hand.Rotation = OpenVRMatrixToRotator(M.m);
        Hand.Trigger = State.rAxis[0].x;
        Hand.Grip = State.rAxis[1].x;
    }
    return true;
#else
    return false;
#endif
}

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHandPoseColumnsTest, "HUTB.DReyeVR.HandPose.Columns",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHandPoseColumnsTest::RunTest(const FString &Parameters)
{
    // 1. No controllers: the row keeps its width, only the two valid flags are filled.
    FDReyeVRHandPosePair Invalid;
    TArray<FString> InvalidRow;
    AppendHandPoseColumns(InvalidRow, Invalid);
    TestEqual(TEXT("Invalid pair appends 18 fields"), InvalidRow.Num(), 18);
    TestEqual(TEXT("Left hand is marked invalid"), InvalidRow[0], FString(TEXT("0")));
    TestEqual(TEXT("Right hand is marked invalid"), InvalidRow[9], FString(TEXT("0")));
    bool bAllEmpty = true;
    for (int32 Field = 0; Field < InvalidRow.Num(); ++Field)
        if (Field != 0 && Field != 9) bAllEmpty = bAllEmpty && InvalidRow[Field].IsEmpty();
    TestTrue(TEXT("Every other invalid field is empty"), bAllEmpty);

    // 2. Both controllers valid: 9 per hand, left first, right second.
    FDReyeVRHandPosePair Valid;
    Valid.Left.bValid = Valid.Right.bValid = true;
    Valid.Left.Location = Valid.Right.Location = FVector(0.1f, 0.2f, 0.3f);
    Valid.Left.Rotation = Valid.Right.Rotation = FRotator(10.f, 20.f, 30.f);
    Valid.Left.Trigger = Valid.Right.Trigger = 0.5f;
    Valid.Left.Grip = Valid.Right.Grip = 0.25f;
    TArray<FString> Row;
    AppendHandPoseColumns(Row, Valid);
    TestEqual(TEXT("Valid pair appends 18 fields"), Row.Num(), 18);
    TestEqual(TEXT("Left valid"), Row[0], FString(TEXT("1")));
    TestEqual(TEXT("Left x in metres"), Row[1], FString(TEXT("0.100000")));
    TestEqual(TEXT("Left pitch in degrees"), Row[4], FString(TEXT("10.000000")));
    TestEqual(TEXT("Left trigger"), Row[7], FString(TEXT("0.500")));
    TestEqual(TEXT("Left grip"), Row[8], FString(TEXT("0.250")));
    TestEqual(TEXT("Right group starts after the left one"), Row[9 + 1], FString(TEXT("0.100000")));
    TestEqual(TEXT("Right yaw"), Row[9 + 5], FString(TEXT("20.000000")));

    // 3. OpenVR right-handed column-basis Y-X-Z Euler decomposition.
    const float Identity[3][4] = {{1.f, 0.f, 0.f, 0.f}, {0.f, 1.f, 0.f, 0.f}, {0.f, 0.f, 1.f, 0.f}};
    const FRotator IdRot = OpenVRMatrixToRotator(Identity);
    TestTrue(TEXT("Identity OpenVR matrix -> zero rotator"),
             FMath::IsNearlyZero(IdRot.Pitch, 1e-3f) && FMath::IsNearlyZero(IdRot.Yaw, 1e-3f) &&
                 FMath::IsNearlyZero(IdRot.Roll, 1e-3f));

    // +90 deg yaw around +Y (controller turns left: local +X -> -Z, local +Z -> +X)
    const float Yaw90[3][4] = {{0.f, 0.f, 1.f, 0.f}, {0.f, 1.f, 0.f, 0.f}, {-1.f, 0.f, 0.f, 0.f}};
    const FRotator YawRot = OpenVRMatrixToRotator(Yaw90);
    TestTrue(TEXT("+90 deg OpenVR yaw decomposed without transpose"),
             FMath::IsNearlyEqual(YawRot.Yaw, 90.f, 1e-3f) && FMath::IsNearlyZero(YawRot.Pitch, 1e-3f) &&
                 FMath::IsNearlyZero(YawRot.Roll, 1e-3f));

    // +30 deg pitch around +X (controller nose tilts up: local -Z gains +Y, so M[1][2] = -sin(30) = -0.5)
    const float C30 = 0.8660254f, S30 = 0.5f;
    const float Pitch30[3][4] = {{1.f, 0.f, 0.f, 0.f}, {0.f, C30, -S30, 0.f}, {0.f, S30, C30, 0.f}};
    const FRotator PitchRot = OpenVRMatrixToRotator(Pitch30);
    TestTrue(TEXT("+30 deg OpenVR pitch decomposed cleanly"),
             FMath::IsNearlyEqual(PitchRot.Pitch, 30.f, 1e-2f) && FMath::IsNearlyZero(PitchRot.Yaw, 1e-3f) &&
                 FMath::IsNearlyZero(PitchRot.Roll, 1e-3f));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDReyeVRAutoCalibratorTest, "HUTB.DReyeVR.HandPose.AutoCalibrator",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDReyeVRAutoCalibratorTest::RunTest(const FString &Parameters)
{
    FDReyeVRAutoCalibrator Calib;
    const FVector SeatHead(10.f, -20.f, 120.f);
    const FRotator LookFwd(2.f, -15.f, 0.5f);

    // 1. A single huge hitch frame (e.g. map load) must NOT satisfy 5 s stillness.
    TestFalse(TEXT("Anchor frame never triggers"), Calib.Tick(0.02, SeatHead, LookFwd, -0.013f));
    TestFalse(TEXT("6 s frame hitch is clamped and cannot arm"), Calib.Tick(6.0, SeatHead, LookFwd, 0.05f));

    // 2. Head motion > 6 deg resets accumulated stillness.
    for (int32 i = 0; i < 45; ++i)
        TestFalse(TEXT("Accumulating 4.5 s stillness"), Calib.Tick(0.1, SeatHead, LookFwd, -0.013f));
    TestFalse(TEXT("Head turn resets timer"), Calib.Tick(0.1, SeatHead, FRotator(2.f, 5.f, 0.5f), -0.013f));
    TestTrue(TEXT("Stillness reset to zero on head turn"), Calib.GetStillSeconds() < 1e-6);

    // 3. Five continuous seconds of slight natural micro-sway (< 5 cm, < 6 deg) with steady wheel arms without firing.
    for (int32 i = 0; i < 55; ++i)
    {
        const FVector Sway = SeatHead + FVector(1.2f, -0.8f, 0.5f);
        TestFalse(TEXT("Steady head + steady wheel waits for wheel turn"),
                  Calib.Tick(0.1, Sway, FRotator(2.5f, 6.5f, 0.5f), -0.013f));
    }
    TestTrue(TEXT("Armed after >= 5 s stillness"), Calib.GetStillSeconds() >= 5.0);
    TestFalse(TEXT("Not yet calibrated before wheel rotates"), Calib.HasCalibrated());

    // 4. Starting to rotate the steering wheel triggers calibration ONCE and never again.
    TestTrue(TEXT("Wheel rotation after 5 s stillness triggers calibration"),
             Calib.Tick(0.05, SeatHead, FRotator(2.5f, 6.5f, 0.5f), 0.008f));
    TestTrue(TEXT("Calibrator latches HasCalibrated"), Calib.HasCalibrated());
    for (int32 i = 0; i < 100; ++i)
        TestFalse(TEXT("One-shot: never fires a second time"),
                  Calib.Tick(0.1, SeatHead, FRotator(2.5f, 6.5f, 0.5f), (i % 2) ? 0.25f : -0.25f));
    return true;
}
#endif
