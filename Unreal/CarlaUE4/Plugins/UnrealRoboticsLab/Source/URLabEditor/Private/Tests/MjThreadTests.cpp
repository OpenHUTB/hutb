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
#include "Engine/World.h"
#include "mujoco/mujoco.h"

// ============================================================================
// URLab.Thread.PIERestart
//   Verifies the system can fully reinitialize after a shutdown cycle
// ============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMjThreadPIERestart,
    "URLab.Thread.PIERestart",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMjThreadPIERestart::RunTest(const FString& Parameters)
{
    // First session
    {
        FMjUESession S;
        if (!S.Init())
        {
            AddError(FString::Printf(TEXT("First session Init() failed: %s"), *S.LastError));
            return false;
        }
        TestTrue(TEXT("First session: Manager should be initialized"), S.Manager->IsInitialized());
        S.Cleanup();
    }

    // Second session — verifies reinitalization after full shutdown
    {
        FMjUESession S2;
        if (!S2.Init())
        {
            AddError(FString::Printf(TEXT("Second session Init() failed: %s"), *S2.LastError));
            return false;
        }
        TestTrue(TEXT("Second session: Manager should be initialized after restart"),
            S2.Manager->IsInitialized());
        S2.Cleanup();
    }

    return true;
}

// ============================================================================
// URLab.Thread.ConcurrentRead
//   Smoke test: step 200 times and verify xpos values remain finite
// ============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMjThreadConcurrentRead,
    "URLab.Thread.ConcurrentRead",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMjThreadConcurrentRead::RunTest(const FString& Parameters)
{
    FMjUESession S;
    if (!S.Init())
    {
        AddError(FString::Printf(TEXT("Init() failed: %s"), *S.LastError));
        return false;
    }

    // Step 200 times from the test thread while spot-checking xpos after each batch
    const int TotalSteps   = 200;
    const int BatchSize    = 50;
    bool      bAllFinite   = true;

    for (int Batch = 0; Batch < TotalSteps / BatchSize; ++Batch)
    {
        S.Step(BatchSize);

        // Read body positions; nv >= 1 because the world body always exists
        if (S.Manager->PhysicsEngine->m_data && S.Manager->PhysicsEngine->m_model)
        {
            const int NBody = S.Manager->PhysicsEngine->m_model->nbody;
            for (int i = 0; i < NBody; ++i)
            {
                const double* XPos = &S.Manager->PhysicsEngine->m_data->xpos[i * 3];
                if (!FMath::IsFinite((float)XPos[0]) ||
                    !FMath::IsFinite((float)XPos[1]) ||
                    !FMath::IsFinite((float)XPos[2]))
                {
                    bAllFinite = false;
                    AddError(FString::Printf(
                        TEXT("xpos for body %d is non-finite after %d steps"),
                        i, (Batch + 1) * BatchSize));
                    break;
                }
            }
        }

        if (!bAllFinite) break;
    }

    TestTrue(TEXT("All xpos values should remain finite after 200 steps"), bAllFinite);

    S.Cleanup();
    return true;
}

// ============================================================================
// URLab.Thread.PauseResume
//   Verify that pausing and resuming leaves the manager in a running state
// ============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMjThreadPauseResume,
    "URLab.Thread.PauseResume",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMjThreadPauseResume::RunTest(const FString& Parameters)
{
    FMjUESession S;
    if (!S.Init())
    {
        AddError(FString::Printf(TEXT("Init() failed: %s"), *S.LastError));
        return false;
    }

    // Pause
    S.Manager->PhysicsEngine->bIsPaused = true;

    // Direct steps still execute; the async loop would honour the flag
    S.Step(10);

    // Resume
    S.Manager->PhysicsEngine->bIsPaused = false;

    TestTrue(TEXT("Manager should be running after unpause"), S.Manager->IsRunning());
    TestTrue(TEXT("Manager should be initialized after unpause"), S.Manager->IsInitialized());

    S.Cleanup();
    return true;
}

// ============================================================================
// URLab.Thread.ModelIntegrity
//   Verify model and data pointers are valid and that time advances after stepping
// ============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMjThreadModelIntegrity,
    "URLab.Thread.ModelIntegrity",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMjThreadModelIntegrity::RunTest(const FString& Parameters)
{
    FMjUESession S;
    if (!S.Init())
    {
        AddError(FString::Printf(TEXT("Init() failed: %s"), *S.LastError));
        return false;
    }

    TestTrue(TEXT("Manager->PhysicsEngine->m_model should not be null"), S.Manager->PhysicsEngine->m_model != nullptr);
    TestTrue(TEXT("Manager->PhysicsEngine->m_data should not be null"),  S.Manager->PhysicsEngine->m_data  != nullptr);

    if (S.Manager->PhysicsEngine->m_model && S.Manager->PhysicsEngine->m_data)
    {
        TestTrue(TEXT("m_model->nq should be >= 0"), S.Manager->PhysicsEngine->m_model->nq >= 0);
        TestTrue(TEXT("m_data->time should be >= 0.0 before stepping"),
            S.Manager->PhysicsEngine->m_data->time >= 0.0);

        S.Step(10);

        TestTrue(TEXT("m_data->time should be > 0.0 after 10 steps"),
            S.Manager->PhysicsEngine->m_data->time > 0.0);
    }

    S.Cleanup();
    return true;
}
