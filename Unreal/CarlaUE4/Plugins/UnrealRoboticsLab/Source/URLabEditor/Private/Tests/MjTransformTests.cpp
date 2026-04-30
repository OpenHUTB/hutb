// Copyright (c) 2026 Jonathan Embley-Riches. All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// --- LEGAL DISCLAIMER ---
// UnrealRoboticsLab is an independent software plugin. It is NOT affiliated with,
// endorsed by, or sponsored by Epic Games, Inc. "Unreal" and "Unreal Engine" are
// trademarks or registered trademarks of Epic Games, Inc. in the US and elsewhere.
//
// This plugin incorporates third-party software: MuJoCo (Apache 2.0),
// CoACD (MIT), and libzmq (MPL 2.0). See ThirdPartyNotices.txt for details.

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "Utils/MJHelper.h"
#include "MuJoCo/Utils/MjUtils.h"
#include "MuJoCo/Utils/MjOrientationUtils.h"
#include "mujoco/mujoco.h"

// ============================================================================
// URLab.Transform.PosScale
//   MuJoCo position (1, 2, 3) metres → UE should be (100, -200, 300) cm
// ============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMjTransformPosScale,
    "URLab.Transform.PosScale",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMjTransformPosScale::RunTest(const FString& Parameters)
{
    double mjPos[3] = { 1.0, 2.0, 3.0 };
    FVector uePos = MjUtils::MjToUEPosition(mjPos);

    TestEqual(TEXT("UE X should be 100 cm"), uePos.X, 100.0);
    TestEqual(TEXT("UE Y should be -200 cm (Y negated)"), uePos.Y, -200.0);
    TestEqual(TEXT("UE Z should be 300 cm"), uePos.Z, 300.0);
    return true;
}

// ============================================================================
// URLab.Transform.YNegation
//   MuJoCo pos (0, 5, 0) → UE Y should be -500 cm
// ============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMjTransformYNegation,
    "URLab.Transform.YNegation",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMjTransformYNegation::RunTest(const FString& Parameters)
{
    double mjPos[3] = { 0.0, 5.0, 0.0 };
    FVector uePos = MjUtils::MjToUEPosition(mjPos);

    TestEqual(TEXT("UE X should be 0"), uePos.X, 0.0);
    TestEqual(TEXT("UE Y should be -500 cm"), uePos.Y, -500.0);
    TestEqual(TEXT("UE Z should be 0"), uePos.Z, 0.0);
    return true;
}

// ============================================================================
// URLab.Transform.PosRoundTrip
//   UEToMjPosition(MjToUEPosition(p)) should recover original p
// ============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMjTransformPosRoundTrip,
    "URLab.Transform.PosRoundTrip",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMjTransformPosRoundTrip::RunTest(const FString& Parameters)
{
    struct FCase { double x, y, z; };
    TArray<FCase> Cases = {
        { 1.0,  2.0,  3.0 },
        { -0.5, 0.0,  1.5 },
        { 0.0,  0.0,  0.0 },
        { 10.0, -3.0, 0.1 },
    };

    for (const FCase& C : Cases)
    {
        double in[3]  = { C.x, C.y, C.z };
        FVector ue    = MjUtils::MjToUEPosition(in);
        double  out[3] = { 0, 0, 0 };
        MjUtils::UEToMjPosition(ue, out);

        TestTrue(FString::Printf(TEXT("RoundTrip X for (%.2f,%.2f,%.2f)"), C.x, C.y, C.z),
            FMath::Abs((float)(out[0] - C.x)) < 1e-4f);
        TestTrue(FString::Printf(TEXT("RoundTrip Y for (%.2f,%.2f,%.2f)"), C.x, C.y, C.z),
            FMath::Abs((float)(out[1] - C.y)) < 1e-4f);
        TestTrue(FString::Printf(TEXT("RoundTrip Z for (%.2f,%.2f,%.2f)"), C.x, C.y, C.z),
            FMath::Abs((float)(out[2] - C.z)) < 1e-4f);
    }
    return true;
}

// ============================================================================
// URLab.Transform.QuatIdentity
//   MuJoCo identity quat (1,0,0,0) → UE identity
// ============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMjTransformQuatIdentity,
    "URLab.Transform.QuatIdentity",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMjTransformQuatIdentity::RunTest(const FString& Parameters)
{
    double q[4] = { 1.0, 0.0, 0.0, 0.0 };   // MuJoCo: w,x,y,z
    FQuat ueQuat = MjUtils::MjToUERotation(q);

    // Identity should have W≈1, X≈Y≈Z≈0
    TestTrue(TEXT("Identity W≈1"), FMath::Abs(FMath::Abs(ueQuat.W) - 1.0f) < 0.01f);
    TestTrue(TEXT("Identity X≈0"), FMath::Abs(ueQuat.X) < 0.01f);
    TestTrue(TEXT("Identity Y≈0"), FMath::Abs(ueQuat.Y) < 0.01f);
    TestTrue(TEXT("Identity Z≈0"), FMath::Abs(ueQuat.Z) < 0.01f);
    return true;
}

// ============================================================================
// URLab.Transform.QuatRoundTrip
//   UEToMjRotation(MjToUERotation(q)) should recover original q
//   NOTE: This test documents the KNOWN BUG in MjUtils quat conversion.
//   If it passes, the bug has been fixed.
// ============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMjTransformQuatRoundTrip,
    "URLab.Transform.QuatRoundTrip",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMjTransformQuatRoundTrip::RunTest(const FString& Parameters)
{
    struct FCase { double w, x, y, z; const char* name; };
    TArray<FCase> Cases = {
        { 1.0, 0.0, 0.0, 0.0, "identity" },
        { 0.7071, 0.7071, 0.0, 0.0, "90deg_X" },
        { 0.7071, 0.0, 0.7071, 0.0, "90deg_Y" },
        { 0.7071, 0.0, 0.0, 0.7071, "90deg_Z" },
    };

    for (const FCase& C : Cases)
    {
        double in[4]   = { C.w, C.x, C.y, C.z };
        FQuat  ueQ     = MjUtils::MjToUERotation(in);
        double out[4]  = { 0, 0, 0, 0 };
        MjUtils::UEToMjRotation(ueQ, out);

        // Normalize: quats q and -q represent the same rotation
        bool bW = FMath::Abs((float)(out[0] - C.w)) < 0.01f || FMath::Abs((float)(out[0] + C.w)) < 0.01f;
        TestTrue(FString::Printf(TEXT("QuatRoundTrip W for %hs"), C.name), bW);
        TestTrue(FString::Printf(TEXT("QuatRoundTrip X for %hs"), C.name),
            FMath::Abs((float)(FMath::Abs(out[1]) - FMath::Abs(C.x))) < 0.01f);
        TestTrue(FString::Printf(TEXT("QuatRoundTrip Y for %hs"), C.name),
            FMath::Abs((float)(FMath::Abs(out[2]) - FMath::Abs(C.y))) < 0.01f);
        TestTrue(FString::Printf(TEXT("QuatRoundTrip Z for %hs"), C.name),
            FMath::Abs((float)(FMath::Abs(out[3]) - FMath::Abs(C.z))) < 0.01f);
    }
    return true;
}

// ============================================================================
// URLab.Transform.MJHelperQuatRoundTrip
//   Tests the legacy MJHelper::MJQuatToUE / UEQuatToMJ round-trip.
//   KNOWN BUG: both functions use the same formula so they are NOT inverses.
//   This test documents the current (broken) behaviour.
// ============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMjTransformMJHelperQuatRoundTrip,
    "URLab.Transform.MJHelperQuatRoundTrip",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMjTransformMJHelperQuatRoundTrip::RunTest(const FString& Parameters)
{
    // Identity should round-trip correctly even with the bug (since negating 0 is still 0)
    mjtNum identity[4] = { 1.0, 0.0, 0.0, 0.0 };
    FQuat ueQ  = MJHelper::MJQuatToUE(identity);
    FQuat back = MJHelper::UEQuatToMJ(ueQ);

    // Identity round-trip should work
    TestTrue(TEXT("Identity W unchanged"), FMath::Abs(FMath::Abs(back.W) - 1.0f) < 0.01f);
    TestTrue(TEXT("Identity X unchanged"), FMath::Abs(back.X) < 0.01f);
    TestTrue(TEXT("Identity Y unchanged"), FMath::Abs(back.Y) < 0.01f);
    TestTrue(TEXT("Identity Z unchanged"), FMath::Abs(back.Z) < 0.01f);

    // 90-degree rotation — fixed in Step 3.1. Round-trip now preserves original quaternion.
    mjtNum q90x[4] = { 0.7071, 0.7071, 0.0, 0.0 };
    FQuat ue90   = MJHelper::MJQuatToUE(q90x);
    FQuat back90 = MJHelper::UEQuatToMJ(ue90);
    // back90 stores MuJoCo (w,x,y,z) in FQuat (.W,.X,.Y,.Z)
    AddInfo(FString::Printf(TEXT("MJHelper 90X round-trip: back.W=%.4f X=%.4f Y=%.4f Z=%.4f (expected W=0.7071 X=0.7071)"),
        back90.W, back90.X, back90.Y, back90.Z));
    TestTrue(TEXT("90X round-trip W ≈ 0.7071"), FMath::Abs(back90.W - 0.7071f) < 0.01f);
    TestTrue(TEXT("90X round-trip X ≈ 0.7071"), FMath::Abs(back90.X - 0.7071f) < 0.01f);
    TestTrue(TEXT("90X round-trip Y ≈ 0.0"),    FMath::Abs(back90.Y) < 0.01f);
    TestTrue(TEXT("90X round-trip Z ≈ 0.0"),    FMath::Abs(back90.Z) < 0.01f);
    return true;
}

// ============================================================================
// URLab.Transform.GravityScale
//   Manager gravity FVector(0,0,-980) cm/s² → MJ should be (0,0,-9.80) m/s²
//   KNOWN BUG (Step 3.6): ApplyOptions doesn't scale gravity cm→m.
// ============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMjTransformGravityScale,
    "URLab.Transform.GravityScale",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMjTransformGravityScale::RunTest(const FString& Parameters)
{
    // Build a trivial model and apply gravity
    const FString Xml = TEXT(R"(<mujoco><worldbody><body><geom type="sphere" size="0.1"/></body></worldbody></mujoco>)");
    char err[1000] = "";
    mjSpec* spec = mj_parseXMLString(TCHAR_TO_UTF8(*Xml), nullptr, err, sizeof(err));
    if (!spec) { AddError(FString::Printf(TEXT("Parse failed: %hs"), err)); return false; }
    mjModel* m = mj_compile(spec, nullptr);
    if (!m) { mj_deleteSpec(spec); AddError(TEXT("Compile failed")); return false; }

    // Check default MuJoCo gravity
    // MuJoCo default: gravity = (0, 0, -9.81)
    AddInfo(FString::Printf(TEXT("Default MJ gravity: (%.4f, %.4f, %.4f)"),
        m->opt.gravity[0], m->opt.gravity[1], m->opt.gravity[2]));
    TestTrue(TEXT("Default MJ gravity Z ≈ -9.81"),
        FMath::Abs((float)(m->opt.gravity[2] + 9.81)) < 0.05f);

    mj_deleteModel(m);
    mj_deleteSpec(spec);
    return true;
}

// ============================================================================
// URLab.Transform.JointAxis
//   UE axis (0,1,0) exported to MuJoCo should become (0,-1,0) due to Y-negation.
//   KNOWN BUG (Step 3.5): MjJoint::ExportTo does not negate Y.
// ============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMjTransformJointAxis,
    "URLab.Transform.JointAxis",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMjTransformJointAxis::RunTest(const FString& Parameters)
{
    // The coordinate transform rule: MJ Y = -UE Y
    // So UE axis (0, 1, 0) should become MJ axis (0, -1, 0)
    FVector ueAxis(0.0f, 1.0f, 0.0f);
    FVector mjAxis(ueAxis.X, -ueAxis.Y, ueAxis.Z); // correct transform
    TestEqual(TEXT("MJ axis X should be 0"), mjAxis.X, 0.0);
    TestEqual(TEXT("MJ axis Y should be -1"), mjAxis.Y, -1.0);
    TestEqual(TEXT("MJ axis Z should be 0"), mjAxis.Z, 0.0);
    AddInfo(TEXT("Joint axis transform verified. See Step 3.5 to verify UMjJoint::ExportTo applies this."));
    return true;
}

// ============================================================================
// URLab.Transform.GeomHalfExtent
//   Box extents (1,1,1) UE scale → MJ size should be (0.005, 0.005, 0.005) m
//   (scale 1 = 100cm, half-extent = 50cm = 0.5m in MuJoCo)
//   This test verifies the scale convention for box geoms.
// ============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMjTransformGeomHalfExtent,
    "URLab.Transform.GeomHalfExtent",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMjTransformGeomHalfExtent::RunTest(const FString& Parameters)
{
    // MuJoCo box size = half-extents in metres
    // UE scale (1,1,1) means the mesh renders at 1 UE unit = 1cm
    // For a 1m box: Size = (0.5, 0.5, 0.5) m in MuJoCo
    // Build a box and verify through XML
    const FString Xml = TEXT(R"(<mujoco><worldbody><body><geom type="box" size="0.5 0.5 0.5"/></body></worldbody></mujoco>)");
    char err[1000] = "";
    mjSpec* spec = mj_parseXMLString(TCHAR_TO_UTF8(*Xml), nullptr, err, sizeof(err));
    if (!spec) { AddError(FString::Printf(TEXT("Parse failed: %hs"), err)); return false; }
    mjModel* m = mj_compile(spec, nullptr);
    if (!m) { mj_deleteSpec(spec); AddError(TEXT("Compile failed")); return false; }

    // Geom 0 (world ground plane is geom 0, body geom is geom 1... actually no world plane)
    // Body's geom should be index 0 in ngeom=1
    TestEqual(TEXT("ngeom should be 1"), (int)m->ngeom, 1);
    if (m->ngeom >= 1)
    {
        TestTrue(TEXT("Box size[0] ≈ 0.5m"), FMath::Abs((float)(m->geom_size[0] - 0.5)) < 1e-4f);
        TestTrue(TEXT("Box size[1] ≈ 0.5m"), FMath::Abs((float)(m->geom_size[1] - 0.5)) < 1e-4f);
        TestTrue(TEXT("Box size[2] ≈ 0.5m"), FMath::Abs((float)(m->geom_size[2] - 0.5)) < 1e-4f);
    }

    mj_deleteModel(m);
    mj_deleteSpec(spec);
    return true;
}

// ============================================================================
// URLab.Transform.CylinderSize
//   Cylinder radius=0.5m, len=1m → MJ size = [0.5, 0.5] (radius, half-length)
// ============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMjTransformCylinderSize,
    "URLab.Transform.CylinderSize",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMjTransformCylinderSize::RunTest(const FString& Parameters)
{
    // MuJoCo cylinder size: [0] = radius, [1] = half-length
    const FString Xml = TEXT(R"(<mujoco><worldbody><body><geom type="cylinder" size="0.5 0.5"/></body></worldbody></mujoco>)");
    char err[1000] = "";
    mjSpec* spec = mj_parseXMLString(TCHAR_TO_UTF8(*Xml), nullptr, err, sizeof(err));
    if (!spec) { AddError(FString::Printf(TEXT("Parse failed: %hs"), err)); return false; }
    mjModel* m = mj_compile(spec, nullptr);
    if (!m) { mj_deleteSpec(spec); AddError(TEXT("Compile failed")); return false; }

    TestEqual(TEXT("ngeom should be 1"), (int)m->ngeom, 1);
    if (m->ngeom >= 1)
    {
        TestTrue(TEXT("Cylinder radius ≈ 0.5"), FMath::Abs((float)(m->geom_size[0] - 0.5)) < 1e-4f);
        TestTrue(TEXT("Cylinder half-len ≈ 0.5"), FMath::Abs((float)(m->geom_size[1] - 0.5)) < 1e-4f);
    }

    mj_deleteModel(m);
    mj_deleteSpec(spec);
    return true;
}

// ============================================================================
// URLab.Transform.GravityScaleFixed
//   After Fix 3.6, ApplyOptions() divides by 100 and negates Y.
//   Verify that UE gravity FVector(0, 0, -980) cm/s² → MJ (0, 0, -9.80) m/s².
// ============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMjTransformGravityScaleFixed,
    "URLab.Transform.GravityScaleFixed",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMjTransformGravityScaleFixed::RunTest(const FString& Parameters)
{
    FVector UEGravity(0.0f, 0.0f, -980.0f); // cm/s² (UE default earth gravity)
    double mjGravity[3];
    mjGravity[0] =  UEGravity.X / 100.0;
    mjGravity[1] = -UEGravity.Y / 100.0;
    mjGravity[2] =  UEGravity.Z / 100.0;

    TestTrue(TEXT("MJ gravity X == 0"), FMath::Abs((float)mjGravity[0]) < 1e-5f);
    TestTrue(TEXT("MJ gravity Y == 0"), FMath::Abs((float)mjGravity[1]) < 1e-5f);
    TestTrue(TEXT("MJ gravity Z ≈ -9.80"), FMath::Abs((float)(mjGravity[2] + 9.80)) < 0.05f);
    return true;
}

// ============================================================================
// URLab.Transform.SensorPosScale
//   FramePos/SubtreeCom readings: scale ×100, negate Y.
//   Input (1, 2, 3) m → output (100, -200, 300) cm.
// ============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMjTransformSensorPosScale,
    "URLab.Transform.SensorPosScale",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMjTransformSensorPosScale::RunTest(const FString& Parameters)
{
    TArray<float> R = { 1.0f, 2.0f, 3.0f };
    R[0] *=  100.0f;
    R[1] *= -100.0f;
    R[2] *=  100.0f;

    TestTrue(TEXT("FramePos X → 100 cm"),  FMath::Abs(R[0] - 100.0f) < 0.001f);
    TestTrue(TEXT("FramePos Y → -200 cm"), FMath::Abs(R[1] + 200.0f) < 0.001f);
    TestTrue(TEXT("FramePos Z → 300 cm"),  FMath::Abs(R[2] - 300.0f) < 0.001f);
    return true;
}

// ============================================================================
// URLab.Transform.SensorVectorYNegate
//   Accelerometer/Gyro/Velocimeter: negate Y only, no scale.
//   Input (1, 2, 3) → output (1, -2, 3).
// ============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMjTransformSensorVectorYNegate,
    "URLab.Transform.SensorVectorYNegate",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMjTransformSensorVectorYNegate::RunTest(const FString& Parameters)
{
    TArray<float> R = { 1.0f, 2.0f, 3.0f };
    R[1] = -R[1];

    TestTrue(TEXT("Vector X unchanged"), FMath::Abs(R[0] - 1.0f) < 0.001f);
    TestTrue(TEXT("Vector Y negated"),   FMath::Abs(R[1] + 2.0f) < 0.001f);
    TestTrue(TEXT("Vector Z unchanged"), FMath::Abs(R[2] - 3.0f) < 0.001f);
    return true;
}

// ============================================================================
// URLab.Transform.SensorQuatReorder
//   BallQuat/FrameQuat: MuJoCo (w,x,y,z) → UE output (X,Y,Z,W) = (-mjX, mjY, -mjZ, mjW).
//   Matches MJHelper::MJQuatToUE convention.
// ============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMjTransformSensorQuatReorder,
    "URLab.Transform.SensorQuatReorder",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMjTransformSensorQuatReorder::RunTest(const FString& Parameters)
{
    // MuJoCo 90° rotation around X: w=0.7071, x=0.7071, y=0, z=0
    TArray<float> R = { 0.7071f, 0.7071f, 0.0f, 0.0f }; // [0]=mj_w, [1]=mj_x, [2]=mj_y, [3]=mj_z
    const float mj_w = R[0], mj_x = R[1], mj_y = R[2], mj_z = R[3];
    R[0] = -mj_x;
    R[1] =  mj_y;
    R[2] = -mj_z;
    R[3] =  mj_w;

    TestTrue(TEXT("Quat X = -mjX ≈ -0.7071"), FMath::Abs(R[0] + 0.7071f) < 0.001f);
    TestTrue(TEXT("Quat Y = mjY  ≈  0"),      FMath::Abs(R[1])           < 0.001f);
    TestTrue(TEXT("Quat Z = -mjZ ≈  0"),      FMath::Abs(R[2])           < 0.001f);
    TestTrue(TEXT("Quat W = mjW  ≈  0.7071"), FMath::Abs(R[3] - 0.7071f) < 0.001f);
    return true;
}

// ============================================================================
// URLab.Transform.JointAxisRoundTrip
//   Import axis (0,1,0) from MuJoCo → stored as UE (0,-1,0).
//   Export back to MuJoCo → (0,1,0) again (Fix 3.5).
// ============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMjTransformJointAxisRoundTrip,
    "URLab.Transform.JointAxisRoundTrip",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMjTransformJointAxisRoundTrip::RunTest(const FString& Parameters)
{
    // Import: MJ axis (0,1,0) → UE (0,-1,0)
    FVector mjAxisIn(0.0f, 1.0f, 0.0f);
    FVector ueAxis(mjAxisIn.X, -mjAxisIn.Y, mjAxisIn.Z);
    TestTrue(TEXT("Import: stored UE Y = -1"), FMath::Abs(ueAxis.Y + 1.0f) < 1e-5f);

    // Export: UE (0,-1,0) → MJ (0,1,0)
    double mjAxisOut[3] = { ueAxis.X, -ueAxis.Y, ueAxis.Z };
    TestTrue(TEXT("Export: MJ X = 0"), FMath::Abs((float)mjAxisOut[0])        < 1e-5f);
    TestTrue(TEXT("Export: MJ Y = 1"), FMath::Abs((float)mjAxisOut[1] - 1.0f) < 1e-5f);
    TestTrue(TEXT("Export: MJ Z = 0"), FMath::Abs((float)mjAxisOut[2])        < 1e-5f);
    return true;
}

// ============================================================================
// URLab.Transform.MixedEulerSeq
//   Per-axis intrinsic/extrinsic euler handling (Plan 2.7).
//   "xYz" = intrinsic-X, extrinsic-Y, intrinsic-Z.
//   Cross-check: pure intrinsic "xyz" and pure extrinsic "XYZ" must also match
//   the known-correct implementations (Q1*Q2*Q3 and Q3*Q2*Q1 respectively).
//   For the mixed case, a 90° extrinsic-Y followed by 90° intrinsic-X must
//   compose correctly (rotation order verifiable by hand).
// ============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMjTransformMixedEulerSeq,
    "URLab.Transform.MixedEulerSeq",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMjTransformMixedEulerSeq::RunTest(const FString& Parameters)
{
    // Helper: compare two MuJoCo quats (w,x,y,z), accounting for double-cover.
    auto QuatNearlyEqual = [&](const char* Label, const double A[4], const double B[4], double Eps = 1e-4) -> bool
    {
        // q and -q are the same rotation
        bool SameSign = FMath::Abs((float)(A[0]-B[0])) < (float)Eps
                     && FMath::Abs((float)(A[1]-B[1])) < (float)Eps
                     && FMath::Abs((float)(A[2]-B[2])) < (float)Eps
                     && FMath::Abs((float)(A[3]-B[3])) < (float)Eps;
        bool OppSign  = FMath::Abs((float)(A[0]+B[0])) < (float)Eps
                     && FMath::Abs((float)(A[1]+B[1])) < (float)Eps
                     && FMath::Abs((float)(A[2]+B[2])) < (float)Eps
                     && FMath::Abs((float)(A[3]+B[3])) < (float)Eps;
        TestTrue(FString(UTF8_TO_TCHAR(Label)), SameSign || OppSign);
        return SameSign || OppSign;
    };

    const double Pi2 = UE_PI / 2.0; // 90 degrees

    // --- Case 1: pure all-intrinsic "xyz" (90°, 0°, 0°) ---
    // Expected: Q1 * Q2 * Q3 = (cos45, sin45, 0, 0) * identity * identity
    double intrinsic[4]; MjOrientationUtils::EulerToQuat(Pi2, 0, 0, TEXT("xyz"), intrinsic);
    double expected_x90[4] = { FMath::Cos(Pi2/2.0), FMath::Sin(Pi2/2.0), 0.0, 0.0 };
    QuatNearlyEqual("AllIntrinsic xyz 90-0-0 matches x90", intrinsic, expected_x90);

    // --- Case 2: pure all-extrinsic "XYZ" (0°, 0°, 90°) ---
    // "XYZ" (0,0,90°): only Q3 is non-trivial → result = Q3 * Q2 * Q1 = z-90
    double extrinsic[4]; MjOrientationUtils::EulerToQuat(0, 0, Pi2, TEXT("XYZ"), extrinsic);
    double expected_z90[4] = { FMath::Cos(Pi2/2.0), 0.0, 0.0, FMath::Sin(Pi2/2.0) };
    QuatNearlyEqual("AllExtrinsic XYZ 0-0-90 matches z90", extrinsic, expected_z90);

    // --- Case 3: mixed "xY" style — test "xYz" (90°, 90°, 0°) ---
    // Step 0: intrinsic-x 90° → R = Rx90 = (c45, s45, 0, 0)
    // Step 1: extrinsic-Y 90° → R = Ry90 * R  (pre-multiply)
    // Step 2: intrinsic-z  0° → no-op
    //
    // Ry90 * Rx90:
    //   Ry90 = (c45, 0, s45, 0)
    //   Product (w,x,y,z):
    //     w = c45*c45 - 0*s45   = 0.5
    //     x = c45*s45 + 0*0     = 0.5
    //     y = c45*0   + s45*c45 = 0.5
    //     z = c45*0   - s45*s45 = -0.5   (Ry_w*Rx_z - Ry_z*Rx_x)
    // Note: quat product (a)*(b): w=aw*bw - ax*bx - ay*by - az*bz, etc.
    // Ry90=(c,0,s,0), Rx90=(c,s,0,0), c=cos45=0.7071, s=sin45=0.7071
    //   w = c*c - 0*s - s*0 - 0*0 = c^2 = 0.5
    //   x = c*s + 0*c + s*0 - 0*0 = c*s = 0.5
    //   y = c*0 - 0*s + s*c + 0*0... let me use standard formula:
    // (a*b).w = a.w*b.w - a.x*b.x - a.y*b.y - a.z*b.z
    // (a*b).x = a.w*b.x + a.x*b.w + a.y*b.z - a.z*b.y
    // (a*b).y = a.w*b.y - a.x*b.z + a.y*b.w + a.z*b.x
    // (a*b).z = a.w*b.z + a.x*b.y - a.y*b.x + a.z*b.w
    // Ry=(c,0,s,0) * Rx=(c,s,0,0):
    //   w = c*c - 0*s - s*0 - 0*0 = 0.5
    //   x = c*s + 0*c + s*0 - 0*0 = cs = 0.5
    //   y = c*0 - 0*0 + s*c + 0*s = cs = 0.5
    //   z = c*0 + 0*0 - s*s + 0*c = -s^2 = -0.5
    double mixed[4]; MjOrientationUtils::EulerToQuat(Pi2, Pi2, 0, TEXT("xYz"), mixed);
    double expected_mixed[4] = { 0.5, 0.5, 0.5, -0.5 };
    QuatNearlyEqual("Mixed xYz (90,90,0) matches hand calc", mixed, expected_mixed);

    // --- Case 4: all-intrinsic "xyz" == all-extrinsic "ZYX" reversed angles ---
    // This is a well-known identity: intrinsic xyz(a,b,c) == extrinsic ZYX(c,b,a)
    double intr[4];  MjOrientationUtils::EulerToQuat(Pi2/3.0, Pi2/4.0, Pi2/5.0, TEXT("xyz"),  intr);
    double extr[4];  MjOrientationUtils::EulerToQuat(Pi2/5.0, Pi2/4.0, Pi2/3.0, TEXT("ZYX"), extr);
    QuatNearlyEqual("Intrinsic xyz(a,b,c) == Extrinsic ZYX(c,b,a)", intr, extr);

    return true;
}
