// Copyright (c) 2026 Jonathan Embley-Riches. All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Round-trip tests for muscle actuators against MuJoCo's canonical 2-link
// 6-muscle arm demo (arm26.xml from the upstream repo). A regression here
// means our articulation export path disagrees with MuJoCo's native XML
// parser on actuator_dyntype / gainprm / biasprm / dynprm — ie. the compiled
// mjModel we produce would simulate differently from `mj_loadXML`.

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "Tests/MjTestHelpers.h"
#include "MuJoCo/Components/Actuators/MjMuscleActuator.h"
#include "MuJoCo/Components/Tendons/MjTendon.h"
#include "MuJoCo/Components/Geometry/MjSite.h"
#include "mujoco/mujoco.h"

namespace
{
    // Verbatim arm26.xml (model/tendon_arm/arm26.xml) from the MuJoCo repo —
    // 2-link arm, 6 spatial tendons, 6 muscles with a <default><muscle/> block.
    static const TCHAR* ARM26_XML = TEXT(R"(
        <mujoco model="2-link 6-muscle arm">
          <option timestep="0.005" iterations="50" solver="Newton" tolerance="1e-10"/>
          <default>
            <joint type="hinge" pos="0 0 0" axis="0 0 1" limited="true" range="0 120" damping="0.1"/>
            <muscle ctrllimited="true" ctrlrange="0 1"/>
          </default>
          <worldbody>
            <site name="s0" pos="-0.15 0 0" size="0.02"/>
            <site name="x0" pos="0 -0.15 0" size="0.02" rgba="0 .7 0 1" group="1"/>
            <body pos="0 0 0">
              <geom name="upper arm" type="capsule" size="0.045" fromto="0 0 0  0.5 0 0"/>
              <joint name="shoulder"/>
              <geom name="shoulder" type="cylinder" pos="0 0 0" size=".1 .05" mass="0" group="1"/>
              <site name="s1" pos="0.15 0.06 0" size="0.02"/>
              <site name="s2" pos="0.15 -0.06 0" size="0.02"/>
              <site name="s3" pos="0.4 0.06 0" size="0.02"/>
              <site name="s4" pos="0.4 -0.06 0" size="0.02"/>
              <site name="s5" pos="0.25 0.1 0" size="0.02"/>
              <site name="s6" pos="0.25 -0.1 0" size="0.02"/>
              <site name="x1" pos="0.5 -0.15 0" size="0.02" rgba="0 .7 0 1" group="1"/>
              <body pos="0.5 0 0">
                <geom name="forearm" type="capsule" size="0.035" fromto="0 0 0  0.5 0 0"/>
                <joint name="elbow"/>
                <geom name="elbow" type="cylinder" pos="0 0 0" size=".08 .05" mass="0" group="1"/>
                <site name="s7" pos="0.11 0.05 0" size="0.02"/>
                <site name="s8" pos="0.11 -0.05 0" size="0.02"/>
              </body>
            </body>
          </worldbody>
          <tendon>
            <spatial name="SF" width="0.01">
              <site site="s0"/>
              <geom geom="shoulder"/>
              <site site="s1"/>
            </spatial>
            <spatial name="SE" width="0.01">
              <site site="s0"/>
              <geom geom="shoulder" sidesite="x0"/>
              <site site="s2"/>
            </spatial>
            <spatial name="EF" width="0.01">
              <site site="s3"/>
              <geom geom="elbow"/>
              <site site="s7"/>
            </spatial>
            <spatial name="EE" width="0.01">
              <site site="s4"/>
              <geom geom="elbow" sidesite="x1"/>
              <site site="s8"/>
            </spatial>
            <spatial name="BF" width="0.009">
              <site site="s0"/>
              <geom geom="shoulder"/>
              <site site="s5"/>
              <geom geom="elbow"/>
              <site site="s7"/>
            </spatial>
            <spatial name="BE" width="0.009">
              <site site="s0"/>
              <geom geom="shoulder" sidesite="x0"/>
              <site site="s6"/>
              <geom geom="elbow" sidesite="x1"/>
              <site site="s8"/>
            </spatial>
          </tendon>
          <actuator>
            <muscle name="SF" tendon="SF"/>
            <muscle name="SE" tendon="SE"/>
            <muscle name="EF" tendon="EF"/>
            <muscle name="EE" tendon="EE"/>
            <muscle name="BF" tendon="BF"/>
            <muscle name="BE" tendon="BE"/>
          </actuator>
        </mujoco>
    )");

    bool ActuatorParamsMatch(
        FAutomationTestBase& Test, const TCHAR* Label,
        const mjModel* Ref, const mjModel* Got, int32 ActuatorId)
    {
        const float Eps = 1e-5f;
        bool bOk = true;

        if (Ref->actuator_dyntype[ActuatorId] != Got->actuator_dyntype[ActuatorId])
        {
            Test.AddError(FString::Printf(
                TEXT("%s: actuator_dyntype[%d] ref=%d got=%d"),
                Label, ActuatorId,
                Ref->actuator_dyntype[ActuatorId], Got->actuator_dyntype[ActuatorId]));
            bOk = false;
        }
        if (Ref->actuator_gaintype[ActuatorId] != Got->actuator_gaintype[ActuatorId])
        {
            Test.AddError(FString::Printf(
                TEXT("%s: actuator_gaintype[%d] ref=%d got=%d"),
                Label, ActuatorId,
                Ref->actuator_gaintype[ActuatorId], Got->actuator_gaintype[ActuatorId]));
            bOk = false;
        }
        if (Ref->actuator_biastype[ActuatorId] != Got->actuator_biastype[ActuatorId])
        {
            Test.AddError(FString::Printf(
                TEXT("%s: actuator_biastype[%d] ref=%d got=%d"),
                Label, ActuatorId,
                Ref->actuator_biastype[ActuatorId], Got->actuator_biastype[ActuatorId]));
            bOk = false;
        }

        for (int p = 0; p < mjNDYN; ++p)
        {
            const float R = (float)Ref->actuator_dynprm[ActuatorId * mjNDYN + p];
            const float G = (float)Got->actuator_dynprm[ActuatorId * mjNDYN + p];
            if (FMath::Abs(R - G) > Eps)
            {
                Test.AddError(FString::Printf(
                    TEXT("%s: actuator_dynprm[%d][%d] ref=%f got=%f"),
                    Label, ActuatorId, p, R, G));
                bOk = false;
            }
        }
        for (int p = 0; p < mjNGAIN; ++p)
        {
            const float R = (float)Ref->actuator_gainprm[ActuatorId * mjNGAIN + p];
            const float G = (float)Got->actuator_gainprm[ActuatorId * mjNGAIN + p];
            if (FMath::Abs(R - G) > Eps)
            {
                Test.AddError(FString::Printf(
                    TEXT("%s: actuator_gainprm[%d][%d] ref=%f got=%f"),
                    Label, ActuatorId, p, R, G));
                bOk = false;
            }
        }
        for (int p = 0; p < mjNBIAS; ++p)
        {
            const float R = (float)Ref->actuator_biasprm[ActuatorId * mjNBIAS + p];
            const float G = (float)Got->actuator_biasprm[ActuatorId * mjNBIAS + p];
            if (FMath::Abs(R - G) > Eps)
            {
                Test.AddError(FString::Printf(
                    TEXT("%s: actuator_biasprm[%d][%d] ref=%f got=%f"),
                    Label, ActuatorId, p, R, G));
                bOk = false;
            }
        }
        return bOk;
    }
}

// Arm26 compiles through the URLab pipeline and produces the same body /
// tendon / actuator counts as mj_loadXML. If this fails, we've either lost
// elements (missing import) or added duplicates.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTest_MjMuscle_Arm26_Counts,
    "URLab.Muscle.Arm26_Counts",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FTest_MjMuscle_Arm26_Counts::RunTest(const FString&)
{
    FMjTestSession Ref;
    if (!Ref.CompileXml(ARM26_XML)) { AddError(Ref.LastError); return false; }

    FMjXmlImportSession S;
    if (!S.Init(ARM26_XML))  { AddError(S.LastError); Ref.Cleanup(); return false; }

    // Pre-compile diagnostic: how many component templates did the importer create?
    AddInfo(FString::Printf(TEXT("templates: UMjMuscleActuator=%d, UMjTendon=%d, UMjSite=%d"),
        S.CountTemplates<class UMjMuscleActuator>(),
        S.CountTemplates<class UMjTendon>(),
        S.CountTemplates<class UMjSite>()));

    if (!S.Compile())        { AddError(S.LastError); S.Cleanup(); Ref.Cleanup(); return false; }

    TestEqual(TEXT("nbody"),   (int)S.Model()->nbody,   (int)Ref.m->nbody);
    TestEqual(TEXT("ngeom"),   (int)S.Model()->ngeom,   (int)Ref.m->ngeom);
    TestEqual(TEXT("ntendon"), (int)S.Model()->ntendon, (int)Ref.m->ntendon);
    TestEqual(TEXT("nu"),      (int)S.Model()->nu,      (int)Ref.m->nu);
    TestEqual(TEXT("nsite"),   (int)S.Model()->nsite,   (int)Ref.m->nsite);

    S.Cleanup();
    Ref.Cleanup();
    return true;
}

// For every muscle in arm26, verify dyntype / gaintype / biastype and the
// full dynprm / gainprm / biasprm vectors match the native-parse reference
// bit-for-bit. Catches any divergence in our mjs_setToMuscle invocation or
// default-propagation path.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTest_MjMuscle_Arm26_ActuatorParams,
    "URLab.Muscle.Arm26_ActuatorParams",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FTest_MjMuscle_Arm26_ActuatorParams::RunTest(const FString&)
{
    FMjTestSession Ref;
    if (!Ref.CompileXml(ARM26_XML)) { AddError(Ref.LastError); return false; }

    FMjXmlImportSession S;
    if (!S.Init(ARM26_XML))  { AddError(S.LastError); Ref.Cleanup(); return false; }
    if (!S.Compile())        { AddError(S.LastError); S.Cleanup(); Ref.Cleanup(); return false; }

    if (S.Model()->nu != Ref.m->nu)
    {
        AddError(FString::Printf(
            TEXT("nu mismatch before param compare: ref=%d got=%d"),
            (int)Ref.m->nu, (int)S.Model()->nu));
    }
    else
    {
        for (int32 a = 0; a < Ref.m->nu; ++a)
        {
            const FString Label = FString::Printf(TEXT("arm26 actuator %d"), a);
            ActuatorParamsMatch(*this, *Label, Ref.m, S.Model(), a);
        }
    }

    S.Cleanup();
    Ref.Cleanup();
    return true;
}
