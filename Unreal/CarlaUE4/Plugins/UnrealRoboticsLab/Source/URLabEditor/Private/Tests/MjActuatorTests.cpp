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
#include "MuJoCo/Components/Bodies/MjBody.h"
#include "MuJoCo/Components/Geometry/MjGeom.h"
#include "MuJoCo/Components/Joints/MjJoint.h"
#include "MuJoCo/Components/Actuators/MjActuator.h"
#include "MuJoCo/Components/Actuators/MjMotorActuator.h"
#include "Engine/World.h"
#include "mujoco/mujoco.h"

// ============================================================================
// URLab.Actuator.MotorActuator_Binds
//   Verify that a UMjMotorActuator targeting a hinge joint receives a valid
//   actuator ID after compilation, and that nu == 1.
// ============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMjActuatorMotorActuatorBinds,
    "URLab.Actuator.MotorActuator_Binds",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMjActuatorMotorActuatorBinds::RunTest(const FString& Parameters)
{
    UMjMotorActuator* Actuator = nullptr;

    FMjUESession S;
    if (!S.Init([&Actuator](FMjUESession& Sess)
    {
        Sess.Joint->Type = EMjJointType::Hinge;
        Sess.Joint->bOverride_Type = true;

        Actuator = NewObject<UMjMotorActuator>(Sess.Robot, TEXT("TestActuator"));
        Actuator->TargetName = TEXT("TestJoint");
        Actuator->RegisterComponent();
    }))
    {
        AddError(FString::Printf(TEXT("FMjUESession::Init failed: %s"), *S.LastError));
        S.Cleanup();
        return false;
    }

    TestNotNull(TEXT("Actuator should not be null after Init"), Actuator);
    if (Actuator)
    {
        TestTrue(TEXT("Actuator->m_ActuatorView.id should be >= 0 after Bind()"),
            Actuator->m_ActuatorView.id >= 0);
    }

    TestEqual(TEXT("Manager->PhysicsEngine->m_model->nu should be 1"),
        (int)S.Manager->PhysicsEngine->m_model->nu, 1);

    S.Cleanup();
    return true;
}

// ============================================================================
// URLab.Actuator.MotorActuator_SetControl
//   Call SetControl(5.0f) on a bound motor actuator and verify that
//   GetControl() returns approximately 5.0.
// ============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMjActuatorMotorActuatorSetControl,
    "URLab.Actuator.MotorActuator_SetControl",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMjActuatorMotorActuatorSetControl::RunTest(const FString& Parameters)
{
    UMjMotorActuator* Actuator = nullptr;

    FMjUESession S;
    if (!S.Init([&Actuator](FMjUESession& Sess)
    {
        Sess.Joint->Type = EMjJointType::Hinge;
        Sess.Joint->bOverride_Type = true;

        Actuator = NewObject<UMjMotorActuator>(Sess.Robot, TEXT("TestActuator"));
        Actuator->TargetName = TEXT("TestJoint");
        Actuator->RegisterComponent();

        // ControlSource 0 = ZMQ (returns NetworkValue), 1 = UI (returns InternalValue).
        // Tests drive control via SetControl() which writes to InternalValue, so set UI source.
        Sess.Robot->ControlSource = 1;
    }))
    {
        AddError(FString::Printf(TEXT("FMjUESession::Init failed: %s"), *S.LastError));
        S.Cleanup();
        return false;
    }

    TestNotNull(TEXT("Actuator should not be null"), Actuator);
    if (!Actuator)
    {
        S.Cleanup();
        return false;
    }

    Actuator->SetControl(5.0f);

    TestTrue(TEXT("GetControl() should return approximately 5.0 after SetControl(5.0f)"),
        FMath::Abs(Actuator->GetControl() - 5.0f) < 0.001f);

    S.Cleanup();
    return true;
}

// ============================================================================
// URLab.Actuator.NuCountMatchesActuators
//   A single UMjMotorActuator should result in nu == 1 on the compiled model.
// ============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMjActuatorNuCountMatchesActuators,
    "URLab.Actuator.NuCountMatchesActuators",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMjActuatorNuCountMatchesActuators::RunTest(const FString& Parameters)
{
    UMjMotorActuator* Actuator = nullptr;

    FMjUESession S;
    if (!S.Init([&Actuator](FMjUESession& Sess)
    {
        Sess.Joint->Type = EMjJointType::Hinge;
        Sess.Joint->bOverride_Type = true;

        Actuator = NewObject<UMjMotorActuator>(Sess.Robot, TEXT("TestActuator"));
        Actuator->TargetName = TEXT("TestJoint");
        Actuator->RegisterComponent();
    }))
    {
        AddError(FString::Printf(TEXT("FMjUESession::Init failed: %s"), *S.LastError));
        S.Cleanup();
        return false;
    }

    TestEqual(TEXT("Manager->PhysicsEngine->m_model->nu should be 1 for one motor actuator"),
        (int)S.Manager->PhysicsEngine->m_model->nu, 1);

    S.Cleanup();
    return true;
}

// ============================================================================
// URLab.Actuator.MultipleActuators_AllBind
//   Two hinge joints, each with its own UMjMotorActuator.  Both actuator IDs
//   must be >= 0 after compilation and nu must equal 2.
// ============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMjActuatorMultipleActuatorsAllBind,
    "URLab.Actuator.MultipleActuators_AllBind",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMjActuatorMultipleActuatorsAllBind::RunTest(const FString& Parameters)
{
    UMjMotorActuator* Act1 = nullptr;
    UMjMotorActuator* Act2 = nullptr;

    FMjUESession S;
    if (!S.Init([&Act1, &Act2](FMjUESession& Sess)
    {
        // Configure the default joint as a hinge
        Sess.Joint->Type = EMjJointType::Hinge;
        Sess.Joint->bOverride_Type = true;

        // First actuator targets the default joint
        Act1 = NewObject<UMjMotorActuator>(Sess.Robot, TEXT("TestActuator1"));
        Act1->TargetName = TEXT("TestJoint");
        Act1->RegisterComponent();

        // Second body + joint hierarchy
        UMjBody* Body2 = NewObject<UMjBody>(Sess.Robot, TEXT("Body2"));
        Body2->RegisterComponent();
        Body2->AttachToComponent(Sess.Body, FAttachmentTransformRules::KeepRelativeTransform);

        UMjJoint* Joint2 = NewObject<UMjJoint>(Sess.Robot, TEXT("TestJoint2"));
        Joint2->Type = EMjJointType::Hinge;
        Joint2->bOverride_Type = true;
        Joint2->RegisterComponent();
        Joint2->AttachToComponent(Body2, FAttachmentTransformRules::KeepRelativeTransform);

        UMjGeom* Geom2 = NewObject<UMjGeom>(Sess.Robot, TEXT("Geom2"));
        Geom2->Size = FVector(0.1f, 0.1f, 0.1f);
        Geom2->bOverride_Size = true;
        Geom2->RegisterComponent();
        Geom2->AttachToComponent(Body2, FAttachmentTransformRules::KeepRelativeTransform);

        // Second actuator targets the second joint
        Act2 = NewObject<UMjMotorActuator>(Sess.Robot, TEXT("TestActuator2"));
        Act2->TargetName = TEXT("TestJoint2");
        Act2->RegisterComponent();
    }))
    {
        AddError(FString::Printf(TEXT("FMjUESession::Init failed: %s"), *S.LastError));
        S.Cleanup();
        return false;
    }

    TestNotNull(TEXT("Act1 should not be null"), Act1);
    TestNotNull(TEXT("Act2 should not be null"), Act2);

    if (Act1)
    {
        TestTrue(TEXT("Act1->m_ActuatorView.id should be >= 0"),
            Act1->m_ActuatorView.id >= 0);
    }
    if (Act2)
    {
        TestTrue(TEXT("Act2->m_ActuatorView.id should be >= 0"),
            Act2->m_ActuatorView.id >= 0);
    }

    TestEqual(TEXT("Manager->PhysicsEngine->m_model->nu should be 2"),
        (int)S.Manager->PhysicsEngine->m_model->nu, 2);

    S.Cleanup();
    return true;
}

// ============================================================================
// URLab.Actuator.MotorActuator_GetForce_NocrashAfterStep
//   Verify that calling GetForce() after stepping does not crash and returns
//   a finite floating-point value.
// ============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMjActuatorMotorActuatorGetForceNoCrashAfterStep,
    "URLab.Actuator.MotorActuator_GetForce_NocrashAfterStep",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMjActuatorMotorActuatorGetForceNoCrashAfterStep::RunTest(const FString& Parameters)
{
    UMjMotorActuator* Actuator = nullptr;

    FMjUESession S;
    if (!S.Init([&Actuator](FMjUESession& Sess)
    {
        Sess.Joint->Type = EMjJointType::Hinge;
        Sess.Joint->bOverride_Type = true;

        Actuator = NewObject<UMjMotorActuator>(Sess.Robot, TEXT("TestActuator"));
        Actuator->TargetName = TEXT("TestJoint");
        Actuator->RegisterComponent();
    }))
    {
        AddError(FString::Printf(TEXT("FMjUESession::Init failed: %s"), *S.LastError));
        S.Cleanup();
        return false;
    }

    TestNotNull(TEXT("Actuator should not be null"), Actuator);
    if (!Actuator)
    {
        S.Cleanup();
        return false;
    }

    S.Step(5);

    float Force = Actuator->GetForce();
    TestTrue(TEXT("GetForce() should return a finite value after 5 steps"),
        FMath::IsFinite(Force));

    S.Cleanup();
    return true;
}
