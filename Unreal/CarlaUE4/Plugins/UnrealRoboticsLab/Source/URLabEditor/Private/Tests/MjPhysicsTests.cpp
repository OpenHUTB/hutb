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
#include "MuJoCo/Components/Geometry/MjGeom.h"
#include "MuJoCo/Components/Joints/MjJoint.h"
#include "MuJoCo/Components/Bodies/MjBody.h"
#include "Engine/World.h"
#include "mujoco/mujoco.h"

// ============================================================================
// URLab.Physics.ManagerInitializesCorrectly
//   Default Init() should leave the Manager in an initialized and running state.
// ============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMjPhysicsManagerInitializesCorrectly,
    "URLab.Physics.ManagerInitializesCorrectly",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMjPhysicsManagerInitializesCorrectly::RunTest(const FString& Parameters)
{
    FMjUESession S;
    if (!S.Init())
    {
        AddError(S.LastError);
        return false;
    }

    TestTrue(TEXT("Manager->IsInitialized() should be true"), S.Manager->IsInitialized());
    // Sim starts paused by design — check initialized, not running.
    TestTrue(TEXT("PhysicsEngine->IsInitialized()"), S.Manager->PhysicsEngine->IsInitialized());

    S.Cleanup();
    return true;
}

// ============================================================================
// URLab.Physics.ModelPointersValid
//   After default Init() the model and data pointers must be non-null and the
//   model must contain at least one body (the world body).
// ============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMjPhysicsModelPointersValid,
    "URLab.Physics.ModelPointersValid",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMjPhysicsModelPointersValid::RunTest(const FString& Parameters)
{
    FMjUESession S;
    if (!S.Init())
    {
        AddError(S.LastError);
        return false;
    }

    TestTrue(TEXT("Manager->PhysicsEngine->m_model should not be nullptr"), S.Manager->PhysicsEngine->m_model != nullptr);
    TestTrue(TEXT("Manager->PhysicsEngine->m_data should not be nullptr"),  S.Manager->PhysicsEngine->m_data  != nullptr);

    if (S.Manager->PhysicsEngine->m_model)
    {
        TestTrue(TEXT("m_model->nbody should be >= 1"), S.Manager->PhysicsEngine->m_model->nbody >= 1);
    }

    S.Cleanup();
    return true;
}

// ============================================================================
// URLab.Physics.TimeAdvancesAfterStep
//   Stepping 10 times must advance Manager->PhysicsEngine->m_data->time beyond zero.
// ============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMjPhysicsTimeAdvancesAfterStep,
    "URLab.Physics.TimeAdvancesAfterStep",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMjPhysicsTimeAdvancesAfterStep::RunTest(const FString& Parameters)
{
    FMjUESession S;
    if (!S.Init())
    {
        AddError(S.LastError);
        return false;
    }

    S.Step(10);

    TestTrue(TEXT("Manager->PhysicsEngine->m_data->time should be > 0.0 after 10 steps"),
        S.Manager->PhysicsEngine->m_data->time > 0.0);

    S.Cleanup();
    return true;
}

// ============================================================================
// URLab.Physics.GravityApplied
//   Default MuJoCo gravity should be negative on the Z axis (~-9.81 m/s²),
//   confirming that ApplyOptions() is called during compilation.
// ============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMjPhysicsGravityApplied,
    "URLab.Physics.GravityApplied",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMjPhysicsGravityApplied::RunTest(const FString& Parameters)
{
    FMjUESession S;
    if (!S.Init())
    {
        AddError(S.LastError);
        return false;
    }

    TestTrue(TEXT("m_model->opt.gravity[2] should be < 0.0 (default MuJoCo gravity is -9.81 m/s²)"),
        S.Manager->PhysicsEngine->m_model->opt.gravity[2] < 0.0);

    S.Cleanup();
    return true;
}

// ============================================================================
// URLab.Physics.GeomViewBoundToModel
//   The GeomView's internal model pointer must match Manager->PhysicsEngine->m_model,
//   confirming that Bind() was called with the correct model after compilation.
// ============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMjPhysicsGeomViewBoundToModel,
    "URLab.Physics.GeomViewBoundToModel",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMjPhysicsGeomViewBoundToModel::RunTest(const FString& Parameters)
{
    FMjUESession S;
    if (!S.Init())
    {
        AddError(S.LastError);
        return false;
    }

    TestTrue(TEXT("Geom->m_GeomView._m should equal Manager->PhysicsEngine->m_model"),
        S.Geom->m_GeomView._m == S.Manager->PhysicsEngine->m_model);

    S.Cleanup();
    return true;
}

// ============================================================================
// URLab.Physics.JointSetAndGetPosition
//   Writing a position via SetPosition() and running mj_forward() should
//   produce the same value back from GetPosition() within floating-point
//   tolerance, verifying the plugin's joint position read/write API.
// ============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMjPhysicsJointSetAndGetPosition,
    "URLab.Physics.JointSetAndGetPosition",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMjPhysicsJointSetAndGetPosition::RunTest(const FString& Parameters)
{
    FMjUESession S;
    if (!S.Init([](FMjUESession& S)
    {
        S.Joint->Type = EMjJointType::Hinge;
        S.Joint->bOverride_Type = true;
    }))
    {
        AddError(S.LastError);
        return false;
    }

    S.Joint->SetPosition(1.2f);
    mj_forward(S.Manager->PhysicsEngine->m_model, S.Manager->PhysicsEngine->m_data);

    TestTrue(TEXT("Joint->GetPosition() should equal 1.2f within 0.001"),
        FMath::Abs(S.Joint->GetPosition() - 1.2f) < 0.001f);

    S.Cleanup();
    return true;
}

// ============================================================================
// URLab.Physics.JointGetPositionAfterStep
//   After stepping a hinge joint model 5 times, GetPosition() must return a
//   finite value (no NaN or Inf).
// ============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMjPhysicsJointGetPositionAfterStep,
    "URLab.Physics.JointGetPositionAfterStep",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMjPhysicsJointGetPositionAfterStep::RunTest(const FString& Parameters)
{
    FMjUESession S;
    if (!S.Init([](FMjUESession& S)
    {
        S.Joint->Type = EMjJointType::Hinge;
        S.Joint->bOverride_Type = true;
    }))
    {
        AddError(S.LastError);
        return false;
    }

    S.Step(5);

    TestTrue(TEXT("Joint->GetPosition() should be a finite value after 5 steps"),
        FMath::IsFinite(S.Joint->GetPosition()));

    S.Cleanup();
    return true;
}

// ============================================================================
// URLab.Physics.ResetResetsTime
//   After stepping and calling mj_resetData(), the simulation time must return
//   to zero.
// ============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMjPhysicsResetResetsTime,
    "URLab.Physics.ResetResetsTime",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMjPhysicsResetResetsTime::RunTest(const FString& Parameters)
{
    FMjUESession S;
    if (!S.Init())
    {
        AddError(S.LastError);
        return false;
    }

    S.Step(20);
    TestTrue(TEXT("Manager->PhysicsEngine->m_data->time should be > 0.0 after 20 steps"),
        S.Manager->PhysicsEngine->m_data->time > 0.0);

    mj_resetData(S.Manager->PhysicsEngine->m_model, S.Manager->PhysicsEngine->m_data);

    TestTrue(TEXT("Manager->PhysicsEngine->m_data->time should be ~0.0 after mj_resetData()"),
        FMath::Abs(S.Manager->PhysicsEngine->m_data->time) < 1e-10);

    S.Cleanup();
    return true;
}

// ============================================================================
// URLab.Physics.GravityScaleIntegration
//   ApplyOptions() must divide UE cm/s² by 100 before writing to mjOpt.
//   Set Gravity=(0,0,-980) cm/s² → m_model->opt.gravity[2] must ≈ -9.80 m/s².
//   Verifies Fix 3.6.
// ============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMjPhysicsGravityScaleIntegration,
    "URLab.Physics.GravityScaleIntegration",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMjPhysicsGravityScaleIntegration::RunTest(const FString& Parameters)
{
    FMjUESession S;
    if (!S.Init([](FMjUESession& Sess)
    {
        Sess.Manager->PhysicsEngine->Options.Gravity = FVector(0.0f, 0.0f, -980.0f); // 9.80 m/s² earth gravity
    }))
    {
        AddError(S.LastError);
        return false;
    }

    TestTrue(TEXT("gravity X ≈ 0"),
        FMath::Abs((float)S.Manager->PhysicsEngine->m_model->opt.gravity[0]) < 1e-5f);
    TestTrue(TEXT("gravity Y ≈ 0"),
        FMath::Abs((float)S.Manager->PhysicsEngine->m_model->opt.gravity[1]) < 1e-5f);
    TestTrue(TEXT("gravity Z ≈ -9.80 m/s² (divided by 100 from -980 cm/s²)"),
        FMath::Abs((float)(S.Manager->PhysicsEngine->m_model->opt.gravity[2] + 9.80f)) < 0.05f);

    S.Cleanup();
    return true;
}

// ============================================================================
// URLab.Physics.GravityYNegate_Integration
//   Y component of gravity must be negated (UE→MuJoCo handedness flip).
//   Set Gravity=(0,100,0) cm/s² → m_model->opt.gravity[1] must ≈ -1.0 m/s².
//   Verifies Fix 3.6 Y-negate path.
// ============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMjPhysicsGravityYNegate_Integration,
    "URLab.Physics.GravityYNegate_Integration",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMjPhysicsGravityYNegate_Integration::RunTest(const FString& Parameters)
{
    FMjUESession S;
    if (!S.Init([](FMjUESession& Sess)
    {
        Sess.Manager->PhysicsEngine->Options.bOverride_Gravity = true;
        Sess.Manager->PhysicsEngine->Options.Gravity = FVector(0.0f, 100.0f, 0.0f); // 1 m/s² along UE +Y
    }))
    {
        AddError(S.LastError);
        return false;
    }

    // UE +Y → MuJoCo -Y (handedness flip)
    TestTrue(TEXT("gravity Y ≈ -1.0 m/s² (negated from +100 cm/s²)"),
        FMath::Abs((float)(S.Manager->PhysicsEngine->m_model->opt.gravity[1] + 1.0f)) < 0.05f);

    S.Cleanup();
    return true;
}

// ============================================================================
// URLab.Physics.JointAxis_Compile
//   UMjJoint with Axis=(0,1,0) must export MuJoCo jnt_axis (0,-1,0).
//   Verifies Fix 3.5: axis is treated as a direction vector with Y-negate.
// ============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMjPhysicsJointAxis_Compile,
    "URLab.Physics.JointAxis_Compile",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMjPhysicsJointAxis_Compile::RunTest(const FString& Parameters)
{
    FMjUESession S;
    if (!S.Init([](FMjUESession& Sess)
    {
        Sess.Joint->Type          = EMjJointType::Hinge;
        Sess.Joint->bOverride_Type = true;
        Sess.Joint->Axis          = FVector(0.0f, 1.0f, 0.0f);
        Sess.Joint->bOverride_Axis = true;
    }))
    {
        AddError(S.LastError);
        return false;
    }

    // Locate the hinge joint by type — name lookup is fragile due to UE component naming.
    // The session has exactly one user joint. Find it by iterating jnt_type[].
    int jid = -1;
    for (int j = 0; j < S.Manager->PhysicsEngine->m_model->njnt; ++j)
    {
        if (S.Manager->PhysicsEngine->m_model->jnt_type[j] == mjJNT_HINGE)
        {
            jid = j;
            break;
        }
    }
    TestTrue(TEXT("hinge joint found in compiled model"), jid >= 0);
    if (jid >= 0)
    {
        const mjtNum* ax = &S.Manager->PhysicsEngine->m_model->jnt_axis[3 * jid];
        TestTrue(TEXT("MJ jnt_axis X ≈ 0"),  FMath::Abs((float)ax[0]) < 1e-4f);
        TestTrue(TEXT("MJ jnt_axis Y ≈ -1"), FMath::Abs((float)(ax[1] + 1.0f)) < 1e-4f);
        TestTrue(TEXT("MJ jnt_axis Z ≈ 0"),  FMath::Abs((float)ax[2]) < 1e-4f);
    }

    S.Cleanup();
    return true;
}

// ============================================================================
// Sleep tests (S.8)
// ============================================================================

// ============================================================================
// URLab.Physics.Sleep_EnableFlagSet
//   When Options.bEnableSleep is true, ApplyOptions() must set mjENBL_SLEEP.
// ============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMjPhysicsSleep_EnableFlagSet,
    "URLab.Physics.Sleep_EnableFlagSet",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMjPhysicsSleep_EnableFlagSet::RunTest(const FString& Parameters)
{
    FMjUESession S;
    if (!S.Init([](FMjUESession& Sess)
    {
        Sess.Manager->PhysicsEngine->Options.bEnableSleep   = true;
        Sess.Manager->PhysicsEngine->Options.SleepTolerance = 1e-3f;
    }))
    {
        AddError(S.LastError);
        return false;
    }

    constexpr int MJ_ENBL_SLEEP = 1 << 5;
    TestTrue(TEXT("mjENBL_SLEEP bit set when bEnableSleep=true"),
        (S.Manager->PhysicsEngine->m_model->opt.enableflags & MJ_ENBL_SLEEP) != 0);
    TestTrue(TEXT("sleep_tolerance matches Options.SleepTolerance"),
        FMath::Abs((float)S.Manager->PhysicsEngine->m_model->opt.sleep_tolerance - 1e-3f) < 1e-6f);

    S.Cleanup();
    return true;
}

// ============================================================================
// URLab.Physics.Sleep_DisableFlagClear
//   When Options.bEnableSleep is false (default), mjENBL_SLEEP must NOT be set.
// ============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMjPhysicsSleep_DisableFlagClear,
    "URLab.Physics.Sleep_DisableFlagClear",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMjPhysicsSleep_DisableFlagClear::RunTest(const FString& Parameters)
{
    FMjUESession S;
    if (!S.Init())
    {
        AddError(S.LastError);
        return false;
    }

    constexpr int MJ_ENBL_SLEEP = 1 << 5;
    TestTrue(TEXT("mjENBL_SLEEP NOT set when bEnableSleep=false (default)"),
        (S.Manager->PhysicsEngine->m_model->opt.enableflags & MJ_ENBL_SLEEP) == 0);

    S.Cleanup();
    return true;
}

// ============================================================================
// URLab.Physics.Sleep_BodyIsAwakeByDefault
//   With sleep enabled, bodies must start awake (body_awake == 1).
// ============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMjPhysicsSleep_BodyIsAwakeByDefault,
    "URLab.Physics.Sleep_BodyIsAwakeByDefault",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMjPhysicsSleep_BodyIsAwakeByDefault::RunTest(const FString& Parameters)
{
    FMjUESession S;
    if (!S.Init([](FMjUESession& Sess)
    {
        Sess.Manager->PhysicsEngine->Options.bEnableSleep = true;
    }))
    {
        AddError(S.LastError);
        return false;
    }

    int BodyId = S.Body->GetMj().id;
    TestTrue(TEXT("Body ID is valid"), BodyId > 0);
    if (BodyId > 0)
    {
        TestTrue(TEXT("body_awake[BodyId] == 1 at init"),
            S.Manager->PhysicsEngine->m_data->body_awake[BodyId] == 1);
    }

    S.Cleanup();
    return true;
}

// ============================================================================
// URLab.Physics.Sleep_IsAwakeReturnsTrue
//   UMjBody::IsAwake() must return true right after compilation.
// ============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMjPhysicsSleep_IsAwakeReturnsTrue,
    "URLab.Physics.Sleep_IsAwakeReturnsTrue",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMjPhysicsSleep_IsAwakeReturnsTrue::RunTest(const FString& Parameters)
{
    FMjUESession S;
    if (!S.Init([](FMjUESession& Sess)
    {
        Sess.Manager->PhysicsEngine->Options.bEnableSleep = true;
    }))
    {
        AddError(S.LastError);
        return false;
    }

    TestTrue(TEXT("Body->IsAwake() returns true after init"), S.Body->IsAwake());

    S.Cleanup();
    return true;
}

// ============================================================================
// URLab.Physics.Sleep_BodySleepPolicyAllowed
//   SleepPolicy=Allowed on UMjBody must compile to mjSLEEP_ALLOWED (4) in
//   the model's tree_sleep_policy for that body's tree.
// ============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMjPhysicsSleep_BodySleepPolicyAllowed,
    "URLab.Physics.Sleep_BodySleepPolicyAllowed",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMjPhysicsSleep_BodySleepPolicyAllowed::RunTest(const FString& Parameters)
{
    FMjUESession S;
    if (!S.Init([](FMjUESession& Sess)
    {
        Sess.Manager->PhysicsEngine->Options.bEnableSleep = true;
        Sess.Body->SleepPolicy = EMjBodySleepPolicy::Allowed;
    }))
    {
        AddError(S.LastError);
        return false;
    }

    int BodyId = S.Body->GetMj().id;
    TestTrue(TEXT("Body ID valid"), BodyId > 0);
    if (BodyId > 0)
    {
        int TreeId = S.Manager->PhysicsEngine->m_model->body_treeid[BodyId];
        TestTrue(TEXT("TreeId valid"), TreeId >= 0);
        if (TreeId >= 0)
        {
            // mjSLEEP_ALLOWED = 4
            TestTrue(TEXT("tree_sleep_policy == mjSLEEP_ALLOWED (4)"),
                S.Manager->PhysicsEngine->m_model->tree_sleep_policy[TreeId] == 4);
        }
    }

    S.Cleanup();
    return true;
}

// ============================================================================
// URLab.Physics.Sleep_WakeBodyRestoresAwakeState
//   After manually putting a body to sleep, Wake() must restore body and tree.
// ============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMjPhysicsSleep_WakeBodyRestoresAwakeState,
    "URLab.Physics.Sleep_WakeBodyRestoresAwakeState",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMjPhysicsSleep_WakeBodyRestoresAwakeState::RunTest(const FString& Parameters)
{
    FMjUESession S;
    if (!S.Init([](FMjUESession& Sess)
    {
        Sess.Manager->PhysicsEngine->Options.bEnableSleep = true;
    }))
    {
        AddError(S.LastError);
        return false;
    }

    int BodyId = S.Body->GetMj().id;
    TestTrue(TEXT("Body ID valid"), BodyId > 0);
    if (BodyId > 0)
    {
        int TreeId = S.Manager->PhysicsEngine->m_model->body_treeid[BodyId];

        // Manually force to sleep
        S.Manager->PhysicsEngine->m_data->body_awake[BodyId] = 0;
        if (TreeId >= 0)
        {
            S.Manager->PhysicsEngine->m_data->tree_asleep[TreeId] = 0;
            S.Manager->PhysicsEngine->m_data->tree_awake[TreeId]  = 0;
        }
        TestTrue(TEXT("Body is asleep after manual set"), !S.Body->IsAwake());

        // Wake via component API
        S.Body->Wake();

        TestTrue(TEXT("Body->IsAwake() true after Wake()"), S.Body->IsAwake());
        if (TreeId >= 0)
        {
            TestTrue(TEXT("tree_awake == 1 after Wake()"),
                S.Manager->PhysicsEngine->m_data->tree_awake[TreeId] == 1);
        }
    }

    S.Cleanup();
    return true;
}

// ============================================================================
// URLab.Physics.Sleep_ForceSleepBody
//   Body->Sleep() must put the body to sleep; Wake() must restore it.
// ============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMjPhysicsSleep_ForceSleepBody,
    "URLab.Physics.Sleep_ForceSleepBody",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMjPhysicsSleep_ForceSleepBody::RunTest(const FString& Parameters)
{
    FMjUESession S;
    if (!S.Init([](FMjUESession& Sess)
    {
        Sess.Manager->PhysicsEngine->Options.bEnableSleep = true;
    }))
    {
        AddError(S.LastError);
        return false;
    }

    TestTrue(TEXT("Body starts awake"), S.Body->IsAwake());
    S.Body->Sleep();
    TestTrue(TEXT("Body->IsAwake() false after Sleep()"), !S.Body->IsAwake());
    S.Body->Wake();
    TestTrue(TEXT("Body->IsAwake() true after Wake()"), S.Body->IsAwake());

    S.Cleanup();
    return true;
}
