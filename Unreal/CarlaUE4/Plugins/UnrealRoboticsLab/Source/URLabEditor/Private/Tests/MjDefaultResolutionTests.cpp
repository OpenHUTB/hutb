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
#include "MjTestHelpers.h"
#include "MuJoCo/Components/Defaults/MjDefault.h"
#include "MuJoCo/Components/Bodies/MjBody.h"
#include "MuJoCo/Components/Joints/MjJoint.h"
#include "MuJoCo/Components/Geometry/MjGeom.h"

// ============================================================================
// URLab.DefaultRes.LocalOverrideWins
//   When bOverride_Axis is true on the joint, GetResolvedAxis returns it directly.
// ============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMjDefResLocalOverride,
    "URLab.DefaultRes.LocalOverrideWins",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMjDefResLocalOverride::RunTest(const FString& Parameters)
{
    FMjUESession S;
    bool bOk = S.Init([](FMjUESession& Ses)
    {
        Ses.Joint->bOverride_Axis = true;
        Ses.Joint->Axis = FVector(1, 0, 0);
        Ses.Joint->bOverride_Type = true;
        Ses.Joint->Type = EMjJointType::Slide;
    });
    if (!TestTrue(TEXT("Init"), bOk)) return false;

    // Before bind, test editor-time resolution
    // (We test after compile here, but the local override path doesn't use JointView)
    // Create a fresh joint to test editor-only path
    UMjJoint* EditorJoint = NewObject<UMjJoint>(S.Robot, TEXT("EditorJoint"));
    EditorJoint->RegisterComponent();
    EditorJoint->AttachToComponent(S.Body, FAttachmentTransformRules::KeepRelativeTransform);
    EditorJoint->bOverride_Axis = true;
    EditorJoint->Axis = FVector(0, 1, 0);

    FVector Resolved = EditorJoint->GetResolvedAxis();
    TestTrue(TEXT("Local override X"), FMath::Abs(Resolved.X) < 1e-4f);
    TestTrue(TEXT("Local override Y"), FMath::Abs(Resolved.Y - 1.0f) < 1e-4f);
    TestTrue(TEXT("Local override Z"), FMath::Abs(Resolved.Z) < 1e-4f);

    S.Cleanup();
    return true;
}

// ============================================================================
// URLab.DefaultRes.FallbackToBuiltin
//   With no overrides and no defaults, returns MuJoCo builtins.
// ============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMjDefResFallbackBuiltin,
    "URLab.DefaultRes.FallbackToBuiltin",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMjDefResFallbackBuiltin::RunTest(const FString& Parameters)
{
    FMjUESession S;
    bool bOk = S.Init();
    if (!TestTrue(TEXT("Init"), bOk)) return false;

    // Create a fresh unbound joint with no overrides
    UMjJoint* J = NewObject<UMjJoint>(S.Robot, TEXT("PlainJoint"));
    J->RegisterComponent();
    J->AttachToComponent(S.Body, FAttachmentTransformRules::KeepRelativeTransform);

    // No overrides, no MjClassName, no body ChildClassName → builtins
    TestTrue(TEXT("Axis Z-up"), J->GetResolvedAxis().Equals(FVector(0, 0, 1), 1e-4f));
    TestEqual(TEXT("Type Hinge"), J->GetResolvedType(), EMjJointType::Hinge);
    TestFalse(TEXT("Not limited"), J->GetResolvedLimited());
    TestTrue(TEXT("Range (0,0)"), J->GetResolvedRange().Equals(FVector2D(0, 0), 1e-4f));

    S.Cleanup();
    return true;
}

// ============================================================================
// URLab.DefaultRes.DefaultClassResolvesAxis
//   Joint with MjClassName resolves axis from a UMjDefault's child UMjJoint.
// ============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMjDefResDefaultClass,
    "URLab.DefaultRes.DefaultClassResolvesAxis",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMjDefResDefaultClass::RunTest(const FString& Parameters)
{
    FMjUESession S;
    bool bOk = S.Init([](FMjUESession& Ses)
    {
        // Create a default class "arm" with axis=(1,0,0)
        UMjDefault* Def = NewObject<UMjDefault>(Ses.Robot, TEXT("DefArm"));
        Def->ClassName = TEXT("arm");
        Def->bIsDefault = true;
        Def->RegisterComponent();
        Def->AttachToComponent(Ses.Robot->GetRootComponent(), FAttachmentTransformRules::KeepRelativeTransform);

        UMjJoint* DefJoint = NewObject<UMjJoint>(Ses.Robot, TEXT("DefArmJoint"));
        DefJoint->bIsDefault = true;
        DefJoint->bOverride_Axis = true;
        DefJoint->Axis = FVector(1, 0, 0);
        DefJoint->RegisterComponent();
        DefJoint->AttachToComponent(Def, FAttachmentTransformRules::KeepRelativeTransform);

        // Set the test joint to use class "arm"
        Ses.Joint->MjClassName = TEXT("arm");
    });
    if (!TestTrue(TEXT("Init"), bOk)) return false;

    // Create unbound joint with MjClassName="arm" to test editor-time path
    UMjJoint* EdJ = NewObject<UMjJoint>(S.Robot, TEXT("EdJoint"));
    EdJ->RegisterComponent();
    EdJ->AttachToComponent(S.Body, FAttachmentTransformRules::KeepRelativeTransform);
    EdJ->MjClassName = TEXT("arm");

    FVector Axis = EdJ->GetResolvedAxis();
    TestTrue(TEXT("Resolved axis X from default"), FMath::Abs(Axis.X - 1.0f) < 1e-4f);
    TestTrue(TEXT("Resolved axis Y"), FMath::Abs(Axis.Y) < 1e-4f);
    TestTrue(TEXT("Resolved axis Z"), FMath::Abs(Axis.Z) < 1e-4f);

    S.Cleanup();
    return true;
}

// ============================================================================
// URLab.DefaultRes.ParentChainWalk
//   Default "child" inherits from "parent". Joint gets axis from parent chain.
// ============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMjDefResParentChain,
    "URLab.DefaultRes.ParentChainWalk",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMjDefResParentChain::RunTest(const FString& Parameters)
{
    FMjUESession S;
    bool bOk = S.Init([](FMjUESession& Ses)
    {
        // Parent default "base" with axis=(0,1,0)
        UMjDefault* DefBase = NewObject<UMjDefault>(Ses.Robot, TEXT("DefBase"));
        DefBase->ClassName = TEXT("base");
        DefBase->bIsDefault = true;
        DefBase->RegisterComponent();
        DefBase->AttachToComponent(Ses.Robot->GetRootComponent(), FAttachmentTransformRules::KeepRelativeTransform);

        UMjJoint* BaseJoint = NewObject<UMjJoint>(Ses.Robot, TEXT("DefBaseJoint"));
        BaseJoint->bIsDefault = true;
        BaseJoint->bOverride_Axis = true;
        BaseJoint->Axis = FVector(0, 1, 0);
        BaseJoint->RegisterComponent();
        BaseJoint->AttachToComponent(DefBase, FAttachmentTransformRules::KeepRelativeTransform);

        // Child default "derived" inherits from "base", overrides range but NOT axis
        UMjDefault* DefDerived = NewObject<UMjDefault>(Ses.Robot, TEXT("DefDerived"));
        DefDerived->ClassName = TEXT("derived");
        DefDerived->ParentClassName = TEXT("base");
        DefDerived->bIsDefault = true;
        DefDerived->RegisterComponent();
        DefDerived->AttachToComponent(Ses.Robot->GetRootComponent(), FAttachmentTransformRules::KeepRelativeTransform);

        UMjJoint* DerivedJoint = NewObject<UMjJoint>(Ses.Robot, TEXT("DefDerivedJoint"));
        DerivedJoint->bIsDefault = true;
        DerivedJoint->bOverride_Range = true;
        DerivedJoint->Range = {-1.57f, 1.57f};
        DerivedJoint->bOverride_Limited = true;
        DerivedJoint->bLimited = true;
        DerivedJoint->RegisterComponent();
        DerivedJoint->AttachToComponent(DefDerived, FAttachmentTransformRules::KeepRelativeTransform);
    });
    if (!TestTrue(TEXT("Init"), bOk)) return false;

    // Test unbound joint with class "derived"
    UMjJoint* EdJ = NewObject<UMjJoint>(S.Robot, TEXT("ChainJoint"));
    EdJ->RegisterComponent();
    EdJ->AttachToComponent(S.Body, FAttachmentTransformRules::KeepRelativeTransform);
    EdJ->MjClassName = TEXT("derived");

    // Axis should come from parent "base" (derived doesn't override axis)
    FVector Axis = EdJ->GetResolvedAxis();
    TestTrue(TEXT("Axis from parent: Y=1"), FMath::Abs(Axis.Y - 1.0f) < 1e-4f);

    // Range should come from "derived" directly
    FVector2D Range = EdJ->GetResolvedRange();
    TestTrue(TEXT("Range min from derived"), FMath::Abs(Range.X - (-1.57f)) < 1e-3f);
    TestTrue(TEXT("Range max from derived"), FMath::Abs(Range.Y - 1.57f) < 1e-3f);

    // Limited should come from "derived"
    TestTrue(TEXT("Limited from derived"), EdJ->GetResolvedLimited());

    S.Cleanup();
    return true;
}

// ============================================================================
// URLab.DefaultRes.ChildClassInheritance
//   Joint with no MjClassName inherits from parent body's ChildClassName.
// ============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMjDefResChildClass,
    "URLab.DefaultRes.ChildClassInheritance",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMjDefResChildClass::RunTest(const FString& Parameters)
{
    FMjUESession S;
    bool bOk = S.Init([](FMjUESession& Ses)
    {
        // Create default class "bodydef" with axis=(0,0,-1) and slide type
        UMjDefault* Def = NewObject<UMjDefault>(Ses.Robot, TEXT("DefBodyDef"));
        Def->ClassName = TEXT("bodydef");
        Def->bIsDefault = true;
        Def->RegisterComponent();
        Def->AttachToComponent(Ses.Robot->GetRootComponent(), FAttachmentTransformRules::KeepRelativeTransform);

        UMjJoint* DefJ = NewObject<UMjJoint>(Ses.Robot, TEXT("DefBodyDefJoint"));
        DefJ->bIsDefault = true;
        DefJ->bOverride_Type = true;
        DefJ->Type = EMjJointType::Slide;
        DefJ->RegisterComponent();
        DefJ->AttachToComponent(Def, FAttachmentTransformRules::KeepRelativeTransform);

        // Set the body's ChildClassName
        Ses.Body->bOverride_ChildClassName = true;
        Ses.Body->ChildClassName = TEXT("bodydef");
    });
    if (!TestTrue(TEXT("Init"), bOk)) return false;

    // Joint has no MjClassName but parent body has ChildClassName="bodydef"
    UMjJoint* EdJ = NewObject<UMjJoint>(S.Robot, TEXT("ChildClassJoint"));
    EdJ->RegisterComponent();
    EdJ->AttachToComponent(S.Body, FAttachmentTransformRules::KeepRelativeTransform);

    TestEqual(TEXT("Type from body childclass"), EdJ->GetResolvedType(), EMjJointType::Slide);

    S.Cleanup();
    return true;
}

// ============================================================================
// URLab.DefaultRes.MissingClassFallback
//   Joint with MjClassName pointing to nonexistent class → MuJoCo builtins.
// ============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMjDefResMissingClass,
    "URLab.DefaultRes.MissingClassFallback",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMjDefResMissingClass::RunTest(const FString& Parameters)
{
    FMjUESession S;
    bool bOk = S.Init();
    if (!TestTrue(TEXT("Init"), bOk)) return false;

    UMjJoint* EdJ = NewObject<UMjJoint>(S.Robot, TEXT("NoClassJoint"));
    EdJ->RegisterComponent();
    EdJ->AttachToComponent(S.Body, FAttachmentTransformRules::KeepRelativeTransform);
    EdJ->MjClassName = TEXT("nonexistent_class");

    // Should fall through to builtins
    TestTrue(TEXT("Axis Z-up"), EdJ->GetResolvedAxis().Equals(FVector(0, 0, 1), 1e-4f));
    TestEqual(TEXT("Type Hinge"), EdJ->GetResolvedType(), EMjJointType::Hinge);

    S.Cleanup();
    return true;
}

// ============================================================================
// URLab.DefaultRes.CycleProtection
//   Circular ParentClassName chain doesn't infinite loop.
// ============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMjDefResCycle,
    "URLab.DefaultRes.CycleProtection",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMjDefResCycle::RunTest(const FString& Parameters)
{
    FMjUESession S;
    bool bOk = S.Init([](FMjUESession& Ses)
    {
        // Create circular: A→B→A
        UMjDefault* DefA = NewObject<UMjDefault>(Ses.Robot, TEXT("DefA"));
        DefA->ClassName = TEXT("classA");
        DefA->ParentClassName = TEXT("classB");
        DefA->bIsDefault = true;
        DefA->RegisterComponent();
        DefA->AttachToComponent(Ses.Robot->GetRootComponent(), FAttachmentTransformRules::KeepRelativeTransform);

        UMjDefault* DefB = NewObject<UMjDefault>(Ses.Robot, TEXT("DefB"));
        DefB->ClassName = TEXT("classB");
        DefB->ParentClassName = TEXT("classA");
        DefB->bIsDefault = true;
        DefB->RegisterComponent();
        DefB->AttachToComponent(Ses.Robot->GetRootComponent(), FAttachmentTransformRules::KeepRelativeTransform);
    });
    if (!TestTrue(TEXT("Init"), bOk)) return false;

    UMjJoint* EdJ = NewObject<UMjJoint>(S.Robot, TEXT("CycleJoint"));
    EdJ->RegisterComponent();
    EdJ->AttachToComponent(S.Body, FAttachmentTransformRules::KeepRelativeTransform);
    EdJ->MjClassName = TEXT("classA");

    // Should not hang — falls through to builtin
    FVector Axis = EdJ->GetResolvedAxis();
    TestTrue(TEXT("Cycle safe: returns builtin Z-up"), Axis.Equals(FVector(0, 0, 1), 1e-4f));

    S.Cleanup();
    return true;
}

// ============================================================================
// URLab.DefaultRes.DefaultNoChildJoint
//   Default class exists but has no child UMjJoint → walks to parent or builtin.
// ============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMjDefResNoChild,
    "URLab.DefaultRes.DefaultNoChildJoint",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMjDefResNoChild::RunTest(const FString& Parameters)
{
    FMjUESession S;
    bool bOk = S.Init([](FMjUESession& Ses)
    {
        // Default "empty" has no child UMjJoint
        UMjDefault* Def = NewObject<UMjDefault>(Ses.Robot, TEXT("DefEmpty"));
        Def->ClassName = TEXT("empty");
        Def->bIsDefault = true;
        Def->RegisterComponent();
        Def->AttachToComponent(Ses.Robot->GetRootComponent(), FAttachmentTransformRules::KeepRelativeTransform);

        // Only a geom child, no joint child
        UMjGeom* DefGeom = NewObject<UMjGeom>(Ses.Robot, TEXT("DefEmptyGeom"));
        DefGeom->bIsDefault = true;
        DefGeom->RegisterComponent();
        DefGeom->AttachToComponent(Def, FAttachmentTransformRules::KeepRelativeTransform);
    });
    if (!TestTrue(TEXT("Init"), bOk)) return false;

    UMjJoint* EdJ = NewObject<UMjJoint>(S.Robot, TEXT("NoChildJoint"));
    EdJ->RegisterComponent();
    EdJ->AttachToComponent(S.Body, FAttachmentTransformRules::KeepRelativeTransform);
    EdJ->MjClassName = TEXT("empty");

    // No UMjJoint child in "empty" → falls to builtin
    TestTrue(TEXT("Axis Z-up"), EdJ->GetResolvedAxis().Equals(FVector(0, 0, 1), 1e-4f));

    S.Cleanup();
    return true;
}

// ============================================================================
// URLab.DefaultRes.EditorMatchesCompiled
//   Verify that editor-time resolution matches compiled model values.
//   Import a model with defaults, check GetResolved* before bind, then
//   verify it matches post-bind values.
// ============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMjDefResEditorMatchesCompiled,
    "URLab.DefaultRes.EditorMatchesCompiled",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMjDefResEditorMatchesCompiled::RunTest(const FString& Parameters)
{
    FMjUESession S;

    // Pre-resolve values captured before compile
    FVector PreAxis;
    FVector2D PreRange;
    bool PreLimited;
    EMjJointType PreType;

    bool bOk = S.Init([&](FMjUESession& Ses)
    {
        // Create default "robot" with axis=(1,0,0), limited=true, range=(-0.5, 0.5)
        UMjDefault* Def = NewObject<UMjDefault>(Ses.Robot, TEXT("DefRobot"));
        Def->ClassName = TEXT("robot");
        Def->bIsDefault = true;
        Def->RegisterComponent();
        Def->AttachToComponent(Ses.Robot->GetRootComponent(), FAttachmentTransformRules::KeepRelativeTransform);

        UMjJoint* DefJ = NewObject<UMjJoint>(Ses.Robot, TEXT("DefRobotJoint"));
        DefJ->bIsDefault = true;
        DefJ->bOverride_Axis = true;
        DefJ->Axis = FVector(1, 0, 0);
        DefJ->bOverride_Limited = true;
        DefJ->bLimited = true;
        DefJ->bOverride_Range = true;
        DefJ->Range = {-0.5f, 0.5f};
        DefJ->bOverride_Type = true;
        DefJ->Type = EMjJointType::Hinge;
        DefJ->RegisterComponent();
        DefJ->AttachToComponent(Def, FAttachmentTransformRules::KeepRelativeTransform);

        // Set the test joint to use class "robot" (no local overrides)
        Ses.Joint->MjClassName = TEXT("robot");

        // Capture editor-time resolved values BEFORE compile
        PreAxis = Ses.Joint->GetResolvedAxis();
        PreRange = Ses.Joint->GetResolvedRange();
        PreLimited = Ses.Joint->GetResolvedLimited();
        PreType = Ses.Joint->GetResolvedType();
    });
    if (!TestTrue(TEXT("Init"), bOk)) return false;

    // Run forward kinematics so xaxis/xanchor are populated
    if (S.Manager->PhysicsEngine->m_model && S.Manager->PhysicsEngine->m_data)
    {
        mj_forward(S.Manager->PhysicsEngine->m_model, S.Manager->PhysicsEngine->m_data);
    }

    // Post-compile: joint should be bound now
    TestTrue(TEXT("Joint is bound"), S.Joint->IsBound());

    // Get runtime values (from JointView)
    FVector PostAxis = S.Joint->GetResolvedAxis();
    FVector2D PostRange = S.Joint->GetResolvedRange();
    EMjJointType PostType = S.Joint->GetResolvedType();

    // Editor-time values should match compiled values
    TestTrue(TEXT("Axis X matches"), FMath::Abs(PreAxis.X - PostAxis.X) < 1e-3f);
    TestTrue(TEXT("Axis Y matches"), FMath::Abs(PreAxis.Y - PostAxis.Y) < 1e-3f);
    TestTrue(TEXT("Axis Z matches"), FMath::Abs(PreAxis.Z - PostAxis.Z) < 1e-3f);

    TestTrue(TEXT("Range min matches"), FMath::Abs(PreRange.X - PostRange.X) < 1e-3f);
    TestTrue(TEXT("Range max matches"), FMath::Abs(PreRange.Y - PostRange.Y) < 1e-3f);

    TestEqual(TEXT("Type matches"), PreType, PostType);

    // Pre-editor said limited, compiled model should also show non-zero range
    TestTrue(TEXT("Pre-limited was true"), PreLimited);
    TestTrue(TEXT("Post-range is non-zero"), PostRange.X != 0.0f || PostRange.Y != 0.0f);

    S.Cleanup();
    return true;
}
