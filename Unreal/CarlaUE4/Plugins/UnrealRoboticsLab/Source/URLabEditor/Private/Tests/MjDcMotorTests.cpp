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
#include "MuJoCo/Components/Actuators/MjDcMotorActuator.h"
#include "mujoco/mujoco.h"

namespace
{
    // Based on MuJoCo upstream test/engine/testdata/derivative/dcmotor.xml (3.7.0).
    // Covers the range of dcmotor configurations used in the reference test.
    const TCHAR* kDcMotorArmXml = TEXT(R"(<mujoco>
  <worldbody>
    <body name="motor1" pos="0 0.1 0">
      <joint name="slide1" type="slide" axis="0 0 1"/>
      <geom size=".03"/>
    </body>
    <body name="motor4" pos="0 0.4 0">
      <joint name="joint4"/>
      <geom size=".03"/>
    </body>
  </worldbody>

  <actuator>
    <dcmotor name="dc_bias" joint="slide1" motorconst="2.0" resistance="0.5"/>
    <dcmotor name="dc_lugre" joint="joint4" motorconst="0.05" resistance="2.0"
             damping="0.001" lugre="1e4 100 0.005 0.008 0.1"/>
  </actuator>
</mujoco>)");
}

// ============================================================================
// URLab.DcMotor.Import_CreatesDcMotorComponents
//   The XML parser maps <dcmotor> elements onto UMjDcMotorActuator components.
// ============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMjDcMotorImportCreatesComponents,
    "URLab.DcMotor.Import_CreatesDcMotorComponents",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMjDcMotorImportCreatesComponents::RunTest(const FString& Parameters)
{
    FMjXmlImportSession S;
    if (!S.Init(kDcMotorArmXml))
    {
        AddError(FString::Printf(TEXT("Init failed: %s"), *S.LastError));
        return false;
    }

    UMjDcMotorActuator* DcBias  = S.FindTemplate<UMjDcMotorActuator>(TEXT("dc_bias"));
    UMjDcMotorActuator* DcLugre = S.FindTemplate<UMjDcMotorActuator>(TEXT("dc_lugre"));

    TestNotNull(TEXT("dc_bias component"),  DcBias);
    TestNotNull(TEXT("dc_lugre component"), DcLugre);

    if (DcBias)
    {
        TestTrue(TEXT("dc_bias: Type == DcMotor"), DcBias->Type == EMjActuatorType::DcMotor);
        TestTrue(TEXT("dc_bias: motorconst overridden"), DcBias->bOverride_MotorConst);
        if (DcBias->bOverride_MotorConst && DcBias->MotorConst.Num() >= 1)
        {
            TestEqual(TEXT("dc_bias: motorconst[0]"), DcBias->MotorConst[0], 2.0f);
        }
        TestTrue(TEXT("dc_bias: resistance overridden"), DcBias->bOverride_Resistance);
        TestEqual(TEXT("dc_bias: resistance"), DcBias->Resistance, 0.5f);
    }

    if (DcLugre)
    {
        TestTrue(TEXT("dc_lugre: lugre overridden"), DcLugre->bOverride_LuGre);
        TestEqual(TEXT("dc_lugre: lugre length"), DcLugre->LuGre.Num(), 5);
        if (DcLugre->LuGre.Num() == 5)
        {
            TestEqual(TEXT("dc_lugre: lugre[0]"), DcLugre->LuGre[0], 1.0e4f);
            TestEqual(TEXT("dc_lugre: lugre[4]"), DcLugre->LuGre[4], 0.1f);
        }
    }

    return true;
}

// ============================================================================
// URLab.DcMotor.Compile_MatchesNativeActuatorCount
//   Compiling the imported articulation via Manager->Compile() should produce
//   a model with 2 actuators, each with gaintype mjGAIN_DCMOTOR.
// ============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMjDcMotorCompile,
    "URLab.DcMotor.Compile_MatchesNativeActuatorCount",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMjDcMotorCompile::RunTest(const FString& Parameters)
{
    FMjXmlImportSession S;
    if (!S.Init(kDcMotorArmXml))
    {
        AddError(FString::Printf(TEXT("Init failed: %s"), *S.LastError));
        return false;
    }
    if (!S.Compile())
    {
        AddError(FString::Printf(TEXT("Compile failed: %s"), *S.LastError));
        return false;
    }

    const mjModel* M = S.Model();
    if (!TestNotNull(TEXT("compiled model"), (void*)M)) return false;

    TestEqual(TEXT("nu (actuator count)"), (int32)M->nu, (int32)2);
    for (int i = 0; i < M->nu; ++i)
    {
        TestEqual(FString::Printf(TEXT("actuator %d gaintype"), i),
            (int32)M->actuator_gaintype[i], (int32)mjGAIN_DCMOTOR);
        TestEqual(FString::Printf(TEXT("actuator %d biastype"), i),
            (int32)M->actuator_biastype[i], (int32)mjBIAS_DCMOTOR);
        TestEqual(FString::Printf(TEXT("actuator %d dyntype"), i),
            (int32)M->actuator_dyntype[i], (int32)mjDYN_DCMOTOR);
    }
    return true;
}
