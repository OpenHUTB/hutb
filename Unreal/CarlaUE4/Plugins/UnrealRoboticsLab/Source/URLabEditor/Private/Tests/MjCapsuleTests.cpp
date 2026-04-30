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
#include "MuJoCo/Components/Geometry/Primitives/MjCapsule.h"
#include "mujoco/mujoco.h"

// ============================================================================
// URLab.Capsule.Import_SizeForm_CreatesUMjCapsule
//   <geom type="capsule" size="radius halflength"> → UMjCapsule with matching
//   Radius/HalfLength and parent scale set to cm-convention (X=Y=2R, Z=2H).
// ============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMjCapsuleImportSizeForm,
    "URLab.Capsule.Import_SizeForm_CreatesUMjCapsule",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMjCapsuleImportSizeForm::RunTest(const FString& Parameters)
{
    const TCHAR* Xml = TEXT(R"(<mujoco>
  <worldbody>
    <body name="arm" pos="0 0 1">
      <geom name="upper" type="capsule" size="0.05 0.15" pos="0 0 0"/>
      <joint name="j" type="hinge" axis="0 1 0"/>
    </body>
  </worldbody>
</mujoco>)");

    FMjXmlImportSession S;
    if (!S.Init(Xml))
    {
        AddError(FString::Printf(TEXT("Init failed: %s"), *S.LastError));
        return false;
    }

    UMjCapsule* Cap = S.FindTemplate<UMjCapsule>(TEXT("upper"));
    if (!TestNotNull(TEXT("UMjCapsule 'upper'"), Cap)) return false;

    TestEqual(TEXT("Radius"),     Cap->Radius,     0.05f);
    TestEqual(TEXT("HalfLength"), Cap->HalfLength, 0.15f);

    // Parent scale should map radius → 2R (cm-to-UE), halflength → 2H.
    const FVector Scale = Cap->GetRelativeScale3D();
    TestTrue(TEXT("Scale.X ≈ 0.1"), FMath::IsNearlyEqual(Scale.X, 0.1f, 1e-4f));
    TestTrue(TEXT("Scale.Y ≈ 0.1"), FMath::IsNearlyEqual(Scale.Y, 0.1f, 1e-4f));
    TestTrue(TEXT("Scale.Z ≈ 0.3"), FMath::IsNearlyEqual(Scale.Z, 0.3f, 1e-4f));

    return true;
}

// ============================================================================
// URLab.Capsule.Import_FromToForm_ResolvedByBase
//   <geom type="capsule" fromto="...">. Base UMjGeom resolves fromto into
//   pos/quat/size[1]; the capsule subclass just reads the resolved values.
//   HalfLength should equal |B-A| / 2.
// ============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMjCapsuleImportFromTo,
    "URLab.Capsule.Import_FromToForm_ResolvedByBase",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMjCapsuleImportFromTo::RunTest(const FString& Parameters)
{
    // Endpoints 0.3 m apart along +Z → halflength 0.15 m. Radius from size[0].
    const TCHAR* Xml = TEXT(R"(<mujoco>
  <worldbody>
    <body name="arm" pos="0 0 1">
      <geom name="upper" type="capsule" size="0.05" fromto="0 0 -0.15 0 0 0.15"/>
      <joint name="j" type="hinge" axis="0 1 0"/>
    </body>
  </worldbody>
</mujoco>)");

    FMjXmlImportSession S;
    if (!S.Init(Xml))
    {
        AddError(FString::Printf(TEXT("Init failed: %s"), *S.LastError));
        return false;
    }

    UMjCapsule* Cap = S.FindTemplate<UMjCapsule>(TEXT("upper"));
    if (!TestNotNull(TEXT("UMjCapsule 'upper'"), Cap)) return false;

    TestEqual(TEXT("Radius (from size)"),       Cap->Radius,     0.05f);
    TestTrue (TEXT("HalfLength resolved to ~0.15"),
              FMath::IsNearlyEqual(Cap->HalfLength, 0.15f, 1e-4f));
    return true;
}

// ============================================================================
// URLab.Capsule.Compile_ProducesCapsuleGeom
//   A compiled model should report geom type mjGEOM_CAPSULE, the intended
//   radius, and matching half-length, proving we're round-tripping the shape
//   through the URLab pipeline rather than silently dropping it.
// ============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMjCapsuleCompile,
    "URLab.Capsule.Compile_ProducesCapsuleGeom",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMjCapsuleCompile::RunTest(const FString& Parameters)
{
    const TCHAR* Xml = TEXT(R"(<mujoco>
  <worldbody>
    <body name="arm" pos="0 0 1">
      <geom name="upper" type="capsule" size="0.05 0.15"/>
      <joint name="j" type="hinge" axis="0 1 0"/>
    </body>
  </worldbody>
</mujoco>)");

    FMjXmlImportSession S;
    if (!S.Init(Xml))        { AddError(S.LastError); return false; }
    if (!S.Compile())        { AddError(S.LastError); return false; }

    const mjModel* M = S.Model();
    if (!TestNotNull(TEXT("compiled model"), (void*)M)) return false;

    // Locate the capsule geom — the plugin may prefix names with the actor.
    int GeomId = -1;
    for (int i = 0; i < M->ngeom; ++i)
    {
        if (M->geom_type[i] == mjGEOM_CAPSULE) { GeomId = i; break; }
    }
    if (!TestTrue(TEXT("capsule geom present in model"), GeomId >= 0))
    {
        return false;
    }

    TestTrue(TEXT("radius ≈ 0.05"),
        FMath::IsNearlyEqual((float)M->geom_size[GeomId * 3 + 0], 0.05f, 1e-4f));
    TestTrue(TEXT("halflength ≈ 0.15"),
        FMath::IsNearlyEqual((float)M->geom_size[GeomId * 3 + 1], 0.15f, 1e-4f));
    return true;
}
