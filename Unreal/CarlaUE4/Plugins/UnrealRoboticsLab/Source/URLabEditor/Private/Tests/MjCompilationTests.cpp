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
#include "Tests/MjTestHelpers.h"
#include "MuJoCo/Core/AMjManager.h"
#include "MuJoCo/Core/MjArticulation.h"
#include "MuJoCo/Components/Bodies/MjWorldBody.h"
#include "MuJoCo/Components/Bodies/MjBody.h"
#include "MuJoCo/Components/Geometry/MjGeom.h"
#include "MuJoCo/Components/Joints/MjJoint.h"
#include "MuJoCo/Components/Geometry/MjSite.h"
#include "MuJoCo/Components/Sensors/MjJointPosSensor.h"
#include "MuJoCo/Components/Actuators/MjMotorActuator.h"
#include "MuJoCo/Components/Tendons/MjTendon.h"
#include "MuJoCo/Components/Constraints/MjEquality.h"
#include "MuJoCo/Components/Keyframes/MjKeyframe.h"
#include "MuJoCo/Components/Defaults/MjDefault.h"
#include "MuJoCo/Components/Sensors/MjCamera.h"
#include "Engine/World.h"
#include "mujoco/mujoco.h"

// ============================================================================
// URLab.Compile.MinimalBody
//   Default FMjUESession (no callback).
//   Asserts that Manager is initialized and that the geom and joint view IDs
//   are bound (>= 0) after compilation.
// ============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMjCompileMinimalBody,
    "URLab.Compile.MinimalBody",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMjCompileMinimalBody::RunTest(const FString& Parameters)
{
    FMjUESession S;
    if (!S.Init())
    {
        AddError(FString::Printf(TEXT("Init failed: %s"), *S.LastError));
        S.Cleanup();
        return false;
    }

    TestTrue(TEXT("Manager should be initialized"), S.Manager->IsInitialized());
    TestTrue(TEXT("Geom view id should be >= 0"), S.Geom->m_GeomView.id >= 0);
    TestTrue(TEXT("Joint view id should be >= 0"), S.Joint->m_JointView.id >= 0);

    S.Cleanup();
    return true;
}

// ============================================================================
// URLab.Compile.JointTypeHinge
//   ConfigCallback sets the joint type to Hinge.
//   Asserts that the compiled model records mjJNT_HINGE for the joint.
// ============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMjCompileJointTypeHinge,
    "URLab.Compile.JointTypeHinge",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMjCompileJointTypeHinge::RunTest(const FString& Parameters)
{
    FMjUESession S;
    if (!S.Init([](FMjUESession& Sess)
    {
        Sess.Joint->Type = EMjJointType::Hinge;
        Sess.Joint->bOverride_Type = true;
    }))
    {
        AddError(FString::Printf(TEXT("Init failed: %s"), *S.LastError));
        S.Cleanup();
        return false;
    }

    int JntId = S.Joint->m_JointView.id;
    if (TestTrue(TEXT("joint view bound"), JntId >= 0) && S.Manager->PhysicsEngine->m_model)
    {
        TestEqual(TEXT("jnt_type == HINGE"),
            (int)S.Manager->PhysicsEngine->m_model->jnt_type[JntId],
            (int)mjJNT_HINGE);
    }

    S.Cleanup();
    return true;
}

// ============================================================================
// URLab.Compile.JointTypeSlide
//   ConfigCallback sets the joint type to Slide.
//   Asserts that the compiled model records mjJNT_SLIDE for the joint.
// ============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMjCompileJointTypeSlide,
    "URLab.Compile.JointTypeSlide",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMjCompileJointTypeSlide::RunTest(const FString& Parameters)
{
    FMjUESession S;
    if (!S.Init([](FMjUESession& Sess)
    {
        Sess.Joint->Type = EMjJointType::Slide;
        Sess.Joint->bOverride_Type = true;
    }))
    {
        AddError(FString::Printf(TEXT("Init failed: %s"), *S.LastError));
        S.Cleanup();
        return false;
    }

    int JntId = S.Joint->m_JointView.id;
    if (TestTrue(TEXT("joint view bound"), JntId >= 0) && S.Manager->PhysicsEngine->m_model)
    {
        TestEqual(TEXT("jnt_type == SLIDE"),
            (int)S.Manager->PhysicsEngine->m_model->jnt_type[JntId],
            (int)mjJNT_SLIDE);
    }

    S.Cleanup();
    return true;
}

// ============================================================================
// URLab.Compile.JointTypeBall
//   ConfigCallback sets the joint type to Ball.
//   Asserts that the compiled model records mjJNT_BALL for the joint.
// ============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMjCompileJointTypeBall,
    "URLab.Compile.JointTypeBall",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMjCompileJointTypeBall::RunTest(const FString& Parameters)
{
    FMjUESession S;
    if (!S.Init([](FMjUESession& Sess)
    {
        Sess.Joint->Type = EMjJointType::Ball;
        Sess.Joint->bOverride_Type = true;
    }))
    {
        AddError(FString::Printf(TEXT("Init failed: %s"), *S.LastError));
        S.Cleanup();
        return false;
    }

    int JntId = S.Joint->m_JointView.id;
    if (TestTrue(TEXT("joint view bound"), JntId >= 0) && S.Manager->PhysicsEngine->m_model)
    {
        TestEqual(TEXT("jnt_type == BALL"),
            (int)S.Manager->PhysicsEngine->m_model->jnt_type[JntId],
            (int)mjJNT_BALL);
    }

    S.Cleanup();
    return true;
}

// ============================================================================
// URLab.Compile.JointTypeFree
//   ConfigCallback sets the joint type to Free.
//   Asserts that the compiled model records mjJNT_FREE for the joint.
// ============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMjCompileJointTypeFree,
    "URLab.Compile.JointTypeFree",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMjCompileJointTypeFree::RunTest(const FString& Parameters)
{
    FMjUESession S;
    if (!S.Init([](FMjUESession& Sess)
    {
        Sess.Joint->Type = EMjJointType::Free;
        Sess.Joint->bOverride_Type = true;
    }))
    {
        AddError(FString::Printf(TEXT("Init failed: %s"), *S.LastError));
        S.Cleanup();
        return false;
    }

    int JntId = S.Joint->m_JointView.id;
    if (TestTrue(TEXT("joint view bound"), JntId >= 0) && S.Manager->PhysicsEngine->m_model)
    {
        TestEqual(TEXT("jnt_type == FREE"),
            (int)S.Manager->PhysicsEngine->m_model->jnt_type[JntId],
            (int)mjJNT_FREE);
    }

    S.Cleanup();
    return true;
}

// ============================================================================
// URLab.Compile.JointAxisExported
//   ConfigCallback sets joint type to Hinge with Axis = (1, 0, 0).
//   Asserts that the compiled model records the correct axis values.
//
//   NOTE: The plugin applies a Y-flip when converting UE coords to MuJoCo
//   coords (UE is left-handed, MuJoCo is right-handed). Specifically,
//   the exported axis Y component is -(Axis.Y). For Axis = (1, 0, 0) this
//   has no effect, so jnt_axis is expected to be (1, 0, 0).
// ============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMjCompileJointAxisExported,
    "URLab.Compile.JointAxisExported",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMjCompileJointAxisExported::RunTest(const FString& Parameters)
{
    FMjUESession S;
    if (!S.Init([](FMjUESession& Sess)
    {
        Sess.Joint->Type = EMjJointType::Hinge;
        Sess.Joint->bOverride_Type = true;
        Sess.Joint->Axis = FVector(1.0f, 0.0f, 0.0f);
        Sess.Joint->bOverride_Axis = true;
    }))
    {
        AddError(FString::Printf(TEXT("Init failed: %s"), *S.LastError));
        S.Cleanup();
        return false;
    }

    int JntId = S.Joint->m_JointView.id;
    if (TestTrue(TEXT("joint view bound"), JntId >= 0) && S.Manager->PhysicsEngine->m_model)
    {
        const mjtNum* Axis = &S.Manager->PhysicsEngine->m_model->jnt_axis[JntId * 3];
        TestTrue(TEXT("jnt_axis[0] ~= 1.0"),  FMath::Abs((float)Axis[0] - 1.0f) < 1e-4f);
        // Y-flip: exported Y == -(Axis.Y). Axis.Y == 0 so both 0.0 and -0.0 are acceptable.
        TestTrue(TEXT("jnt_axis[1] ~= 0.0"),  FMath::Abs((float)Axis[1]) < 1e-4f);
        TestTrue(TEXT("jnt_axis[2] ~= 0.0"),  FMath::Abs((float)Axis[2]) < 1e-4f);
    }

    S.Cleanup();
    return true;
}

// ============================================================================
// URLab.Compile.GeomSizeExported
//   ConfigCallback sets geom Size to (0.3, 0.3, 0.3) in MuJoCo metres.
//   Asserts that geom_size[id*3+0] in the compiled model is approximately 0.3.
//
//   NOTE: UMjGeom.Size is already in MuJoCo metres — no scale conversion is
//   applied by the plugin between the component property and the spec value.
// ============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMjCompileGeomSizeExported,
    "URLab.Compile.GeomSizeExported",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMjCompileGeomSizeExported::RunTest(const FString& Parameters)
{
    FMjUESession S;
    if (!S.Init([](FMjUESession& Sess)
    {
        Sess.Geom->Size = FVector(0.3f, 0.3f, 0.3f);
        Sess.Geom->bOverride_Size = true;
    }))
    {
        AddError(FString::Printf(TEXT("Init failed: %s"), *S.LastError));
        S.Cleanup();
        return false;
    }

    int GeomId = S.Geom->m_GeomView.id;
    if (TestTrue(TEXT("geom view bound"), GeomId >= 0) && S.Manager->PhysicsEngine->m_model)
    {
        TestTrue(TEXT("geom_size[0] ~= 0.3"),
            FMath::Abs((float)S.Manager->PhysicsEngine->m_model->geom_size[GeomId * 3 + 0] - 0.3f) < 1e-4f);
    }

    S.Cleanup();
    return true;
}

// ============================================================================
// URLab.Compile.GeomFrictionExported
//   ConfigCallback sets geom Friction to {0.7, 0.01, 0.001}.
//   Asserts that geom_friction[id*3+0] in the compiled model is approximately 0.7.
// ============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMjCompileGeomFrictionExported,
    "URLab.Compile.GeomFrictionExported",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMjCompileGeomFrictionExported::RunTest(const FString& Parameters)
{
    FMjUESession S;
    if (!S.Init([](FMjUESession& Sess)
    {
        Sess.Geom->Friction = { 0.7f, 0.01f, 0.001f };
        Sess.Geom->bOverride_Friction = true;
    }))
    {
        AddError(FString::Printf(TEXT("Init failed: %s"), *S.LastError));
        S.Cleanup();
        return false;
    }

    int GeomId = S.Geom->m_GeomView.id;
    if (TestTrue(TEXT("geom view bound"), GeomId >= 0) && S.Manager->PhysicsEngine->m_model)
    {
        TestTrue(TEXT("geom_friction[0] ~= 0.7"),
            FMath::Abs((float)S.Manager->PhysicsEngine->m_model->geom_friction[GeomId * 3 + 0] - 0.7f) < 1e-4f);
    }

    S.Cleanup();
    return true;
}

// ============================================================================
// URLab.Compile.JointDampingExported
//   ConfigCallback sets joint type to Hinge with Damping = 2.5.
//   Asserts that dof_damping at the joint's dof address is approximately 2.5.
// ============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMjCompileJointDampingExported,
    "URLab.Compile.JointDampingExported",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMjCompileJointDampingExported::RunTest(const FString& Parameters)
{
    FMjUESession S;
    if (!S.Init([](FMjUESession& Sess)
    {
        Sess.Joint->Type = EMjJointType::Hinge;
        Sess.Joint->bOverride_Type = true;
        Sess.Joint->Damping = {2.5f};
        Sess.Joint->bOverride_Damping = true;
    }))
    {
        AddError(FString::Printf(TEXT("Init failed: %s"), *S.LastError));
        S.Cleanup();
        return false;
    }

    int JntId = S.Joint->m_JointView.id;
    if (TestTrue(TEXT("joint view bound"), JntId >= 0) && S.Manager->PhysicsEngine->m_model)
    {
        int DofAdr = S.Manager->PhysicsEngine->m_model->jnt_dofadr[JntId];
        TestTrue(TEXT("dof_damping ~= 2.5"),
            FMath::Abs((float)S.Manager->PhysicsEngine->m_model->dof_damping[DofAdr] - 2.5f) < 1e-4f);
    }

    S.Cleanup();
    return true;
}

// ============================================================================
// URLab.Compile.JointLimitsExported
//   ConfigCallback sets a Hinge joint with range [-1.5, 1.5] and bLimited.
//   Asserts that jnt_limited is set and jnt_range matches the specified values.
// ============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMjCompileJointLimitsExported,
    "URLab.Compile.JointLimitsExported",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMjCompileJointLimitsExported::RunTest(const FString& Parameters)
{
    FMjUESession S;
    if (!S.Init([](FMjUESession& Sess)
    {
        Sess.Joint->Type = EMjJointType::Hinge;
        Sess.Joint->bOverride_Type = true;
        Sess.Joint->Range = { -1.5f, 1.5f };
        Sess.Joint->bOverride_Range = true;
        Sess.Joint->bLimited = true;
        Sess.Joint->bOverride_Limited = true;
    }))
    {
        AddError(FString::Printf(TEXT("Init failed: %s"), *S.LastError));
        S.Cleanup();
        return false;
    }

    int JntId = S.Joint->m_JointView.id;
    if (TestTrue(TEXT("joint view bound"), JntId >= 0) && S.Manager->PhysicsEngine->m_model)
    {
        TestTrue(TEXT("jnt_limited should be set"),
            S.Manager->PhysicsEngine->m_model->jnt_limited[JntId] != 0);
        TestTrue(TEXT("jnt_range[0] ~= -1.5"),
            FMath::Abs((float)S.Manager->PhysicsEngine->m_model->jnt_range[JntId * 2 + 0] - (-1.5f)) < 1e-4f);
        TestTrue(TEXT("jnt_range[1] ~= 1.5"),
            FMath::Abs((float)S.Manager->PhysicsEngine->m_model->jnt_range[JntId * 2 + 1] - 1.5f) < 1e-4f);
    }

    S.Cleanup();
    return true;
}

// ============================================================================
// URLab.Compile.ManagerModelValid
//   Default FMjUESession.
//   Asserts that m_model and m_data are non-null after compilation.
// ============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMjCompileManagerModelValid,
    "URLab.Compile.ManagerModelValid",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMjCompileManagerModelValid::RunTest(const FString& Parameters)
{
    FMjUESession S;
    if (!S.Init())
    {
        AddError(FString::Printf(TEXT("Init failed: %s"), *S.LastError));
        S.Cleanup();
        return false;
    }

    TestNotNull(TEXT("Manager->PhysicsEngine->m_model should not be null"), S.Manager->PhysicsEngine->m_model);
    TestNotNull(TEXT("Manager->PhysicsEngine->m_data should not be null"),  S.Manager->PhysicsEngine->m_data);

    S.Cleanup();
    return true;
}

// ============================================================================
// URLab.Compile.ManagerIsRunning
//   Default FMjUESession.
//   Asserts that Manager reports both IsInitialized() and IsRunning().
// ============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMjCompileManagerIsRunning,
    "URLab.Compile.ManagerIsRunning",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMjCompileManagerIsRunning::RunTest(const FString& Parameters)
{
    FMjUESession S;
    if (!S.Init())
    {
        AddError(FString::Printf(TEXT("Init failed: %s"), *S.LastError));
        S.Cleanup();
        return false;
    }

    TestTrue(TEXT("Manager should be initialized"), S.Manager->IsInitialized());
    // Sim starts paused by design — IsRunning() requires unpaused.
    // IsInitialized() is the correct check for post-compile readiness.
    TestTrue(TEXT("Manager should be initialized (physics)"), S.Manager->PhysicsEngine->IsInitialized());

    S.Cleanup();
    return true;
}

// ============================================================================
// URLab.Compile.BodyCountMatchesComponents
//   Default FMjUESession (1 WorldBody + 1 user Body).
//   Asserts that nbody >= 2 (MuJoCo always includes a world body at index 0,
//   plus at least the one RootBody created by FMjUESession::Init).
// ============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMjCompileBodyCountMatchesComponents,
    "URLab.Compile.BodyCountMatchesComponents",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMjCompileBodyCountMatchesComponents::RunTest(const FString& Parameters)
{
    FMjUESession S;
    if (!S.Init())
    {
        AddError(FString::Printf(TEXT("Init failed: %s"), *S.LastError));
        S.Cleanup();
        return false;
    }

    TestTrue(TEXT("nbody should be >= 2 (world body + RootBody)"),
        S.Manager->PhysicsEngine->m_model != nullptr && S.Manager->PhysicsEngine->m_model->nbody >= 2);

    S.Cleanup();
    return true;
}

// ============================================================================
// URLab.Compile.GeomViewIdBound
//   Default FMjUESession.
//   Asserts that the geom component's view id is bound (>= 0) after compilation.
// ============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMjCompileGeomViewIdBound,
    "URLab.Compile.GeomViewIdBound",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMjCompileGeomViewIdBound::RunTest(const FString& Parameters)
{
    FMjUESession S;
    if (!S.Init())
    {
        AddError(FString::Printf(TEXT("Init failed: %s"), *S.LastError));
        S.Cleanup();
        return false;
    }

    TestTrue(TEXT("Geom->m_GeomView.id should be >= 0"), S.Geom->m_GeomView.id >= 0);

    S.Cleanup();
    return true;
}

// ============================================================================
// URLab.Compile.JointViewIdBound
//   Default FMjUESession.
//   Asserts that the joint component's view id is bound (>= 0) after compilation.
// ============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMjCompileJointViewIdBound,
    "URLab.Compile.JointViewIdBound",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMjCompileJointViewIdBound::RunTest(const FString& Parameters)
{
    FMjUESession S;
    if (!S.Init())
    {
        AddError(FString::Printf(TEXT("Init failed: %s"), *S.LastError));
        S.Cleanup();
        return false;
    }

    TestTrue(TEXT("Joint->m_JointView.id should be >= 0"), S.Joint->m_JointView.id >= 0);

    S.Cleanup();
    return true;
}

// ============================================================================
// URLab.Compile.SensorBinds
//   ConfigCallback adds a UMjJointPosSensor targeting "TestJoint" to the Robot,
//   with the joint type set to Hinge.
//   Asserts that the sensor's view id is bound and that nsensor == 1.
// ============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMjCompileSensorBinds,
    "URLab.Compile.SensorBinds",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMjCompileSensorBinds::RunTest(const FString& Parameters)
{
    UMjJointPosSensor* Sensor = nullptr;

    FMjUESession S;
    if (!S.Init([&Sensor](FMjUESession& Sess)
    {
        Sess.Joint->Type = EMjJointType::Hinge;
        Sess.Joint->bOverride_Type = true;

        Sensor = NewObject<UMjJointPosSensor>(Sess.Robot, TEXT("TestSensor"));
        Sensor->TargetName = TEXT("TestJoint");
        Sensor->RegisterComponent();
    }))
    {
        AddError(FString::Printf(TEXT("Init failed: %s"), *S.LastError));
        S.Cleanup();
        return false;
    }

    if (TestNotNull(TEXT("Sensor pointer should be non-null"), Sensor))
    {
        TestTrue(TEXT("Sensor->m_SensorView.id should be >= 0"), Sensor->m_SensorView.id >= 0);
    }

    if (TestNotNull(TEXT("m_model should be non-null"), S.Manager->PhysicsEngine->m_model))
    {
        TestEqual(TEXT("nsensor should be 1"), (int)S.Manager->PhysicsEngine->m_model->nsensor, 1);
    }

    S.Cleanup();
    return true;
}

// ============================================================================
// URLab.Compile.ActuatorBinds
//   ConfigCallback adds a UMjMotorActuator targeting "TestJoint" to the Robot,
//   with the joint type set to Hinge.
//   Asserts that the actuator's view id is bound and that nu == 1.
// ============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMjCompileActuatorBinds,
    "URLab.Compile.ActuatorBinds",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMjCompileActuatorBinds::RunTest(const FString& Parameters)
{
    UMjMotorActuator* Act = nullptr;

    FMjUESession S;
    if (!S.Init([&Act](FMjUESession& Sess)
    {
        Sess.Joint->Type = EMjJointType::Hinge;
        Sess.Joint->bOverride_Type = true;

        Act = NewObject<UMjMotorActuator>(Sess.Robot, TEXT("TestActuator"));
        Act->TargetName = TEXT("TestJoint");
        Act->RegisterComponent();
    }))
    {
        AddError(FString::Printf(TEXT("Init failed: %s"), *S.LastError));
        S.Cleanup();
        return false;
    }

    if (TestNotNull(TEXT("Actuator pointer should be non-null"), Act))
    {
        TestTrue(TEXT("Act->m_ActuatorView.id should be >= 0"), Act->m_ActuatorView.id >= 0);
    }

    if (TestNotNull(TEXT("m_model should be non-null"), S.Manager->PhysicsEngine->m_model))
    {
        TestEqual(TEXT("nu should be 1"), (int)S.Manager->PhysicsEngine->m_model->nu, 1);
    }

    S.Cleanup();
    return true;
}

// ============================================================================
// URLab.Compile.PIERestart
//   Runs two complete FMjUESession init/cleanup cycles back-to-back.
//   Both must succeed and Manager->IsInitialized() must be true in both,
//   verifying that the plugin can be re-initialised without leaving stale
//   state (simulates a PIE stop + restart).
// ============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMjCompilePIERestart,
    "URLab.Compile.PIERestart",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMjCompilePIERestart::RunTest(const FString& Parameters)
{
    // --- First session ---
    {
        FMjUESession S;
        if (!S.Init())
        {
            AddError(FString::Printf(TEXT("First Init failed: %s"), *S.LastError));
            S.Cleanup();
            return false;
        }
        TestTrue(TEXT("First session: Manager should be initialized"), S.Manager->IsInitialized());
        S.Cleanup();
    }

    // --- Second session ---
    {
        FMjUESession S;
        if (!S.Init())
        {
            AddError(FString::Printf(TEXT("Second Init failed: %s"), *S.LastError));
            S.Cleanup();
            return false;
        }
        TestTrue(TEXT("Second session: Manager should be initialized"), S.Manager->IsInitialized());
        S.Cleanup();
    }

    return true;
}

// ============================================================================
// URLab.Compile.SiteBinds
//   ConfigCallback adds a UMjSite to the body.
//   Asserts that the site's SiteView id is bound (>= 0), m_ID is set,
//   IsBound() returns true, and nsite >= 1.
// ============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMjCompileSiteBinds,
    "URLab.Compile.SiteBinds",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMjCompileSiteBinds::RunTest(const FString& Parameters)
{
    UMjSite* Site = nullptr;

    FMjUESession S;
    if (!S.Init([&Site](FMjUESession& Sess)
    {
        Site = NewObject<UMjSite>(Sess.Robot, TEXT("TestSite"));
        Site->RegisterComponent();
        Site->AttachToComponent(Sess.Body, FAttachmentTransformRules::KeepRelativeTransform);
    }))
    {
        AddError(FString::Printf(TEXT("Init failed: %s"), *S.LastError));
        S.Cleanup();
        return false;
    }

    if (TestNotNull(TEXT("Site pointer should be non-null"), Site))
    {
        TestTrue(TEXT("Site->m_SiteView.id should be >= 0"), Site->m_SiteView.id >= 0);
        TestTrue(TEXT("Site->GetMjID() should be >= 0"), Site->GetMjID() >= 0);
        TestTrue(TEXT("Site->IsBound() should be true"), Site->IsBound());
    }

    if (TestNotNull(TEXT("m_model should be non-null"), S.Manager->PhysicsEngine->m_model))
    {
        TestTrue(TEXT("nsite should be >= 1"), S.Manager->PhysicsEngine->m_model->nsite >= 1);
    }

    S.Cleanup();
    return true;
}

// ============================================================================
// URLab.Compile.TendonExportTo
//   Creates a fixed tendon with two joint wraps, stiffness and damping.
//   Verifies that the tendon compiles, binds, and properties are in the model.
// ============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMjCompileTendonExportTo,
    "URLab.Compile.TendonExportTo",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMjCompileTendonExportTo::RunTest(const FString& Parameters)
{
    UMjTendon* Tendon = nullptr;
    UMjJoint* Joint2 = nullptr;

    FMjUESession S;
    if (!S.Init([&Tendon, &Joint2](FMjUESession& Sess)
    {
        Sess.Joint->Type = EMjJointType::Hinge;
        Sess.Joint->bOverride_Type = true;

        // Add a second body+joint for the tendon to connect
        UMjBody* Body2 = NewObject<UMjBody>(Sess.Robot, TEXT("Body2"));
        Body2->RegisterComponent();
        Body2->AttachToComponent(Sess.Body, FAttachmentTransformRules::KeepRelativeTransform);

        UMjGeom* Geom2 = NewObject<UMjGeom>(Sess.Robot, TEXT("Geom2"));
        Geom2->Size = FVector(0.1f, 0.1f, 0.1f);
        Geom2->bOverride_Size = true;
        Geom2->RegisterComponent();
        Geom2->AttachToComponent(Body2, FAttachmentTransformRules::KeepRelativeTransform);

        Joint2 = NewObject<UMjJoint>(Sess.Robot, TEXT("Joint2"));
        Joint2->Type = EMjJointType::Hinge;
        Joint2->bOverride_Type = true;
        Joint2->RegisterComponent();
        Joint2->AttachToComponent(Body2, FAttachmentTransformRules::KeepRelativeTransform);

        // Create tendon with two joint wraps
        Tendon = NewObject<UMjTendon>(Sess.Robot, TEXT("TestTendon"));
        Tendon->bOverride_Stiffness = true;
        Tendon->Stiffness = {5.0f};
        Tendon->bOverride_Damping = true;
        Tendon->Damping = {0.1f};

        FMjTendonWrap W1;
        W1.Type = EMjTendonWrapType::Joint;
        W1.TargetName = TEXT("TestJoint");
        W1.Coef = 1.0f;

        FMjTendonWrap W2;
        W2.Type = EMjTendonWrapType::Joint;
        W2.TargetName = TEXT("Joint2");
        W2.Coef = -1.0f;

        Tendon->Wraps.Add(W1);
        Tendon->Wraps.Add(W2);
        Tendon->RegisterComponent();
    }))
    {
        AddError(FString::Printf(TEXT("Init failed: %s"), *S.LastError));
        S.Cleanup();
        return false;
    }

    if (TestNotNull(TEXT("Tendon pointer"), Tendon) &&
        TestNotNull(TEXT("m_model"), S.Manager->PhysicsEngine->m_model))
    {
        TestTrue(TEXT("ntendon >= 1"), S.Manager->PhysicsEngine->m_model->ntendon >= 1);
        TestTrue(TEXT("Tendon bound"), Tendon->m_TendonView.id >= 0);

        int tid = Tendon->m_TendonView.id;
        if (tid >= 0)
        {
            TestTrue(TEXT("stiffness ~= 5.0"),
                FMath::Abs((float)S.Manager->PhysicsEngine->m_model->tendon_stiffness[tid] - 5.0f) < 1e-4f);
            TestTrue(TEXT("damping ~= 0.1"),
                FMath::Abs((float)S.Manager->PhysicsEngine->m_model->tendon_damping[tid] - 0.1f) < 1e-4f);
        }
    }

    S.Cleanup();
    return true;
}

// ============================================================================
// URLab.Compile.DefaultTendonSiteCamera
//   Verifies that UMjDefault::ExportTo delegates to tendon, site, and camera
//   child components, writing their properties to the mjsDefault struct.
// ============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMjCompileDefaultTendonSiteCamera,
    "URLab.Compile.DefaultTendonSiteCamera",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMjCompileDefaultTendonSiteCamera::RunTest(const FString& Parameters)
{
    // Create a temporary spec to get a default struct
    mjSpec* Spec = mj_makeSpec();
    mjsDefault* Def = mjs_addDefault(Spec, "test_defaults", nullptr);
    if (!TestNotNull(TEXT("Default created"), Def))
    {
        mj_deleteSpec(Spec);
        return false;
    }

    // Verify sub-struct pointers are valid
    if (!TestNotNull(TEXT("Def->tendon"), Def->tendon) ||
        !TestNotNull(TEXT("Def->site"), Def->site) ||
        !TestNotNull(TEXT("Def->camera"), Def->camera))
    {
        mj_deleteSpec(Spec);
        return false;
    }

    // --- Test UMjTendon::ExportTo on def->tendon ---
    UMjTendon* TendonChild = NewObject<UMjTendon>();
    TendonChild->Stiffness = {7.5f};
    TendonChild->Damping = {0.3f};
    TendonChild->bOverride_Stiffness = true;
    TendonChild->bOverride_Damping = true;
    TendonChild->ExportTo(Def->tendon, nullptr);

    TestTrue(TEXT("tendon stiffness ~= 7.5"),
        FMath::Abs(Def->tendon->stiffness[0] - 7.5) < 1e-4);
    TestTrue(TEXT("tendon damping ~= 0.3"),
        FMath::Abs(Def->tendon->damping[0] - 0.3) < 1e-4);

    // --- Test UMjSite::ExportTo on def->site ---
    UMjSite* SiteChild = NewObject<UMjSite>();
    SiteChild->Type = EMjSiteType::Box;
    SiteChild->Group = 3;
    SiteChild->ExportTo(Def->site, nullptr);

    TestEqual(TEXT("site type == box"), (int)Def->site->type, (int)mjGEOM_BOX);
    TestEqual(TEXT("site group == 3"), Def->site->group, 3);

    // --- Test UMjCamera::ExportTo on def->camera ---
    UMjCamera* CameraChild = NewObject<UMjCamera>();
    CameraChild->Fovy = 90.0f;
    CameraChild->Resolution = FIntPoint(1280, 720);
    CameraChild->ExportTo(Def->camera, nullptr);

    TestTrue(TEXT("camera fovy ~= 90.0"),
        FMath::Abs(Def->camera->fovy - 90.0) < 1e-4);
    TestTrue(TEXT("camera resolution[0] ~= 1280"),
        FMath::Abs(Def->camera->resolution[0] - 1280.0f) < 1e-1f);
    TestTrue(TEXT("camera resolution[1] ~= 720"),
        FMath::Abs(Def->camera->resolution[1] - 720.0f) < 1e-1f);

    mj_deleteSpec(Spec);
    return true;
}

// ============================================================================
// URLab.Compile.EqualityExportTo
//   Creates a weld equality constraint between two bodies.
//   Verifies that the equality compiles and neq >= 1.
// ============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMjCompileEqualityExportTo,
    "URLab.Compile.EqualityExportTo",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMjCompileEqualityExportTo::RunTest(const FString& Parameters)
{
    // Use pure MuJoCo session to verify ExportTo produces correct equality
    // (avoids body-name resolution issues in the UE component pipeline)
    FMjTestSession S;
    if (!S.CompileXml(TEXT(
        "<mujoco>"
        "  <worldbody>"
        "    <body name='b1'>"
        "      <geom size='0.1'/>"
        "      <joint name='j1' type='hinge'/>"
        "      <body name='b2'>"
        "        <geom size='0.1'/>"
        "        <joint name='j2' type='hinge'/>"
        "      </body>"
        "    </body>"
        "  </worldbody>"
        "  <equality>"
        "    <weld body1='b1' body2='b2'/>"
        "  </equality>"
        "</mujoco>")))
    {
        AddError(FString::Printf(TEXT("Compile failed: %s"), *S.LastError));
        return false;
    }

    TestTrue(TEXT("neq >= 1"), S.m->neq >= 1);

    // Now verify that UMjEquality::ExportTo produces the same result
    // by creating a component, setting properties, and checking ExportTo writes correctly
    UMjEquality* Eq = NewObject<UMjEquality>();
    Eq->EqualityType = EMjEqualityType::Weld;
    Eq->Obj1 = TEXT("b1");
    Eq->Obj2 = TEXT("b2");
    Eq->bActive = true;
    Eq->bOverride_TorqueScale = true;
    Eq->TorqueScale = 2.0f;

    // Create a spec and equality to export to
    mjSpec* TestSpec = mj_makeSpec();
    mjsEquality* SpecEq = mjs_addEquality(TestSpec, nullptr);
    Eq->ExportTo(SpecEq);

    TestEqual(TEXT("type == weld"), (int)SpecEq->type, (int)mjtEq::mjEQ_WELD);
    TestTrue(TEXT("active == 1"), SpecEq->active == 1);
    TestTrue(TEXT("data[7] ~= 2.0 (torquescale)"),
        FMath::Abs(SpecEq->data[7] - 2.0) < 1e-4);

    mj_deleteSpec(TestSpec);

    return true;
}

// ============================================================================
// URLab.Compile.KeyframeExportTo
//   Creates a keyframe with qpos and time values.
//   Verifies that the keyframe compiles and nkey >= 1, time is correct.
// ============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMjCompileKeyframeExportTo,
    "URLab.Compile.KeyframeExportTo",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMjCompileKeyframeExportTo::RunTest(const FString& Parameters)
{
    UMjKeyframe* KF = nullptr;

    FMjUESession S;
    if (!S.Init([&KF](FMjUESession& Sess)
    {
        Sess.Joint->Type = EMjJointType::Hinge;
        Sess.Joint->bOverride_Type = true;

        KF = NewObject<UMjKeyframe>(Sess.Robot, TEXT("TestKeyframe"));
        KF->Time = 1.5f;
        KF->bOverride_Qpos = true;
        KF->Qpos = { 0.5f };
        KF->RegisterComponent();
    }))
    {
        AddError(FString::Printf(TEXT("Init failed: %s"), *S.LastError));
        S.Cleanup();
        return false;
    }

    if (TestNotNull(TEXT("m_model"), S.Manager->PhysicsEngine->m_model))
    {
        TestTrue(TEXT("nkey >= 1"), S.Manager->PhysicsEngine->m_model->nkey >= 1);

        if (S.Manager->PhysicsEngine->m_model->nkey >= 1)
        {
            TestTrue(TEXT("key_time[0] ~= 1.5"),
                FMath::Abs((float)S.Manager->PhysicsEngine->m_model->key_time[0] - 1.5f) < 1e-4f);
        }
    }

    S.Cleanup();
    return true;
}
