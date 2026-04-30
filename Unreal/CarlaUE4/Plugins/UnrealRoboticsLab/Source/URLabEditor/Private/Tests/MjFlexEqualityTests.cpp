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
#include "MuJoCo/Components/Constraints/MjEquality.h"

namespace
{
    // Minimal MJCF that compiles: one free body with a capsule geom, plus a
    // flexcomp that produces a real <flex> asset. The generated flex is named
    // "cloth" because flexcomp's name becomes the flex name when the flex
    // section is absent. The flexcomp is authored with edge equality off so
    // we can add our own explicit <equality><flex ... /> entries.
    FString MakeFlexEqualityXml(const FString& EqualityChildTag)
    {
        // Trilinear flexes can't self-collide, so disable via the flexcomp
        // contact block. The equality on the flex is the whole point of the
        // test — flexcomp's own edge equality is disabled so we're the
        // single source of a flex-referencing equality in the spec.
        return FString::Printf(TEXT(R"(<mujoco>
  <worldbody>
    <body name="anchor" pos="0 0 0">
      <geom size=".05"/>
    </body>
    <flexcomp name="cloth" type="grid" count="2 2 1" spacing="0.1 0.1 0.1"
              pos="0 0 0.3" dof="trilinear">
      <contact selfcollide="none"/>
      <edge equality="false"/>
    </flexcomp>
  </worldbody>

  <equality>
    <%s flex="cloth" active="true"/>
  </equality>
</mujoco>)"), *EqualityChildTag);
    }
}

// Parametrised helper: run the parser on a flex-equality child tag and verify
// the generated UMjEquality uses the expected enum value.
static bool CheckFlexEqualityVariant(FAutomationTestBase& Tester,
                                     const FString& Tag,
                                     EMjEqualityType Expected)
{
    FMjXmlImportSession S;
    const FString Xml = MakeFlexEqualityXml(Tag);
    if (!S.Init(Xml))
    {
        Tester.AddError(FString::Printf(TEXT("Init failed (%s): %s"), *Tag, *S.LastError));
        return false;
    }

    UMjEquality* Eq = S.FindFirstTemplate<UMjEquality>();
    if (!Tester.TestNotNull(TEXT("equality component"), Eq))
    {
        return false;
    }

    Tester.TestTrue(FString::Printf(TEXT("<%s> → EqualityType matches"), *Tag),
        Eq->EqualityType == Expected);
    Tester.TestEqual(FString::Printf(TEXT("<%s>: flex attribute captured as Obj1"), *Tag),
        Eq->Obj1, FString(TEXT("cloth")));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMjEqualityFlex,
    "URLab.Equality.Flex",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FMjEqualityFlex::RunTest(const FString&)
{
    return CheckFlexEqualityVariant(*this, TEXT("flex"), EMjEqualityType::Flex);
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMjEqualityFlexVert,
    "URLab.Equality.FlexVert",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FMjEqualityFlexVert::RunTest(const FString&)
{
    return CheckFlexEqualityVariant(*this, TEXT("flexvert"), EMjEqualityType::FlexVert);
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMjEqualityFlexStrain,
    "URLab.Equality.FlexStrain",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FMjEqualityFlexStrain::RunTest(const FString&)
{
    return CheckFlexEqualityVariant(*this, TEXT("flexstrain"), EMjEqualityType::FlexStrain);
}
