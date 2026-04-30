// Copyright (c) 2026 Jonathan Embley-Riches. All rights reserved.
// Licensed under the Apache License, Version 2.0.

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "Tests/MjTestHelpers.h"
#include "MuJoCo/Components/Geometry/MjGeom.h"
#include "MuJoCo/Components/Joints/MjJoint.h"
#include "MuJoCo/Components/Actuators/MjActuator.h"
#include "MuJoCo/Components/Bodies/MjBody.h"
#include "mujoco/mujoco.h"

// ============================================================================
// URLab.Import.MenagerieH1
//   Imports the Unitree H1 from disk, compiles through our pipeline,
//   and compares body/joint/actuator/geom counts against native MuJoCo.
// ============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMjImportMenagerieH1,
    "URLab.Import.MenagerieH1",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMjImportMenagerieH1::RunTest(const FString& Parameters)
{
    FString MenageriePath = FPlatformMisc::GetEnvironmentVariable(TEXT("MUJOCO_MENAGERIE_PATH"));
    FString XmlPath = MenageriePath.IsEmpty()
        ? FPaths::Combine(FPaths::ProjectPluginsDir(), TEXT("UnrealRoboticsLab/Content/TestData/h1_ue.xml"))
        : FPaths::Combine(MenageriePath, TEXT("unitree_h1/h1_ue.xml"));

    if (!FPaths::FileExists(XmlPath))
    {
        AddWarning(FString::Printf(TEXT("Skipping: XML not found at %s"), *XmlPath));
        return true;
    }

    // Phase 1: Import and check Blueprint was created
    FMjXmlImportSession S;
    if (!S.InitFromFile(XmlPath))
    {
        AddError(FString::Printf(TEXT("InitFromFile failed: %s"), *S.LastError));
        S.Cleanup();
        return false;
    }

    AddInfo(FString::Printf(TEXT("Native MuJoCo: %d bodies, %d joints, %d actuators, %d geoms"),
        (int32)S.NativeBodyCount, (int32)S.NativeJointCount, (int32)S.NativeActuatorCount, (int32)S.NativeGeomCount));

    // Check Blueprint has components (note: defaults are included in raw count)
    int32 BPBodies = S.CountTemplates<UMjBody>();
    int32 BPJoints = S.CountTemplates<UMjJoint>();
    int32 BPActuators = S.CountTemplates<UMjActuator>();
    int32 BPGeoms = S.CountTemplates<UMjGeom>();

    AddInfo(FString::Printf(TEXT("Blueprint: %d bodies, %d joints, %d actuators, %d geoms (includes defaults)"),
        BPBodies, BPJoints, BPActuators, BPGeoms));

    // Bodies: native includes world body, our Blueprint may not have an explicit UMjBody for world
    TestTrue(TEXT("Should have at least 1 body"), BPBodies >= 1);

    // Geoms: we should have at least as many as native (some may be split for visual/collision)
    TestTrue(TEXT("Should have geoms"), BPGeoms >= 1);

    // Phase 2: Compile through our pipeline
    if (!S.Compile())
    {
        AddError(FString::Printf(TEXT("Compile failed: %s"), *S.LastError));
        S.Cleanup();
        return false;
    }

    mjModel* M = S.Model();
    TestTrue(TEXT("Model should be valid"), M != nullptr);

    if (M)
    {
        AddInfo(FString::Printf(TEXT("Our compiled: %d bodies, %d joints, %d actuators, %d geoms"),
            M->nbody, M->njnt, M->nu, M->ngeom));

        // The compiled model should match native MuJoCo counts
        // Note: our pipeline adds a prefix to names, but counts should match
        TestEqual(TEXT("Compiled body count should match native"), (int32)M->nbody, (int32)S.NativeBodyCount);
        TestEqual(TEXT("Compiled joint count should match native"), (int32)M->njnt, (int32)S.NativeJointCount);
        TestEqual(TEXT("Compiled actuator count should match native"), (int32)M->nu, (int32)S.NativeActuatorCount);
        TestEqual(TEXT("Compiled geom count should match native"), (int32)M->ngeom, (int32)S.NativeGeomCount);

        // Verify the model can step without crashing
        mjData* D = S.Data();
        if (D)
        {
            mj_step(M, D);
            mj_step(M, D);
            AddInfo(TEXT("Model steps without crash"));
        }
    }

    S.Cleanup();
    return true;
}

// ============================================================================
// URLab.Import.MenagerieVX300s
//   Imports the Trossen VX300s from disk and verifies all 7 actuators survive
//   the compile. This is the canonical regression for the Default-class /
//   joint-name collision bug — vx300s.xml pairs a <default class="waist">
//   with a <joint name="waist"> and a <position joint="waist">, and before
//   the fix URLab's importer silently dropped every actuator (m->nu == 0)
//   because the joint's UE variable name was disambiguated to "waist1" while
//   the actuator's target kept the raw "waist" reference.
// ============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMjImportMenagerieVX300s,
    "URLab.Import.MenagerieVX300s",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMjImportMenagerieVX300s::RunTest(const FString& Parameters)
{
    FString MenageriePath = FPlatformMisc::GetEnvironmentVariable(TEXT("MUJOCO_MENAGERIE_PATH"));
    if (MenageriePath.IsEmpty())
    {
        AddWarning(TEXT("Skipping: MUJOCO_MENAGERIE_PATH env var not set"));
        return true;
    }
    FString XmlPath = FPaths::Combine(MenageriePath, TEXT("trossen_vx300s/vx300s.xml"));

    if (!FPaths::FileExists(XmlPath))
    {
        AddWarning(FString::Printf(TEXT("Skipping: XML not found at %s"), *XmlPath));
        return true;
    }

    FMjXmlImportSession S;
    if (!S.InitFromFile(XmlPath))
    {
        AddError(FString::Printf(TEXT("InitFromFile failed: %s"), *S.LastError));
        S.Cleanup();
        return false;
    }

    AddInfo(FString::Printf(TEXT("Native MuJoCo: %d bodies, %d joints, %d actuators, %d geoms"),
        (int32)S.NativeBodyCount, (int32)S.NativeJointCount, (int32)S.NativeActuatorCount, (int32)S.NativeGeomCount));

    // VX300s: 7 actuators (waist, shoulder, elbow, forearm_roll, wrist_angle, wrist_rotate, gripper)
    TestEqual(TEXT("Native MuJoCo should report 7 actuators for vx300s"),
        (int32)S.NativeActuatorCount, 7);

    if (!S.Compile())
    {
        AddError(FString::Printf(TEXT("Compile failed: %s"), *S.LastError));
        S.Cleanup();
        return false;
    }

    mjModel* M = S.Model();
    if (!TestNotNull(TEXT("Model valid"), M))
    {
        S.Cleanup();
        return false;
    }

    AddInfo(FString::Printf(TEXT("URLab compiled: %d bodies, %d joints, %d actuators, %d geoms"),
        M->nbody, M->njnt, M->nu, M->ngeom));

    // THE critical assertion: actuators survived the Default-class name collision.
    TestEqual(TEXT("Compiled actuator count matches native (no silent actuator drop)"),
        (int32)M->nu, (int32)S.NativeActuatorCount);
    TestEqual(TEXT("Compiled joint count matches native"),
        (int32)M->njnt, (int32)S.NativeJointCount);

    S.Cleanup();
    return true;
}

// ============================================================================
// URLab.Import.DefaultFromTo
//   Tests that geoms inheriting fromto from default classes compile correctly.
//   This is the H1 foot geom pattern: <geom class="foot1" /> inherits
//   fromto from <default class="foot1"><geom fromto="..." /></default>
// ============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMjImportDefaultFromTo,
    "URLab.Import.DefaultFromTo",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMjImportDefaultFromTo::RunTest(const FString& Parameters)
{
    const FString Xml = TEXT(R"(
        <mujoco>
          <default>
            <default class="parent">
              <geom type="capsule" size=".014" group="3" />
              <default class="foot1">
                <geom fromto="-.035 0 -0.056 .02 0 -0.045" />
              </default>
              <default class="foot2">
                <geom fromto=".02 0 -0.045 .115 0 -0.056" />
              </default>
            </default>
          </default>
          <worldbody>
            <body name="test_body" pos="0 0 1">
              <freejoint/>
              <geom type="sphere" size="0.1"/>
              <body name="foot" pos="0 0 -1">
                <joint name="ankle" type="hinge" axis="0 1 0"/>
                <geom class="foot1" />
                <geom class="foot2" />
              </body>
            </body>
          </worldbody>
        </mujoco>
    )");

    // Phase 1: Verify native MuJoCo compiles this
    FMjTestSession Native;
    if (!Native.CompileXml(Xml))
    {
        AddError(FString::Printf(TEXT("Native compile failed: %s"), *Native.LastError));
        return false;
    }

    AddInfo(FString::Printf(TEXT("Native: %d bodies, %d geoms, %d joints"),
        Native.m->nbody, Native.m->ngeom, Native.m->njnt));

    TestEqual(TEXT("Native should have 3 bodies (world + test + foot)"), (int32)Native.m->nbody, 3);
    TestEqual(TEXT("Native should have 3 geoms (sphere + 2 foot capsules)"), (int32)Native.m->ngeom, 3);

    // Dump native geom sizes for comparison
    for (int i = 0; i < Native.m->ngeom; ++i)
    {
        const char* gname = Native.m->names + Native.m->name_geomadr[i];
        AddInfo(FString::Printf(TEXT("  Native geom %d '%hs': type=%d size=[%.4f, %.4f, %.4f]"),
            i, gname, Native.m->geom_type[i],
            Native.m->geom_size[i*3+0], Native.m->geom_size[i*3+1], Native.m->geom_size[i*3+2]));
    }
    Native.Cleanup();

    // Phase 2: Import through our pipeline
    FMjXmlImportSession S;
    if (!S.Init(Xml))
    {
        AddError(FString::Printf(TEXT("Import Init failed: %s"), *S.LastError));
        S.Cleanup();
        return false;
    }

    // Phase 3: Compile
    if (!S.Compile())
    {
        AddError(FString::Printf(TEXT("Our compile failed: %s"), *S.LastError));
        S.Cleanup();
        return false;
    }

    mjModel* M = S.Model();
    TestTrue(TEXT("Model should be valid"), M != nullptr);

    if (M)
    {
        AddInfo(FString::Printf(TEXT("Our compiled: %d bodies, %d geoms, %d joints"),
            M->nbody, M->ngeom, M->njnt));

        TestEqual(TEXT("Should have 3 bodies"), (int32)M->nbody, 3);
        TestEqual(TEXT("Should have 3 geoms"), (int32)M->ngeom, 3);

        // Verify foot geoms have non-zero size (the fromto should have set size[1])
        for (int i = 0; i < M->ngeom; ++i)
        {
            if (M->geom_type[i] == mjGEOM_CAPSULE)
            {
                float radius = (float)M->geom_size[i * 3 + 0];
                float halflen = (float)M->geom_size[i * 3 + 1];
                AddInfo(FString::Printf(TEXT("  Capsule geom %d: radius=%.4f halflen=%.4f"), i, radius, halflen));
                TestTrue(FString::Printf(TEXT("Capsule %d radius should be > 0"), i), radius > 0.001f);
                TestTrue(FString::Printf(TEXT("Capsule %d halflen should be > 0"), i), halflen > 0.001f);
            }
        }

        // Step to verify no crash
        mjData* D = S.Data();
        if (D)
        {
            mj_step(M, D);
            AddInfo(TEXT("Model steps without crash"));
        }
    }

    S.Cleanup();
    return true;
}
