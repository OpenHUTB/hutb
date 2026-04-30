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
#include "MuJoCo/Components/Sensors/MjSensor.h"
#include "MuJoCo/Components/Sensors/MjJointPosSensor.h"
#include "MuJoCo/Components/Sensors/MjJointVelSensor.h"
#include "Engine/World.h"
#include "mujoco/mujoco.h"

// ============================================================================
// URLab.Sensor.JointPosSensor_Binds
//   Verify that a UMjJointPosSensor targeting a hinge joint receives a valid
//   sensor ID after compilation, and that nsensor == 1.
// ============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMjSensorJointPosSensorBinds,
    "URLab.Sensor.JointPosSensor_Binds",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMjSensorJointPosSensorBinds::RunTest(const FString& Parameters)
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
        AddError(FString::Printf(TEXT("FMjUESession::Init failed: %s"), *S.LastError));
        S.Cleanup();
        return false;
    }

    TestNotNull(TEXT("Sensor should not be null after Init"), Sensor);
    if (Sensor)
    {
        TestTrue(TEXT("Sensor->m_SensorView.id should be >= 0 after Bind()"),
            Sensor->m_SensorView.id >= 0);
    }

    TestEqual(TEXT("Manager->PhysicsEngine->m_model->nsensor should be 1"),
        (int)S.Manager->PhysicsEngine->m_model->nsensor, 1);

    S.Cleanup();
    return true;
}

// ============================================================================
// URLab.Sensor.JointPosSensor_GetReading
//   Set the hinge joint position to 1.5 rad, run mj_forward, and confirm
//   that GetReading() returns a value approximately equal to 1.5.
// ============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMjSensorJointPosSensorGetReading,
    "URLab.Sensor.JointPosSensor_GetReading",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMjSensorJointPosSensorGetReading::RunTest(const FString& Parameters)
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
        AddError(FString::Printf(TEXT("FMjUESession::Init failed: %s"), *S.LastError));
        S.Cleanup();
        return false;
    }

    TestNotNull(TEXT("Sensor should not be null"), Sensor);
    if (!Sensor)
    {
        S.Cleanup();
        return false;
    }

    // Drive the joint to 1.5 rad and propagate through kinematics
    S.Joint->SetPosition(1.5f);
    mj_forward(S.Manager->PhysicsEngine->m_model, S.Manager->PhysicsEngine->m_data);

    TArray<float> Reading = Sensor->GetReading();
    TestTrue(TEXT("GetReading() should return at least one element"), Reading.Num() > 0);

    if (Reading.Num() > 0)
    {
        TestTrue(TEXT("GetReading()[0] should be approximately 1.5"),
            FMath::Abs(Reading[0] - 1.5f) < 0.01f);
    }

    S.Cleanup();
    return true;
}

// ============================================================================
// URLab.Sensor.JointPosSensor_DimMatchesModel
//   Verify that the Dim reported by the sensor component matches the value
//   stored in sensor_dim[] of the compiled mjModel (should be 1 for jointpos).
// ============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMjSensorJointPosSensorDimMatchesModel,
    "URLab.Sensor.JointPosSensor_DimMatchesModel",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMjSensorJointPosSensorDimMatchesModel::RunTest(const FString& Parameters)
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
        AddError(FString::Printf(TEXT("FMjUESession::Init failed: %s"), *S.LastError));
        S.Cleanup();
        return false;
    }

    TestNotNull(TEXT("Sensor should not be null"), Sensor);
    if (Sensor && Sensor->m_SensorView.id >= 0)
    {
        int ModelDim = S.Manager->PhysicsEngine->m_model->sensor_dim[Sensor->m_SensorView.id];
        TestEqual(TEXT("Sensor->Dim should match model's sensor_dim for this sensor"),
            Sensor->Dim, ModelDim);
        TestEqual(TEXT("JointPos sensor Dim should be 1"), Sensor->Dim, 1);
    }

    S.Cleanup();
    return true;
}

// ============================================================================
// URLab.Sensor.MultipleSensors_AllBind
//   Two hinge joints, each with its own UMjJointPosSensor.  Both sensor IDs
//   must be >= 0 after compilation, and nsensor must equal 2.
// ============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMjSensorMultipleSensorsAllBind,
    "URLab.Sensor.MultipleSensors_AllBind",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMjSensorMultipleSensorsAllBind::RunTest(const FString& Parameters)
{
    UMjJointPosSensor* Sensor1 = nullptr;
    UMjJointPosSensor* Sensor2 = nullptr;

    FMjUESession S;
    if (!S.Init([&Sensor1, &Sensor2](FMjUESession& Sess)
    {
        // Configure the default joint as a hinge
        Sess.Joint->Type = EMjJointType::Hinge;
        Sess.Joint->bOverride_Type = true;

        // First sensor targets the default joint
        Sensor1 = NewObject<UMjJointPosSensor>(Sess.Robot, TEXT("TestSensor1"));
        Sensor1->TargetName = TEXT("TestJoint");
        Sensor1->RegisterComponent();

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

        // Second sensor targets the second joint
        Sensor2 = NewObject<UMjJointPosSensor>(Sess.Robot, TEXT("TestSensor2"));
        Sensor2->TargetName = TEXT("TestJoint2");
        Sensor2->RegisterComponent();
    }))
    {
        AddError(FString::Printf(TEXT("FMjUESession::Init failed: %s"), *S.LastError));
        S.Cleanup();
        return false;
    }

    TestNotNull(TEXT("Sensor1 should not be null"), Sensor1);
    TestNotNull(TEXT("Sensor2 should not be null"), Sensor2);

    if (Sensor1)
    {
        TestTrue(TEXT("Sensor1->m_SensorView.id should be >= 0"),
            Sensor1->m_SensorView.id >= 0);
    }
    if (Sensor2)
    {
        TestTrue(TEXT("Sensor2->m_SensorView.id should be >= 0"),
            Sensor2->m_SensorView.id >= 0);
    }

    TestEqual(TEXT("Manager->PhysicsEngine->m_model->nsensor should be 2"),
        (int)S.Manager->PhysicsEngine->m_model->nsensor, 2);

    S.Cleanup();
    return true;
}

// ============================================================================
// URLab.Sensor.JointVelSensor_Binds
//   Verify that a UMjJointVelSensor targeting a hinge joint receives a valid
//   sensor ID (>= 0) after compilation.
// ============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMjSensorJointVelSensorBinds,
    "URLab.Sensor.JointVelSensor_Binds",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMjSensorJointVelSensorBinds::RunTest(const FString& Parameters)
{
    UMjJointVelSensor* Sensor = nullptr;

    FMjUESession S;
    if (!S.Init([&Sensor](FMjUESession& Sess)
    {
        Sess.Joint->Type = EMjJointType::Hinge;
        Sess.Joint->bOverride_Type = true;

        Sensor = NewObject<UMjJointVelSensor>(Sess.Robot, TEXT("TestVelSensor"));
        Sensor->TargetName = TEXT("TestJoint");
        Sensor->RegisterComponent();
    }))
    {
        AddError(FString::Printf(TEXT("FMjUESession::Init failed: %s"), *S.LastError));
        S.Cleanup();
        return false;
    }

    TestNotNull(TEXT("Sensor should not be null after Init"), Sensor);
    if (Sensor)
    {
        TestTrue(TEXT("Sensor->m_SensorView.id should be >= 0 after Bind()"),
            Sensor->m_SensorView.id >= 0);
    }

    S.Cleanup();
    return true;
}

// ============================================================================
// URLab.Sensor.SensorViewModelPointer
//   After compilation the sensor's internal SensorView._m pointer must equal
//   the Manager's compiled mjModel pointer, confirming Bind() was called with
//   the correct model.
// ============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMjSensorSensorViewModelPointer,
    "URLab.Sensor.SensorViewModelPointer",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMjSensorSensorViewModelPointer::RunTest(const FString& Parameters)
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
        AddError(FString::Printf(TEXT("FMjUESession::Init failed: %s"), *S.LastError));
        S.Cleanup();
        return false;
    }

    TestNotNull(TEXT("Sensor should not be null"), Sensor);
    if (Sensor)
    {
        TestTrue(TEXT("Sensor->m_SensorView._m should point to Manager->PhysicsEngine->m_model"),
            Sensor->m_SensorView._m == S.Manager->PhysicsEngine->m_model);
    }

    S.Cleanup();
    return true;
}
