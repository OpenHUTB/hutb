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

// MjImportTests.cpp
//
// Two tiers of import tests:
//
//   TIER 1 — FMjTestSession (pure MuJoCo, no UE pipeline)
//     These tests load inline MJCF XML directly via mj_parseXMLString and
//     inspect the resulting mjModel*.  They mirror the structure of MuJoCo's
//     own xml_native_reader_test.cc and serve as a reference baseline that
//     documents what MuJoCo expects from any given XML feature.
//
//   TIER 2 — FMjXmlImportSession (full URLab importer)
//     These tests pass the same XML through UMujocoGenerationAction and
//     verify that UE component properties were set correctly.  After calling
//     Compile() they also verify the compiled mjModel matches expectations.
//     A discrepancy between Tier 1 and Tier 2 always means a URLab bug.
//
//   ROUND-TRIP tests compare Tier 1 model counts with Tier 2 counts directly.
//   If ngeom/nbody/nsensor differ, the importer dropped or duplicated elements.

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "Tests/MjTestHelpers.h"

// Component types for FindTemplate<>
#include "MuJoCo/Components/Bodies/MjBody.h"
#include "MuJoCo/Components/Geometry/MjGeom.h"
#include "MuJoCo/Components/Joints/MjJoint.h"
#include "MuJoCo/Components/Sensors/MjSensor.h"
#include "MuJoCo/Components/Sensors/MjJointPosSensor.h"
#include "MuJoCo/Components/Sensors/MjJointVelSensor.h"
#include "MuJoCo/Components/Actuators/MjActuator.h"
#include "MuJoCo/Components/Actuators/MjMotorActuator.h"
#include "MuJoCo/Components/Tendons/MjTendon.h"
#include "MuJoCo/Components/Physics/MjContactPair.h"
#include "MuJoCo/Components/Constraints/MjEquality.h"
#include "MuJoCo/Components/Keyframes/MjKeyframe.h"
#include "MuJoCo/Core/MjArticulation.h"

// =============================================================================
// TIER 1 — Pure MuJoCo baseline tests (FMjTestSession)
// Mirror MuJoCo's xml_native_reader_test.cc structure.
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTest_MjImport_MJ_BodyPos,
    "URLab.Import.MJ_BodyPos",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FTest_MjImport_MJ_BodyPos::RunTest(const FString&)
{
    FMjTestSession S;
    if (!S.CompileXml(TEXT(R"(
        <mujoco>
          <worldbody>
            <body name="b1" pos="1 2 3">
              <geom size=".1"/>
            </body>
          </worldbody>
        </mujoco>
    )"))) { AddError(S.LastError); return false; }

    int bid = S.BodyId("b1");
    TestTrue(TEXT("b1 body id valid"), bid >= 0);
    TestNearlyEqual(TEXT("b1 x"), (float)S.m->body_pos[3*bid+0], 1.0f, 1e-4f);
    TestNearlyEqual(TEXT("b1 y"), (float)S.m->body_pos[3*bid+1], 2.0f, 1e-4f);
    TestNearlyEqual(TEXT("b1 z"), (float)S.m->body_pos[3*bid+2], 3.0f, 1e-4f);
    S.Cleanup();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTest_MjImport_MJ_GeomSize,
    "URLab.Import.MJ_GeomSize",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FTest_MjImport_MJ_GeomSize::RunTest(const FString&)
{
    FMjTestSession S;
    if (!S.CompileXml(TEXT(R"(
        <mujoco>
          <worldbody>
            <body>
              <geom name="g1" type="sphere" size=".5"/>
              <geom name="g2" type="box" size="1 2 3"/>
            </body>
          </worldbody>
        </mujoco>
    )"))) { AddError(S.LastError); return false; }

    int g1 = S.GeomId("g1"), g2 = S.GeomId("g2");
    TestTrue(TEXT("g1 valid"), g1 >= 0);
    TestTrue(TEXT("g2 valid"), g2 >= 0);
    TestNearlyEqual(TEXT("sphere radius"), (float)S.m->geom_size[3*g1], 0.5f, 1e-4f);
    TestNearlyEqual(TEXT("box x"),         (float)S.m->geom_size[3*g2+0], 1.0f, 1e-4f);
    TestNearlyEqual(TEXT("box y"),         (float)S.m->geom_size[3*g2+1], 2.0f, 1e-4f);
    TestNearlyEqual(TEXT("box z"),         (float)S.m->geom_size[3*g2+2], 3.0f, 1e-4f);
    S.Cleanup();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTest_MjImport_MJ_JointRange,
    "URLab.Import.MJ_JointRange",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FTest_MjImport_MJ_JointRange::RunTest(const FString&)
{
    // MuJoCo defaults to angle="degree"; use explicit radian mode for numeric values.
    FMjTestSession S;
    if (!S.CompileXml(TEXT(R"(
        <mujoco>
          <compiler angle="radian"/>
          <worldbody>
            <body>
              <joint name="j1" type="hinge" range="-1.57 1.57" limited="true"/>
              <geom size=".1"/>
            </body>
          </worldbody>
        </mujoco>
    )"))) { AddError(S.LastError); return false; }

    int jid = S.JointId("j1");
    TestTrue(TEXT("j1 valid"), jid >= 0);
    TestTrue(TEXT("j1 limited"),  S.m->jnt_limited[jid] != 0);
    TestNearlyEqual(TEXT("range lo"), (float)S.m->jnt_range[2*jid+0], -1.57f, 1e-3f);
    TestNearlyEqual(TEXT("range hi"), (float)S.m->jnt_range[2*jid+1],  1.57f, 1e-3f);
    S.Cleanup();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTest_MjImport_MJ_DefaultClassOverride,
    "URLab.Import.MJ_DefaultClassOverride",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FTest_MjImport_MJ_DefaultClassOverride::RunTest(const FString&)
{
    // MuJoCo: explicit class= overrides parent childclass=
    // Default inheritance works natively because mjs_add*() receives the resolved mjsDefault*.
    FMjTestSession S;
    if (!S.CompileXml(TEXT(R"(
        <mujoco>
          <default>
            <default class="size2"><geom size="2"/></default>
            <default class="size3"><geom size="3"/></default>
          </default>
          <worldbody>
            <body childclass="size2">
              <geom name="g_inherit"/>
              <geom name="g_override" class="size3"/>
            </body>
          </worldbody>
        </mujoco>
    )"))) { AddError(S.LastError); return false; }

    int g0 = S.GeomId("g_inherit"), g1 = S.GeomId("g_override");
    TestTrue(TEXT("g_inherit valid"), g0 >= 0);
    TestTrue(TEXT("g_override valid"), g1 >= 0);
    TestNearlyEqual(TEXT("inherited size2"),  (float)S.m->geom_size[3*g0], 2.0f, 1e-4f);
    TestNearlyEqual(TEXT("overridden size3"), (float)S.m->geom_size[3*g1], 3.0f, 1e-4f);
    S.Cleanup();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTest_MjImport_MJ_FrameElement,
    "URLab.Import.MJ_FrameElement",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FTest_MjImport_MJ_FrameElement::RunTest(const FString&)
{
    // MuJoCo: <frame> applies a transform to nested elements, then is dissolved
    FMjTestSession S;
    if (!S.CompileXml(TEXT(R"(
        <mujoco>
          <worldbody>
            <frame pos="0 0 1">
              <geom name="g_in_frame" size=".1"/>
            </frame>
          </worldbody>
        </mujoco>
    )"))) { AddError(S.LastError); return false; }

    int gid = S.GeomId("g_in_frame");
    TestTrue(TEXT("geom inside frame valid"), gid >= 0);
    TestNearlyEqual(TEXT("frame z offset applied"), (float)S.m->geom_pos[3*gid+2], 1.0f, 1e-4f);
    S.Cleanup();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTest_MjImport_MJ_FrameChildclass,
    "URLab.Import.MJ_FrameChildclass",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FTest_MjImport_MJ_FrameChildclass::RunTest(const FString&)
{
    // <frame childclass="x"> propagates default to nested geoms
    FMjTestSession S;
    if (!S.CompileXml(TEXT(R"(
        <mujoco>
          <default>
            <default class="myclass"><geom size=".5"/></default>
          </default>
          <worldbody>
            <frame childclass="myclass">
              <geom name="g1"/>
              <geom name="g2" class="myclass"/>
            </frame>
          </worldbody>
        </mujoco>
    )"))) { AddError(S.LastError); return false; }

    int g1 = S.GeomId("g1"), g2 = S.GeomId("g2");
    TestTrue(TEXT("g1 valid"), g1 >= 0);
    TestTrue(TEXT("g2 valid"), g2 >= 0);
    TestNearlyEqual(TEXT("g1 size from childclass"), (float)S.m->geom_size[3*g1], 0.5f, 1e-4f);
    TestNearlyEqual(TEXT("g2 size from class"),      (float)S.m->geom_size[3*g2], 0.5f, 1e-4f);
    S.Cleanup();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTest_MjImport_MJ_TendonArmature,
    "URLab.Import.MJ_TendonArmature",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FTest_MjImport_MJ_TendonArmature::RunTest(const FString&)
{
    FMjTestSession S;
    if (!S.CompileXml(TEXT(R"(
        <mujoco>
          <worldbody>
            <site name="a"/>
            <body pos="1 0 0">
              <joint name="slide" type="slide"/>
              <geom size=".1"/>
              <site name="b"/>
            </body>
          </worldbody>
          <tendon>
            <spatial name="t_spatial" armature="1.5">
              <site site="a"/>
              <site site="b"/>
            </spatial>
            <fixed name="t_fixed" armature="2.5">
              <joint joint="slide" coef="1"/>
            </fixed>
          </tendon>
        </mujoco>
    )"))) { AddError(S.LastError); return false; }

    TestEqual(TEXT("ntendon"), (int)S.m->ntendon, 2);
    TestNearlyEqual(TEXT("spatial armature"), (float)S.m->tendon_armature[0], 1.5f, 1e-4f);
    TestNearlyEqual(TEXT("fixed armature"),   (float)S.m->tendon_armature[1], 2.5f, 1e-4f);
    S.Cleanup();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTest_MjImport_MJ_EqualityPolycoef,
    "URLab.Import.MJ_EqualityPolycoef",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FTest_MjImport_MJ_EqualityPolycoef::RunTest(const FString&)
{
    FMjTestSession S;
    if (!S.CompileXml(TEXT(R"(
        <mujoco>
          <worldbody>
            <body><joint name="j0"/><geom size="1"/></body>
            <body><joint name="j1"/><geom size="1"/></body>
          </worldbody>
          <equality>
            <joint joint1="j0" joint2="j1" polycoef="5 6 7 8 9"/>
          </equality>
        </mujoco>
    )"))) { AddError(S.LastError); return false; }

    TestEqual(TEXT("neq"), (int)S.m->neq, 1);
    TestNearlyEqual(TEXT("coef0"), (float)S.m->eq_data[0], 5.0f, 1e-4f);
    TestNearlyEqual(TEXT("coef1"), (float)S.m->eq_data[1], 6.0f, 1e-4f);
    TestNearlyEqual(TEXT("coef4"), (float)S.m->eq_data[4], 9.0f, 1e-4f);
    S.Cleanup();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTest_MjImport_MJ_OptionTimestepGravity,
    "URLab.Import.MJ_OptionTimestepGravity",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FTest_MjImport_MJ_OptionTimestepGravity::RunTest(const FString&)
{
    FMjTestSession S;
    if (!S.CompileXml(TEXT(R"(
        <mujoco>
          <option timestep="0.005" gravity="0 0 -9.81"/>
        </mujoco>
    )"))) { AddError(S.LastError); return false; }

    TestNearlyEqual(TEXT("timestep"), (float)S.m->opt.timestep, 0.005f, 1e-6f);
    TestNearlyEqual(TEXT("gravity z"), (float)S.m->opt.gravity[2], -9.81f, 1e-3f);
    S.Cleanup();
    return true;
}

// =============================================================================
// TIER 2 — URLab importer tests (FMjXmlImportSession)
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTest_MjImport_URLab_BodyPos,
    "URLab.Import.URLab_BodyPos",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FTest_MjImport_URLab_BodyPos::RunTest(const FString&)
{
    // body pos="1 2 3" → UE RelativeLocation (100, -200, 300) cm
    // Coordinate rule: scale ×100, negate Y
    FMjXmlImportSession S;
    if (!S.Init(TEXT(R"(
        <mujoco>
          <worldbody>
            <body name="b1" pos="1 2 3">
              <geom size=".1"/>
            </body>
          </worldbody>
        </mujoco>
    )"))) { AddError(S.LastError); return false; }

    UMjBody* B = S.FindTemplate<UMjBody>(TEXT("b1"));
    if (!B) { AddError(TEXT("Body 'b1' not found in Blueprint")); S.Cleanup(); return false; }

    FVector Loc = B->GetRelativeLocation();
    TestNearlyEqual(TEXT("X=100 cm"),          (float)Loc.X, 100.0f,  1.0f);
    TestNearlyEqual(TEXT("Y=-200 cm (negated)"),(float)Loc.Y, -200.0f, 1.0f);
    TestNearlyEqual(TEXT("Z=300 cm"),           (float)Loc.Z, 300.0f,  1.0f);

    S.Cleanup();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTest_MjImport_URLab_BodyIdentityQuat,
    "URLab.Import.URLab_BodyIdentityQuat",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FTest_MjImport_URLab_BodyIdentityQuat::RunTest(const FString&)
{
    // quat="1 0 0 0" is MuJoCo identity → should map to UE identity rotation
    FMjXmlImportSession S;
    if (!S.Init(TEXT(R"(
        <mujoco>
          <worldbody>
            <body name="b1" quat="1 0 0 0">
              <geom size=".1"/>
            </body>
          </worldbody>
        </mujoco>
    )"))) { AddError(S.LastError); return false; }

    UMjBody* B = S.FindTemplate<UMjBody>(TEXT("b1"));
    if (!B) { AddError(TEXT("Body 'b1' not found")); S.Cleanup(); return false; }

    FQuat Q = B->GetRelativeRotationCache().GetCachedQuat();
    TestTrue(TEXT("identity quat"), Q.Equals(FQuat::Identity, 0.01f));

    S.Cleanup();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTest_MjImport_URLab_GeomFriction,
    "URLab.Import.URLab_GeomFriction",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FTest_MjImport_URLab_GeomFriction::RunTest(const FString&)
{
    FMjXmlImportSession S;
    if (!S.Init(TEXT(R"(
        <mujoco>
          <worldbody>
            <body>
              <geom name="g1" size=".1" friction="0.8 0.1 0.01"/>
            </body>
          </worldbody>
        </mujoco>
    )"))) { AddError(S.LastError); return false; }

    UMjGeom* G = S.FindTemplate<UMjGeom>(TEXT("g1"));
    if (!G) { AddError(TEXT("Geom 'g1' not found")); S.Cleanup(); return false; }

    TestNearlyEqual(TEXT("friction[0]"), G->Friction[0], 0.8f,  1e-4f);
    TestNearlyEqual(TEXT("friction[1]"), G->Friction[1], 0.1f,  1e-4f);
    TestNearlyEqual(TEXT("friction[2]"), G->Friction[2], 0.01f, 1e-3f);

    S.Cleanup();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTest_MjImport_URLab_JointRangeAndDamping,
    "URLab.Import.URLab_JointRangeAndDamping",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FTest_MjImport_URLab_JointRangeAndDamping::RunTest(const FString&)
{
    FMjXmlImportSession S;
    if (!S.Init(TEXT(R"(
        <mujoco>
          <compiler angle="radian"/>
          <worldbody>
            <body>
              <joint name="j1" type="hinge" range="-1.57 1.57" limited="true" damping="0.5"/>
              <geom size=".1"/>
            </body>
          </worldbody>
        </mujoco>
    )"))) { AddError(S.LastError); return false; }

    UMjJoint* J = S.FindTemplate<UMjJoint>(TEXT("j1"));
    if (!J) { AddError(TEXT("Joint 'j1' not found")); S.Cleanup(); return false; }

    TestNearlyEqual(TEXT("range lo"), J->Range[0], -1.57f, 1e-3f);
    TestNearlyEqual(TEXT("range hi"), J->Range[1],  1.57f, 1e-3f);
    TestTrue(TEXT("damping has values"), J->Damping.Num() > 0);
    if (J->Damping.Num() > 0)
        TestNearlyEqual(TEXT("damping[0]"), J->Damping[0], 0.5f, 1e-4f);

    S.Cleanup();
    return true;
}

// Regression: parser used to drop CompilerSettings when recursing into
// <default> blocks, so joints declared inside a default class always saw
// the hardcoded bAngleInDegrees=true fallback. For angle="radian" models
// (e.g. mujoco_menagerie's unitree_go1) the default joint's Range would
// then be re-scaled by π/180, shrinking ±0.86 rad to ±0.015 rad and
// making imported robots barely move. Verifies the default joint
// template keeps its radian value intact.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTest_MjImport_URLab_DefaultClassJointRangeRadians,
    "URLab.Import.URLab_DefaultClassJointRangeRadians",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FTest_MjImport_URLab_DefaultClassJointRangeRadians::RunTest(const FString&)
{
    FMjXmlImportSession S;
    if (!S.Init(TEXT(R"(
        <mujoco>
          <compiler angle="radian" autolimits="true"/>
          <default>
            <default class="hip"><joint range="-0.86 0.86"/></default>
          </default>
          <worldbody>
            <body>
              <joint class="hip" name="j_hip"/>
              <geom size=".1"/>
            </body>
          </worldbody>
        </mujoco>
    )"))) { AddError(S.LastError); return false; }

    UMjJoint* DefaultJoint = nullptr;
    if (S.Blueprint && S.Blueprint->SimpleConstructionScript)
    {
        for (USCS_Node* Node : S.Blueprint->SimpleConstructionScript->GetAllNodes())
        {
            UMjJoint* J = Cast<UMjJoint>(Node->ComponentTemplate);
            if (J && J->bIsDefault) { DefaultJoint = J; break; }
        }
    }
    if (!DefaultJoint) { AddError(TEXT("No default-class joint template found")); S.Cleanup(); return false; }

    if (DefaultJoint->Range.Num() >= 2)
    {
        TestNearlyEqual(TEXT("default joint Range[0] preserved as radians"),
                        DefaultJoint->Range[0], -0.86f, 1e-3f);
        TestNearlyEqual(TEXT("default joint Range[1] preserved as radians"),
                        DefaultJoint->Range[1],  0.86f, 1e-3f);
    }
    else
    {
        AddError(TEXT("default joint Range has < 2 entries"));
    }

    S.Cleanup();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTest_MjImport_URLab_SensorType,
    "URLab.Import.URLab_SensorType",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FTest_MjImport_URLab_SensorType::RunTest(const FString&)
{
    // Sensor XML tags must produce the correct concrete UE sensor subclass
    FMjXmlImportSession S;
    if (!S.Init(TEXT(R"(
        <mujoco>
          <worldbody>
            <body>
              <joint name="j1" type="hinge"/>
              <geom size=".1"/>
            </body>
          </worldbody>
          <sensor>
            <jointpos name="s_jpos" joint="j1"/>
            <jointvel name="s_jvel" joint="j1"/>
          </sensor>
        </mujoco>
    )"))) { AddError(S.LastError); return false; }

    UMjJointPosSensor* SJP = S.FindTemplate<UMjJointPosSensor>(TEXT("s_jpos"));
    UMjJointVelSensor* SJV = S.FindTemplate<UMjJointVelSensor>(TEXT("s_jvel"));

    TestNotNull(TEXT("jointpos concrete class"), SJP);
    TestNotNull(TEXT("jointvel concrete class"), SJV);

    if (SJP) TestEqual(TEXT("jointpos target joint"), SJP->TargetName, FString(TEXT("j1")));
    if (SJV) TestEqual(TEXT("jointvel target joint"), SJV->TargetName, FString(TEXT("j1")));

    S.Cleanup();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTest_MjImport_URLab_OptionTimestep,
    "URLab.Import.URLab_OptionTimestep",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FTest_MjImport_URLab_OptionTimestep::RunTest(const FString&)
{
    // <option> attributes are parsed into AMjArticulation::SimOptions on the CDO
    FMjXmlImportSession S;
    if (!S.Init(TEXT(R"(
        <mujoco>
          <option timestep="0.005" gravity="0 0 -5"/>
        </mujoco>
    )"))) { AddError(S.LastError); return false; }

    if (!S.Blueprint) { AddError(TEXT("No Blueprint")); S.Cleanup(); return false; }
    AMjArticulation* CDO = Cast<AMjArticulation>(S.Blueprint->GeneratedClass->GetDefaultObject());
    if (!CDO) { AddError(TEXT("CDO cast failed")); S.Cleanup(); return false; }

    TestNearlyEqual(TEXT("timestep"), CDO->SimOptions.Timestep, 0.005f, 1e-6f);
    TestNearlyEqual(TEXT("gravity z"), (float)CDO->SimOptions.Gravity.Z, -5.0f, 1e-4f);

    S.Cleanup();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTest_MjImport_URLab_TendonArmature,
    "URLab.Import.URLab_TendonArmature",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FTest_MjImport_URLab_TendonArmature::RunTest(const FString&)
{
    FMjXmlImportSession S;
    if (!S.Init(TEXT(R"(
        <mujoco>
          <worldbody>
            <site name="a"/>
            <body pos="1 0 0">
              <joint name="sl" type="slide"/>
              <geom size=".1"/>
              <site name="b"/>
            </body>
          </worldbody>
          <tendon>
            <fixed name="tf" armature="2.5">
              <joint joint="sl" coef="1"/>
            </fixed>
          </tendon>
        </mujoco>
    )"))) { AddError(S.LastError); return false; }

    UMjTendon* T = S.FindTemplate<UMjTendon>(TEXT("tf"));
    if (!T) { AddError(TEXT("Tendon 'tf' not found")); S.Cleanup(); return false; }

    TestNearlyEqual(TEXT("armature"), T->Armature, 2.5f, 1e-4f);

    S.Cleanup();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTest_MjImport_URLab_ContactPair,
    "URLab.Import.URLab_ContactPair",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FTest_MjImport_URLab_ContactPair::RunTest(const FString&)
{
    FMjXmlImportSession S;
    if (!S.Init(TEXT(R"(
        <mujoco>
          <worldbody>
            <geom name="floor" type="plane" size="1 1 1"/>
            <body>
              <geom name="ball" type="sphere" size=".1"/>
            </body>
          </worldbody>
          <contact>
            <pair geom1="floor" geom2="ball" condim="3"/>
          </contact>
        </mujoco>
    )"))) { AddError(S.LastError); return false; }

    UMjContactPair* CP = S.FindFirstTemplate<UMjContactPair>();
    if (!CP) { AddError(TEXT("ContactPair not found")); S.Cleanup(); return false; }

    TestEqual(TEXT("geom1"), CP->Geom1, FString(TEXT("floor")));
    TestEqual(TEXT("geom2"), CP->Geom2, FString(TEXT("ball")));
    TestEqual(TEXT("condim"), CP->Condim, 3);

    S.Cleanup();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTest_MjImport_URLab_EqualityWeld,
    "URLab.Import.URLab_EqualityWeld",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FTest_MjImport_URLab_EqualityWeld::RunTest(const FString&)
{
    FMjXmlImportSession S;
    if (!S.Init(TEXT(R"(
        <mujoco>
          <worldbody>
            <body name="b1"><geom size=".1"/></body>
            <body name="b2"><geom size=".1"/></body>
          </worldbody>
          <equality>
            <weld name="w1" body1="b1" body2="b2"/>
          </equality>
        </mujoco>
    )"))) { AddError(S.LastError); return false; }

    UMjEquality* EQ = S.FindTemplate<UMjEquality>(TEXT("w1"));
    if (!EQ) { AddError(TEXT("Equality 'w1' not found")); S.Cleanup(); return false; }

    TestEqual(TEXT("body1"), EQ->Obj1, FString(TEXT("b1")));
    TestEqual(TEXT("body2"), EQ->Obj2, FString(TEXT("b2")));

    S.Cleanup();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTest_MjImport_URLab_IncludeFile,
    "URLab.Import.URLab_IncludeFile",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FTest_MjImport_URLab_IncludeFile::RunTest(const FString&)
{
    // Write child XML into the same temp dir that FMjXmlImportSession uses,
    // then reference it via a relative path (MuJoCo resolves includes relative
    // to the parent XML file on Windows — absolute paths get mangled).
    FString TestDir = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("URLab/Tests/incl"));
    IFileManager::Get().MakeDirectory(*TestDir, true);
    FString ChildPath = FPaths::Combine(TestDir, TEXT("child.xml"));
    FFileHelper::SaveStringToFile(
        TEXT("<mujoco><body name=\"inc_body\"><geom name=\"inc_geom\" size=\".2\"/></body></mujoco>"),
        *ChildPath);

    // Use a path relative to the parent XML directory ({Saved}/URLab/Tests/)
    static const FString RelChildPath = TEXT("incl/child.xml");
    FString MainXml = FString::Printf(TEXT(R"(
        <mujoco>
          <worldbody>
            <include file="%s"/>
          </worldbody>
        </mujoco>
    )"), *RelChildPath);

    FMjXmlImportSession S;
    if (!S.Init(MainXml))
    {
        AddError(S.LastError);
        IFileManager::Get().Delete(*ChildPath);
        return false;
    }

    UMjGeom* G = S.FindTemplate<UMjGeom>(TEXT("inc_geom"));
    TestNotNull(TEXT("geom from include file found"), G);

    IFileManager::Get().Delete(*ChildPath);
    S.Cleanup();
    return true;
}

// =============================================================================
// ROUND-TRIP tests: compare Tier1 model counts with Tier2 compiled counts
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTest_MjImport_RoundTrip_nbody,
    "URLab.Import.RoundTrip_nbody",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FTest_MjImport_RoundTrip_nbody::RunTest(const FString&)
{
    static const TCHAR* Xml = TEXT(R"(
        <mujoco>
          <worldbody>
            <body name="torso">
              <freejoint/>
              <geom type="sphere" size=".1"/>
              <body name="arm" pos="0 0 .2">
                <joint type="hinge"/>
                <geom type="capsule" size=".05" fromto="0 0 0 0 0 .2"/>
              </body>
            </body>
          </worldbody>
        </mujoco>
    )");

    // Reference: direct MuJoCo compile
    FMjTestSession Ref;
    if (!Ref.CompileXml(Xml)) { AddError(Ref.LastError); return false; }
    int ExpNbody = Ref.m->nbody;
    int ExpNgeom = Ref.m->ngeom;
    Ref.Cleanup();

    // URLab importer path
    FMjXmlImportSession S;
    if (!S.Init(Xml))  { AddError(S.LastError); return false; }
    if (!S.Compile())  { AddError(S.LastError); S.Cleanup(); return false; }

    TestEqual(TEXT("nbody matches reference"), (int)S.Model()->nbody, ExpNbody);
    TestEqual(TEXT("ngeom matches reference"), (int)S.Model()->ngeom, ExpNgeom);

    S.Cleanup();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTest_MjImport_RoundTrip_Sensors,
    "URLab.Import.RoundTrip_Sensors",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FTest_MjImport_RoundTrip_Sensors::RunTest(const FString&)
{
    static const TCHAR* Xml = TEXT(R"(
        <mujoco>
          <worldbody>
            <body>
              <joint name="j1" type="hinge"/>
              <geom size=".1"/>
            </body>
          </worldbody>
          <sensor>
            <jointpos name="s1" joint="j1"/>
            <jointvel name="s2" joint="j1"/>
          </sensor>
        </mujoco>
    )");

    FMjTestSession Ref;
    if (!Ref.CompileXml(Xml)) { AddError(Ref.LastError); return false; }
    int ExpNsensor = Ref.m->nsensor;
    Ref.Cleanup();

    FMjXmlImportSession S;
    if (!S.Init(Xml))  { AddError(S.LastError); return false; }
    if (!S.Compile())  { AddError(S.LastError); S.Cleanup(); return false; }

    TestEqual(TEXT("nsensor matches reference"), (int)S.Model()->nsensor, ExpNsensor);

    S.Cleanup();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTest_MjImport_RoundTrip_Actuators,
    "URLab.Import.RoundTrip_Actuators",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FTest_MjImport_RoundTrip_Actuators::RunTest(const FString&)
{
    static const TCHAR* Xml = TEXT(R"(
        <mujoco>
          <worldbody>
            <body>
              <joint name="j1" type="hinge"/>
              <geom size=".1"/>
            </body>
          </worldbody>
          <actuator>
            <motor name="a1" joint="j1"/>
            <position name="a2" joint="j1" kp="100"/>
          </actuator>
        </mujoco>
    )");

    FMjTestSession Ref;
    if (!Ref.CompileXml(Xml)) { AddError(Ref.LastError); return false; }
    int ExpNu = Ref.m->nu;
    Ref.Cleanup();

    FMjXmlImportSession S;
    if (!S.Init(Xml))  { AddError(S.LastError); return false; }
    if (!S.Compile())  { AddError(S.LastError); S.Cleanup(); return false; }

    TestEqual(TEXT("nu matches reference"), (int)S.Model()->nu, ExpNu);

    S.Cleanup();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTest_MjImport_RoundTrip_Defaults,
    "URLab.Import.RoundTrip_Defaults",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FTest_MjImport_RoundTrip_Defaults::RunTest(const FString&)
{
    // Default inheritance: geom inside a body with childclass="robot" should inherit
    // friction from the default.  Default values flow through mjs_add*() natively in
    // MuJoCo's spec API — this is why passing the resolved mjsDefault* to mjs_addGeom
    // is sufficient and no manual inheritance copy is needed.
    static const TCHAR* Xml = TEXT(R"(
        <mujoco>
          <default>
            <default class="robot">
              <geom friction="0.7 0.1 0.01"/>
            </default>
          </default>
          <worldbody>
            <body childclass="robot">
              <geom name="g1" size=".1"/>
            </body>
          </worldbody>
        </mujoco>
    )");

    FMjTestSession Ref;
    if (!Ref.CompileXml(Xml)) { AddError(Ref.LastError); return false; }
    int ExpNgeom = Ref.m->ngeom;
    float ExpFriction0 = (float)Ref.m->geom_friction[3 * Ref.GeomId("g1") + 0];
    Ref.Cleanup();

    FMjXmlImportSession S;
    if (!S.Init(Xml))  { AddError(S.LastError); return false; }
    if (!S.Compile())  { AddError(S.LastError); S.Cleanup(); return false; }

    // The URLab importer prefixes names so we can't use mj_name2id("g1") directly.
    // Instead compare ngeom count and check friction on the first non-worldbody geom.
    TestEqual(TEXT("ngeom matches reference"), (int)S.Model()->ngeom, ExpNgeom);
    if (S.Model()->ngeom > 0)
        TestNearlyEqual(TEXT("inherited friction[0]"), (float)S.Model()->geom_friction[0], 0.7f, 1e-4f);

    S.Cleanup();
    return true;
}

// Fix 2.11: <frame> elements are now handled by ImportNodeRecursive.
// This test verifies that geoms inside a frame are imported correctly and
// that the geom count in the compiled model matches the MuJoCo baseline.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTest_MjImport_RoundTrip_Frame_KnownGap,
    "URLab.Import.RoundTrip_Frame_KnownGap",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FTest_MjImport_RoundTrip_Frame_KnownGap::RunTest(const FString&)
{
    static const TCHAR* Xml = TEXT(R"(
        <mujoco>
          <worldbody>
            <frame pos="0 0 1">
              <geom name="g_in_frame" size=".1"/>
            </frame>
          </worldbody>
        </mujoco>
    )");

    // Direct MuJoCo: 1 geom with z=1 from frame offset
    FMjTestSession Ref;
    if (!Ref.CompileXml(Xml)) { AddError(Ref.LastError); return false; }
    int ExpNgeom = Ref.m->ngeom;
    Ref.Cleanup();

    FMjXmlImportSession S;
    if (!S.Init(Xml))  { AddError(S.LastError); return false; }
    if (!S.Compile())  { AddError(S.LastError); S.Cleanup(); return false; }

    TestEqual(TEXT("ngeom matches after frame support fix"), (int)S.Model()->ngeom, ExpNgeom);

    S.Cleanup();
    return true;
}

// =============================================================================
// URLab.Import.MJ_JointAxisImport
//   MuJoCo joint axis (0,1,0) → stored UE axis should have Y negated.
//   Verifies Fix 3.5 through the importer (via UMjJoint::ImportFromXml).
// =============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTest_MjImport_MJ_JointAxisImport,
    "URLab.Import.MJ_JointAxisImport",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FTest_MjImport_MJ_JointAxisImport::RunTest(const FString&)
{
    FMjXmlImportSession S;
    if (!S.Init(TEXT(R"(
        <mujoco>
          <worldbody>
            <body>
              <joint name="j1" type="hinge" axis="0 1 0"/>
              <geom size=".1"/>
            </body>
          </worldbody>
        </mujoco>
    )"))) { AddError(S.LastError); return false; }

    UMjJoint* JC = S.FindTemplate<UMjJoint>(TEXT("j1"));
    TestNotNull(TEXT("joint j1 found"), JC);
    if (JC)
    {
        // axis="0 1 0" in MuJoCo → stored in UE as (0,-1,0) after Y-negate
        TestTrue(TEXT("Axis X ≈ 0"),  FMath::Abs(JC->Axis.X) < 1e-4f);
        TestTrue(TEXT("Axis Y ≈ -1"), FMath::Abs(JC->Axis.Y + 1.0f) < 1e-4f);
        TestTrue(TEXT("Axis Z ≈ 0"),  FMath::Abs(JC->Axis.Z) < 1e-4f);
    }

    S.Cleanup();
    return true;
}

// =============================================================================
// URLab.Import.MJ_SensorTypeFromTag
//   Sensor XML tag must determine the sensor Type enum in UMjSensor.
//   Verifies Fix 2.4: ImportFromXml now reads the tag name.
// =============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTest_MjImport_MJ_SensorTypeFromTag,
    "URLab.Import.MJ_SensorTypeFromTag",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FTest_MjImport_MJ_SensorTypeFromTag::RunTest(const FString&)
{
    FMjXmlImportSession S;
    if (!S.Init(TEXT(R"(
        <mujoco>
          <worldbody>
            <body>
              <geom size=".1"/>
              <joint name="j1" type="hinge"/>
              <site name="s1"/>
            </body>
          </worldbody>
          <sensor>
            <accelerometer  name="s_acc"  site="s1"/>
            <gyro           name="s_gyro" site="s1"/>
            <jointpos       name="s_jp"   joint="j1"/>
            <velocimeter    name="s_vel"  site="s1"/>
          </sensor>
        </mujoco>
    )"))) { AddError(S.LastError); return false; }

    UMjSensor* Acc  = S.FindTemplate<UMjSensor>(TEXT("s_acc"));
    UMjSensor* Gyro = S.FindTemplate<UMjSensor>(TEXT("s_gyro"));
    UMjSensor* JP   = S.FindTemplate<UMjSensor>(TEXT("s_jp"));
    UMjSensor* Vel  = S.FindTemplate<UMjSensor>(TEXT("s_vel"));

    if (Acc)  TestEqual(TEXT("s_acc  type"),  Acc->Type,  EMjSensorType::Accelerometer);
    if (Gyro) TestEqual(TEXT("s_gyro type"),  Gyro->Type, EMjSensorType::Gyro);
    if (JP)   TestEqual(TEXT("s_jp   type"),  JP->Type,   EMjSensorType::JointPos);
    if (Vel)  TestEqual(TEXT("s_vel  type"),  Vel->Type,  EMjSensorType::Velocimeter);

    S.Cleanup();
    return true;
}

// =============================================================================
// URLab.Import.MJ_MocapBody
//   mocap="true" attribute on <body> must set bDrivenByUnreal=true.
//   Verifies Fix 2.9.
// =============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTest_MjImport_MJ_MocapBody,
    "URLab.Import.MJ_MocapBody",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FTest_MjImport_MJ_MocapBody::RunTest(const FString&)
{
    FMjXmlImportSession S;
    if (!S.Init(TEXT(R"(
        <mujoco>
          <worldbody>
            <body name="ref" mocap="true">
              <geom size=".1"/>
            </body>
            <body name="b1">
              <geom size=".1"/>
            </body>
          </worldbody>
        </mujoco>
    )"))) { AddError(S.LastError); return false; }

    UMjBody* RefBody = S.FindTemplate<UMjBody>(TEXT("ref"));
    UMjBody* B1      = S.FindTemplate<UMjBody>(TEXT("b1"));

    if (RefBody) TestTrue(TEXT("ref body bDrivenByUnreal=true"),  RefBody->bDrivenByUnreal);
    if (B1)      TestTrue(TEXT("b1 body bDrivenByUnreal=false"), !B1->bDrivenByUnreal);

    S.Cleanup();
    return true;
}

// =============================================================================
// URLab.Import.MJ_AutoLimits
//   MuJoCo 3.x defaults autolimits="true" — a joint with range= is auto-limited.
//   Explicit limited="false" opts out.
// =============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTest_MjImport_MJ_AutoLimits,
    "URLab.Import.MJ_AutoLimits",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FTest_MjImport_MJ_AutoLimits::RunTest(const FString&)
{
    // Default autolimits: range present → joint IS limited
    FMjTestSession SAuto;
    if (!SAuto.CompileXml(TEXT(R"(
        <mujoco>
          <compiler angle="radian"/>
          <worldbody>
            <body>
              <joint name="j1" type="hinge" range="-1 1"/>
              <geom size=".1"/>
            </body>
          </worldbody>
        </mujoco>
    )"))) { AddError(SAuto.LastError); return false; }
    int jid = SAuto.JointId("j1");
    TestTrue(TEXT("j1 valid"), jid >= 0);
    if (jid >= 0)
        TestTrue(TEXT("j1 IS limited by default autolimits"), SAuto.m->jnt_limited[jid] == 1);
    SAuto.Cleanup();

    // Explicit limited="false" overrides autolimits → joint NOT limited
    FMjTestSession SOverride;
    if (!SOverride.CompileXml(TEXT(R"(
        <mujoco>
          <compiler angle="radian"/>
          <worldbody>
            <body>
              <joint name="j2" type="hinge" range="-1 1" limited="false"/>
              <geom size=".1"/>
            </body>
          </worldbody>
        </mujoco>
    )"))) { AddError(SOverride.LastError); return false; }
    int jid2 = SOverride.JointId("j2");
    TestTrue(TEXT("j2 valid"), jid2 >= 0);
    if (jid2 >= 0)
        TestTrue(TEXT("j2 NOT limited via explicit limited=false"), SOverride.m->jnt_limited[jid2] == 0);
    SOverride.Cleanup();
    return true;
}

// =============================================================================
// URLab.Import.URLab_WeldTorqueScale
//   <weld torquescale="2.5"/> must set TorqueScale=2.5 and bOverride_TorqueScale=true
//   on the imported UMjEquality component.
//   Verifies Fix 3.16 through the URLab importer (not raw MuJoCo API).
// =============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTest_MjImport_URLab_WeldTorqueScale,
    "URLab.Import.URLab_WeldTorqueScale",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FTest_MjImport_URLab_WeldTorqueScale::RunTest(const FString&)
{
    FMjXmlImportSession S;
    if (!S.Init(TEXT(R"(
        <mujoco>
          <worldbody>
            <body name="b1"><geom size=".1"/></body>
            <body name="b2"><geom size=".1"/></body>
          </worldbody>
          <equality>
            <weld name="w1" body1="b1" body2="b2" torquescale="2.5"/>
          </equality>
        </mujoco>
    )"))) { AddError(S.LastError); return false; }

    UMjEquality* Weld = S.FindTemplate<UMjEquality>(TEXT("w1"));
    TestNotNull(TEXT("weld equality 'w1' found"), Weld);
    if (Weld)
    {
        TestTrue(TEXT("bOverride_TorqueScale == true"), Weld->bOverride_TorqueScale);
        TestTrue(TEXT("TorqueScale ≈ 2.5"), FMath::Abs(Weld->TorqueScale - 2.5f) < 1e-4f);
    }
    S.Cleanup();
    return true;
}

// =============================================================================
// URLab.Import.MJ_FrameSensor_ObjType
//   Tier 1: <framepos objtype="body" objname="b1"/> must compile to
//   sensor_objtype == mjOBJ_BODY in the compiled model.
//   Verifies that MuJoCo accepts the objtype+objname pattern for frame sensors.
// =============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTest_MjImport_MJ_FrameSensor_ObjType,
    "URLab.Import.MJ_FrameSensor_ObjType",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FTest_MjImport_MJ_FrameSensor_ObjType::RunTest(const FString&)
{
    FMjTestSession S;
    if (!S.CompileXml(TEXT(R"(
        <mujoco>
          <worldbody>
            <body name="b1">
              <geom size=".1"/>
              <freejoint/>
            </body>
          </worldbody>
          <sensor>
            <framepos  name="fp1" objtype="body" objname="b1"/>
            <framequat name="fq1" objtype="body" objname="b1" reftype="body" refname="world"/>
          </sensor>
        </mujoco>
    )"))) { AddError(S.LastError); return false; }

    int fpId = mj_name2id(S.m, mjOBJ_SENSOR, "fp1");
    TestTrue(TEXT("framepos sensor found"), fpId >= 0);
    if (fpId >= 0)
    {
        TestTrue(TEXT("framepos sensor_objtype == mjOBJ_BODY"),
            S.m->sensor_objtype[fpId] == mjOBJ_BODY);
    }

    int fqId = mj_name2id(S.m, mjOBJ_SENSOR, "fq1");
    TestTrue(TEXT("framequat sensor found"), fqId >= 0);
    if (fqId >= 0)
    {
        TestTrue(TEXT("framequat sensor_objtype == mjOBJ_BODY"),
            S.m->sensor_objtype[fqId] == mjOBJ_BODY);
        TestTrue(TEXT("framequat sensor_reftype == mjOBJ_BODY"),
            S.m->sensor_reftype[fqId] == mjOBJ_BODY);
    }
    S.Cleanup();
    return true;
}

// =============================================================================
// URLab.Import.URLab_FrameSensor_ObjRefType
//   Tier 2: <framepos objtype="body" objname="b1"/> → UMjSensor must have
//   ObjType == Body, ReferenceName populated, and the round-trip compile must
//   produce sensor_objtype == mjOBJ_BODY.
//   Verifies Fix 2.10 (objtype/reftype parsing in ImportFromXml).
// =============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTest_MjImport_URLab_FrameSensor_ObjRefType,
    "URLab.Import.URLab_FrameSensor_ObjRefType",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FTest_MjImport_URLab_FrameSensor_ObjRefType::RunTest(const FString&)
{
    FMjXmlImportSession S;
    if (!S.Init(TEXT(R"(
        <mujoco>
          <worldbody>
            <body name="b1">
              <geom size=".1"/>
              <freejoint/>
              <site name="s1"/>
            </body>
          </worldbody>
          <sensor>
            <framepos  name="fp1" objtype="body" objname="b1"/>
            <framequat name="fq1" objtype="body" objname="b1" reftype="body" refname="world"/>
          </sensor>
        </mujoco>
    )"))) { AddError(S.LastError); return false; }

    // Check UMjSensor component properties
    UMjSensor* FP = S.FindTemplate<UMjSensor>(TEXT("fp1"));
    TestNotNull(TEXT("framepos sensor 'fp1' found"), FP);
    if (FP)
    {
        TestTrue(TEXT("fp1 ObjType == Body"), FP->ObjType == EMjObjType::Body);
        TestTrue(TEXT("fp1 TargetName == 'b1'"), FP->TargetName == TEXT("b1"));
    }

    UMjSensor* FQ = S.FindTemplate<UMjSensor>(TEXT("fq1"));
    TestNotNull(TEXT("framequat sensor 'fq1' found"), FQ);
    if (FQ)
    {
        TestTrue(TEXT("fq1 ObjType == Body"), FQ->ObjType == EMjObjType::Body);
        TestTrue(TEXT("fq1 RefType == Body"), FQ->RefType == EMjObjType::Body);
        TestTrue(TEXT("fq1 ReferenceName == 'world'"), FQ->ReferenceName == TEXT("world"));
    }

    // Round-trip compile: sensors must compile; names are prefixed by the actor name so
    // we cannot look them up by bare "fp1". Instead verify count and objtype by iteration.
    S.Compile();
    if (S.Manager && S.Manager->PhysicsEngine->m_model)
    {
        mjModel* M = S.Manager->PhysicsEngine->m_model;
        // Expect exactly 2 sensors (framepos + framequat)
        TestTrue(TEXT("fp1+fq1 both compile in round-trip"), M->nsensor == 2);
        // At least one sensor should have objtype == mjOBJ_BODY (framepos referencing b1)
        bool bFoundBodyObjType = false;
        for (int i = 0; i < M->nsensor; ++i)
        {
            if (M->sensor_objtype[i] == mjOBJ_BODY)
            {
                bFoundBodyObjType = true;
                break;
            }
        }
        TestTrue(TEXT("fp1 compiled sensor_objtype == mjOBJ_BODY"), bFoundBodyObjType);
    }

    S.Cleanup();
    return true;
}

// =============================================================================
// URLab.Import.DefaultClassJointAxis
//   Joint axis inherited from a <default> class must survive import→compile.
//   The default sets axis="0 1 0"; a joint that inherits (no explicit axis)
//   must compile with jnt_axis=(0,1,0) in MuJoCo, not the builtin (0,0,1).
//   A second joint with an explicit axis="1 0 0" is checked as a control.
// =============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTest_MjImport_DefaultClassJointAxis,
    "URLab.Import.DefaultClassJointAxis",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FTest_MjImport_DefaultClassJointAxis::RunTest(const FString&)
{
    FMjXmlImportSession S;
    if (!S.Init(TEXT(R"(
        <mujoco>
          <default>
            <default class="testclass">
              <joint axis="0 1 0" armature="0.1"/>
            </default>
          </default>
          <worldbody>
            <body childclass="testclass">
              <joint name="inherited" type="hinge" range="-1 1"/>
              <joint name="explicit" type="hinge" axis="1 0 0" range="-1 1"/>
              <geom size=".1"/>
            </body>
          </worldbody>
        </mujoco>
    )"))) { AddError(S.LastError); return false; }

    // Tier 1: check imported UE properties
    UMjJoint* JInherited = S.FindTemplate<UMjJoint>(TEXT("inherited"));
    UMjJoint* JExplicit  = S.FindTemplate<UMjJoint>(TEXT("explicit"));
    TestNotNull(TEXT("inherited joint found"), JInherited);
    TestNotNull(TEXT("explicit joint found"),  JExplicit);

    if (JExplicit)
    {
        // axis="1 0 0" in MuJoCo → UE (1, 0, 0) (only Y negates, X and Z unchanged)
        TestTrue(TEXT("explicit Axis X ≈ 1"),  FMath::Abs(JExplicit->Axis.X - 1.0f) < 1e-4f);
        TestTrue(TEXT("explicit Axis Y ≈ 0"),  FMath::Abs(JExplicit->Axis.Y) < 1e-4f);
        TestTrue(TEXT("explicit Axis Z ≈ 0"),  FMath::Abs(JExplicit->Axis.Z) < 1e-4f);
    }

    // Tier 2: compile and check jnt_axis in the compiled model
    if (!S.Compile()) { AddError(S.LastError); S.Cleanup(); return false; }

    const mjModel* M = S.Model();
    TestNotNull(TEXT("compiled model"), M);
    if (!M) { S.Cleanup(); return false; }

    // Find joints by name in the compiled model
    int idInherited = mj_name2id(M, mjOBJ_JOINT, "inherited");
    int idExplicit  = mj_name2id(M, mjOBJ_JOINT, "explicit");

    // Joints may be prefixed — search with prefix if not found
    if (idInherited < 0 || idExplicit < 0)
    {
        for (int j = 0; j < M->njnt; ++j)
        {
            const char* name = mj_id2name(M, mjOBJ_JOINT, j);
            if (!name) continue;
            FString N = UTF8_TO_TCHAR(name);
            if (N.Contains(TEXT("inherited"))) idInherited = j;
            if (N.Contains(TEXT("explicit")))  idExplicit  = j;
        }
    }

    TestTrue(TEXT("inherited joint compiled"), idInherited >= 0);
    TestTrue(TEXT("explicit joint compiled"),  idExplicit >= 0);

    if (idInherited >= 0)
    {
        const mjtNum* ax = &M->jnt_axis[idInherited * 3];
        TestTrue(TEXT("inherited jnt_axis[0] ≈ 0"), FMath::Abs((float)ax[0]) < 1e-4f);
        TestTrue(TEXT("inherited jnt_axis[1] ≈ 1"), FMath::Abs((float)ax[1] - 1.0f) < 1e-4f);
        TestTrue(TEXT("inherited jnt_axis[2] ≈ 0"), FMath::Abs((float)ax[2]) < 1e-4f);
    }

    if (idExplicit >= 0)
    {
        const mjtNum* ax = &M->jnt_axis[idExplicit * 3];
        TestTrue(TEXT("explicit jnt_axis[0] ≈ 1"), FMath::Abs((float)ax[0] - 1.0f) < 1e-4f);
        TestTrue(TEXT("explicit jnt_axis[1] ≈ 0"), FMath::Abs((float)ax[1]) < 1e-4f);
        TestTrue(TEXT("explicit jnt_axis[2] ≈ 0"), FMath::Abs((float)ax[2]) < 1e-4f);
    }

    S.Cleanup();
    return true;
}

// =============================================================================
// URLab.Import.DefaultClass_JointName_Collision
//   Regression for MJCFs that share a label between a <default class="X"> and
//   a <joint name="X"> — the idiomatic Menagerie pattern for per-joint tuning
//   (vx300s, wx250s, aloha, ...). Before MjName was populated from the XML
//   name= attribute, the SCS uniqueness rule forced the joint's UE variable
//   name to "X1" while the actuator's joint="X" reference kept the raw label,
//   so MuJoCo's compiler dropped every actuator silently (nu == 0).
//   Compare URLab's nu against MuJoCo's baseline nu for the same XML; they
//   must match.
// =============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTest_MjImport_DefaultClassJointNameCollision,
    "URLab.Import.DefaultClass_JointName_Collision",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FTest_MjImport_DefaultClassJointNameCollision::RunTest(const FString&)
{
    static const TCHAR* Xml = TEXT(R"(
        <mujoco>
          <default>
            <default class="hip">
              <joint damping="5"/>
              <position kp="100"/>
            </default>
          </default>
          <worldbody>
            <body>
              <joint name="hip" class="hip" type="hinge"/>
              <geom size=".1"/>
            </body>
          </worldbody>
          <actuator>
            <position class="hip" name="hip" joint="hip"/>
          </actuator>
        </mujoco>
    )");

    // Baseline: what MuJoCo itself produces from this XML.
    FMjTestSession Ref;
    if (!Ref.CompileXml(Xml)) { AddError(Ref.LastError); return false; }
    const int ExpNu = Ref.m->nu;
    const int ExpNjnt = Ref.m->njnt;
    Ref.Cleanup();
    TestEqual(TEXT("baseline MuJoCo nu == 1"), ExpNu, 1);
    TestEqual(TEXT("baseline MuJoCo njnt == 1"), ExpNjnt, 1);

    // Through URLab's importer + compile.
    FMjXmlImportSession S;
    if (!S.Init(Xml))  { AddError(S.LastError); return false; }
    if (!S.Compile())  { AddError(S.LastError); S.Cleanup(); return false; }

    TestEqual(TEXT("URLab nu matches MuJoCo baseline (actuator survived Default-class name collision)"),
        (int)S.Model()->nu, ExpNu);
    TestEqual(TEXT("URLab njnt matches MuJoCo baseline"),
        (int)S.Model()->njnt, ExpNjnt);

    S.Cleanup();
    return true;
}
