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
#include "MuJoCo/Components/Deformable/MjFlexcomp.h"
#include "mujoco/mujoco.h"

// ============================================================================
// URLab.Flexcomp.Grid2D_Compiles
//   A 2D grid flexcomp should compile and produce a flex in the model.
// ============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMjFlexcompGrid2DCompiles,
    "URLab.Flexcomp.Grid2D_Compiles",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMjFlexcompGrid2DCompiles::RunTest(const FString& Parameters)
{
    FMjUESession S;
    bool bOk = S.Init([](FMjUESession& Session)
    {
        UMjFlexcomp* Flex = NewObject<UMjFlexcomp>(Session.Robot, TEXT("TestFlex"));
        Flex->Type = EMjFlexcompType::Grid;
        Flex->Dim = 2;
        Flex->Count = FIntVector(4, 4, 1);
        Flex->Spacing = FVector(0.05, 0.05, 0.05);
        Flex->Mass = 0.5f;
        Flex->Radius = 0.005f;
        Flex->MjName = TEXT("testgrid");
        Flex->RegisterComponent();
        Flex->AttachToComponent(Session.Body, FAttachmentTransformRules::KeepRelativeTransform);
    });

    if (!bOk)
    {
        AddError(FString::Printf(TEXT("Init failed: %s"), *S.LastError));
        S.Cleanup();
        return false;
    }

    mjModel* M = S.Manager->PhysicsEngine->m_model;
    TestTrue(TEXT("Model should have at least 1 flex"), M->nflex >= 1);

    S.Cleanup();
    return true;
}

// ============================================================================
// URLab.Flexcomp.Grid1D_Compiles
//   A 1D grid (rope) should compile.
// ============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMjFlexcompGrid1DCompiles,
    "URLab.Flexcomp.Grid1D_Compiles",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMjFlexcompGrid1DCompiles::RunTest(const FString& Parameters)
{
    FMjUESession S;
    bool bOk = S.Init([](FMjUESession& Session)
    {
        UMjFlexcomp* Flex = NewObject<UMjFlexcomp>(Session.Robot, TEXT("TestRope"));
        Flex->Type = EMjFlexcompType::Grid;
        Flex->Dim = 1;
        Flex->Count = FIntVector(8, 1, 1);
        Flex->Spacing = FVector(0.1, 0.1, 0.1);
        Flex->Mass = 0.2f;
        Flex->Radius = 0.01f;
        Flex->MjName = TEXT("testrope");
        Flex->RegisterComponent();
        Flex->AttachToComponent(Session.Body, FAttachmentTransformRules::KeepRelativeTransform);
    });

    if (!bOk)
    {
        AddError(FString::Printf(TEXT("Init failed: %s"), *S.LastError));
        S.Cleanup();
        return false;
    }

    mjModel* M = S.Manager->PhysicsEngine->m_model;
    TestTrue(TEXT("Model should have at least 1 flex"), M->nflex >= 1);
    TestTrue(TEXT("Flex dim should be 1"), M->flex_dim[0] == 1);

    S.Cleanup();
    return true;
}

// ============================================================================
// URLab.Flexcomp.Grid3D_Compiles
//   A 3D grid (volumetric) should compile.
// ============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMjFlexcompGrid3DCompiles,
    "URLab.Flexcomp.Grid3D_Compiles",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMjFlexcompGrid3DCompiles::RunTest(const FString& Parameters)
{
    FMjUESession S;
    bool bOk = S.Init([](FMjUESession& Session)
    {
        UMjFlexcomp* Flex = NewObject<UMjFlexcomp>(Session.Robot, TEXT("TestVol"));
        Flex->Type = EMjFlexcompType::Grid;
        Flex->Dim = 3;
        Flex->Count = FIntVector(3, 3, 3);
        Flex->Spacing = FVector(0.05, 0.05, 0.05);
        Flex->Mass = 1.0f;
        Flex->Radius = 0.005f;
        Flex->MjName = TEXT("testvol");
        Flex->RegisterComponent();
        Flex->AttachToComponent(Session.Body, FAttachmentTransformRules::KeepRelativeTransform);
    });

    if (!bOk)
    {
        AddError(FString::Printf(TEXT("Init failed: %s"), *S.LastError));
        S.Cleanup();
        return false;
    }

    mjModel* M = S.Manager->PhysicsEngine->m_model;
    TestTrue(TEXT("Model should have at least 1 flex"), M->nflex >= 1);
    TestTrue(TEXT("Flex dim should be 3"), M->flex_dim[0] == 3);

    S.Cleanup();
    return true;
}

// ============================================================================
// URLab.Flexcomp.Grid2D_CorrectBodyCount
//   4x4 grid = 16 vertices = 16 child bodies (all unpinned).
// ============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMjFlexcompGrid2DBodyCount,
    "URLab.Flexcomp.Grid2D_CorrectBodyCount",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMjFlexcompGrid2DBodyCount::RunTest(const FString& Parameters)
{
    FMjUESession S;
    bool bOk = S.Init([](FMjUESession& Session)
    {
        UMjFlexcomp* Flex = NewObject<UMjFlexcomp>(Session.Robot, TEXT("TestFlex"));
        Flex->Type = EMjFlexcompType::Grid;
        Flex->Dim = 2;
        Flex->Count = FIntVector(4, 4, 1);
        Flex->Spacing = FVector(0.05, 0.05, 0.05);
        Flex->Mass = 0.5f;
        Flex->Radius = 0.005f;
        Flex->MjName = TEXT("counttest");
        Flex->RegisterComponent();
        Flex->AttachToComponent(Session.Body, FAttachmentTransformRules::KeepRelativeTransform);
    });

    if (!bOk)
    {
        AddError(FString::Printf(TEXT("Init failed: %s"), *S.LastError));
        S.Cleanup();
        return false;
    }

    mjModel* M = S.Manager->PhysicsEngine->m_model;
    // 4x4 grid = 16 flex vertices
    TestTrue(TEXT("Should have at least 1 flex"), M->nflex >= 1);
    TestEqual(TEXT("Flex should have 16 vertices"), (int32)M->flex_vertnum[0], 16);

    S.Cleanup();
    return true;
}

// ============================================================================
// URLab.Flexcomp.PinnedVertices
//   Pinning vertices should reduce the number of created bodies.
// ============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMjFlexcompPinnedVertices,
    "URLab.Flexcomp.PinnedVertices",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMjFlexcompPinnedVertices::RunTest(const FString& Parameters)
{
    FMjUESession S;
    bool bOk = S.Init([](FMjUESession& Session)
    {
        UMjFlexcomp* Flex = NewObject<UMjFlexcomp>(Session.Robot, TEXT("TestFlex"));
        Flex->Type = EMjFlexcompType::Grid;
        Flex->Dim = 1;
        Flex->Count = FIntVector(5, 1, 1);
        Flex->Spacing = FVector(0.1, 0.1, 0.1);
        Flex->Mass = 0.2f;
        Flex->Radius = 0.01f;
        Flex->MjName = TEXT("pintest");
        Flex->PinIds = {0, 4}; // Pin first and last
        Flex->RegisterComponent();
        Flex->AttachToComponent(Session.Body, FAttachmentTransformRules::KeepRelativeTransform);
    });

    if (!bOk)
    {
        AddError(FString::Printf(TEXT("Init failed: %s"), *S.LastError));
        S.Cleanup();
        return false;
    }

    mjModel* M = S.Manager->PhysicsEngine->m_model;
    TestTrue(TEXT("Should have at least 1 flex"), M->nflex >= 1);
    // 5 vertices in the flex regardless of pinning
    TestEqual(TEXT("Flex should have 5 vertices"), (int32)M->flex_vertnum[0], 5);

    S.Cleanup();
    return true;
}

// ============================================================================
// URLab.Flexcomp.Import_Grid2D
//   Importing a flexcomp XML should create a UMjFlexcomp template.
// ============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMjFlexcompImportGrid2D,
    "URLab.Flexcomp.Import_Grid2D",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMjFlexcompImportGrid2D::RunTest(const FString& Parameters)
{
    FMjXmlImportSession S;
    bool bOk = S.Init(TEXT(
        "<mujoco>"
        "  <worldbody>"
        "    <body name=\"parent\">"
        "      <geom size=\"1\"/>"
        "      <flexcomp name=\"myflex\" type=\"grid\" dim=\"2\" count=\"3 3 1\" spacing=\"0.1 0.1 0.1\" mass=\"0.5\" radius=\"0.01\">"
        "        <elasticity young=\"100\" damping=\"0.5\"/>"
        "        <pin id=\"0 1 2\"/>"
        "      </flexcomp>"
        "    </body>"
        "  </worldbody>"
        "</mujoco>"));

    if (!bOk)
    {
        AddError(FString::Printf(TEXT("Import Init failed: %s"), *S.LastError));
        S.Cleanup();
        return false;
    }

    UMjFlexcomp* Flex = S.FindTemplate<UMjFlexcomp>(TEXT("myflex"));
    TestNotNull(TEXT("Should find flexcomp template"), Flex);

    if (Flex)
    {
        TestEqual(TEXT("Type should be Grid"), Flex->Type, EMjFlexcompType::Grid);
        TestEqual(TEXT("Dim should be 2"), Flex->Dim, 2);
        TestEqual(TEXT("Count.X should be 3"), Flex->Count.X, 3);
        TestEqual(TEXT("Count.Y should be 3"), Flex->Count.Y, 3);
        TestTrue(TEXT("Young should be 100"), MjTestMath::NearlyEqual(Flex->Young, 100.0f));
        TestTrue(TEXT("Damping should be 0.5"), MjTestMath::NearlyEqual(Flex->Damping, 0.5f));
        TestEqual(TEXT("Should have 3 pin IDs"), Flex->PinIds.Num(), 3);
    }

    S.Cleanup();
    return true;
}

// ============================================================================
// URLab.Flexcomp.Elasticity_Imported
//   Elasticity sub-element attributes should be parsed.
// ============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMjFlexcompElasticityImported,
    "URLab.Flexcomp.Elasticity_Imported",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMjFlexcompElasticityImported::RunTest(const FString& Parameters)
{
    FMjXmlImportSession S;
    bool bOk = S.Init(TEXT(
        "<mujoco>"
        "  <worldbody>"
        "    <body name=\"parent\">"
        "      <geom size=\"1\"/>"
        "      <flexcomp name=\"elastic\" type=\"grid\" dim=\"2\" count=\"3 3 1\" spacing=\"0.1 0.1 0.1\" mass=\"0.5\" radius=\"0.01\">"
        "        <elasticity young=\"1000\" poisson=\"0.3\" damping=\"0.01\"/>"
        "      </flexcomp>"
        "    </body>"
        "  </worldbody>"
        "</mujoco>"));

    if (!bOk)
    {
        AddError(FString::Printf(TEXT("Import Init failed: %s"), *S.LastError));
        S.Cleanup();
        return false;
    }

    UMjFlexcomp* Flex = S.FindTemplate<UMjFlexcomp>(TEXT("elastic"));
    TestNotNull(TEXT("Should find flexcomp template"), Flex);

    if (Flex)
    {
        TestTrue(TEXT("Young should be 1000"), MjTestMath::NearlyEqual(Flex->Young, 1000.0f));
        TestTrue(TEXT("Poisson should be 0.3"), MjTestMath::NearlyEqual(Flex->Poisson, 0.3f));
        TestTrue(TEXT("Damping should be 0.01"), MjTestMath::NearlyEqual(Flex->Damping, 0.01f));
    }

    S.Cleanup();
    return true;
}
