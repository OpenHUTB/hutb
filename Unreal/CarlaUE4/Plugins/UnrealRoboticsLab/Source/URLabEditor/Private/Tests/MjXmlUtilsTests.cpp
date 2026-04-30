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
#include "MuJoCo/Utils/MjXmlUtils.h"
#include "XmlFile.h"

// ============================================================================
// URLab.XmlUtils.ParseVector_Normal
//   "1.5 -2.0 3.0" → FVector(1.5, -2.0, 3.0)
// ============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMjXmlUtilsParseVectorNormal,
    "URLab.XmlUtils.ParseVector_Normal",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMjXmlUtilsParseVectorNormal::RunTest(const FString& Parameters)
{
    FVector V = MjXmlUtils::ParseVector(TEXT("1.5 -2.0 3.0"));
    TestTrue(TEXT("X ~= 1.5"), FMath::Abs(V.X - 1.5) < 1e-4);
    TestTrue(TEXT("Y ~= -2.0"), FMath::Abs(V.Y - (-2.0)) < 1e-4);
    TestTrue(TEXT("Z ~= 3.0"), FMath::Abs(V.Z - 3.0) < 1e-4);
    return true;
}

// ============================================================================
// URLab.XmlUtils.ParseVector_Empty
//   Empty string → ZeroVector
// ============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMjXmlUtilsParseVectorEmpty,
    "URLab.XmlUtils.ParseVector_Empty",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMjXmlUtilsParseVectorEmpty::RunTest(const FString& Parameters)
{
    FVector V = MjXmlUtils::ParseVector(TEXT(""));
    TestTrue(TEXT("Empty → ZeroVector"), V.Equals(FVector::ZeroVector, 1e-6));
    return true;
}

// ============================================================================
// URLab.XmlUtils.ParseVector_TwoComponents
//   "1.0 2.0" (only 2 values) → ZeroVector (needs 3)
// ============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMjXmlUtilsParseVectorTwoComponents,
    "URLab.XmlUtils.ParseVector_TwoComponents",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMjXmlUtilsParseVectorTwoComponents::RunTest(const FString& Parameters)
{
    FVector V = MjXmlUtils::ParseVector(TEXT("1.0 2.0"));
    TestTrue(TEXT("2 components → ZeroVector"), V.Equals(FVector::ZeroVector, 1e-6));
    return true;
}

// ============================================================================
// URLab.XmlUtils.ParseVector_ExtraWhitespace
//   "  1.0  2.0  3.0  " → should still parse correctly
// ============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMjXmlUtilsParseVectorWhitespace,
    "URLab.XmlUtils.ParseVector_ExtraWhitespace",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMjXmlUtilsParseVectorWhitespace::RunTest(const FString& Parameters)
{
    FVector V = MjXmlUtils::ParseVector(TEXT("  1.0  2.0  3.0  "));
    TestTrue(TEXT("X ~= 1.0"), FMath::Abs(V.X - 1.0) < 1e-4);
    TestTrue(TEXT("Y ~= 2.0"), FMath::Abs(V.Y - 2.0) < 1e-4);
    TestTrue(TEXT("Z ~= 3.0"), FMath::Abs(V.Z - 3.0) < 1e-4);
    return true;
}

// ============================================================================
// URLab.XmlUtils.ParseVector2D_Normal
//   "1.5 -2.0" → FVector2D(1.5, -2.0)
// ============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMjXmlUtilsParseVector2DNormal,
    "URLab.XmlUtils.ParseVector2D_Normal",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMjXmlUtilsParseVector2DNormal::RunTest(const FString& Parameters)
{
    FVector2D V = MjXmlUtils::ParseVector2D(TEXT("1.5 -2.0"));
    TestTrue(TEXT("X ~= 1.5"), FMath::Abs(V.X - 1.5) < 1e-4);
    TestTrue(TEXT("Y ~= -2.0"), FMath::Abs(V.Y - (-2.0)) < 1e-4);
    return true;
}

// ============================================================================
// URLab.XmlUtils.ParseVector2D_Empty
// ============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMjXmlUtilsParseVector2DEmpty,
    "URLab.XmlUtils.ParseVector2D_Empty",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMjXmlUtilsParseVector2DEmpty::RunTest(const FString& Parameters)
{
    FVector2D V = MjXmlUtils::ParseVector2D(TEXT(""));
    TestTrue(TEXT("Empty → ZeroVector"), V.Equals(FVector2D::ZeroVector, 1e-6));
    return true;
}

// ============================================================================
// URLab.XmlUtils.ParseFloatArray_Normal
//   "0.8 0.1 0.01" → [0.8, 0.1, 0.01]
// ============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMjXmlUtilsParseFloatArrayNormal,
    "URLab.XmlUtils.ParseFloatArray_Normal",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMjXmlUtilsParseFloatArrayNormal::RunTest(const FString& Parameters)
{
    TArray<float> Out;
    MjXmlUtils::ParseFloatArray(TEXT("0.8 0.1 0.01"), Out);
    TestEqual(TEXT("Count == 3"), Out.Num(), 3);
    if (Out.Num() == 3)
    {
        TestTrue(TEXT("[0] ~= 0.8"), FMath::Abs(Out[0] - 0.8f) < 1e-4f);
        TestTrue(TEXT("[1] ~= 0.1"), FMath::Abs(Out[1] - 0.1f) < 1e-4f);
        TestTrue(TEXT("[2] ~= 0.01"), FMath::Abs(Out[2] - 0.01f) < 1e-4f);
    }
    return true;
}

// ============================================================================
// URLab.XmlUtils.ParseFloatArray_SingleValue
//   "0.5" → [0.5]
// ============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMjXmlUtilsParseFloatArraySingle,
    "URLab.XmlUtils.ParseFloatArray_SingleValue",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMjXmlUtilsParseFloatArraySingle::RunTest(const FString& Parameters)
{
    TArray<float> Out;
    MjXmlUtils::ParseFloatArray(TEXT("0.5"), Out);
    TestEqual(TEXT("Count == 1"), Out.Num(), 1);
    if (Out.Num() == 1)
    {
        TestTrue(TEXT("[0] ~= 0.5"), FMath::Abs(Out[0] - 0.5f) < 1e-4f);
    }
    return true;
}

// ============================================================================
// URLab.XmlUtils.ParseFloatArray_Empty
//   "" → empty array
// ============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMjXmlUtilsParseFloatArrayEmpty,
    "URLab.XmlUtils.ParseFloatArray_Empty",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMjXmlUtilsParseFloatArrayEmpty::RunTest(const FString& Parameters)
{
    TArray<float> Out;
    MjXmlUtils::ParseFloatArray(TEXT(""), Out);
    TestEqual(TEXT("Empty → 0 elements"), Out.Num(), 0);
    return true;
}

// ============================================================================
// URLab.XmlUtils.ParseFloatArray_Negative
//   "-1.5 2.0 -3.0" → [-1.5, 2.0, -3.0]
// ============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMjXmlUtilsParseFloatArrayNegative,
    "URLab.XmlUtils.ParseFloatArray_Negative",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMjXmlUtilsParseFloatArrayNegative::RunTest(const FString& Parameters)
{
    TArray<float> Out;
    MjXmlUtils::ParseFloatArray(TEXT("-1.5 2.0 -3.0"), Out);
    TestEqual(TEXT("Count == 3"), Out.Num(), 3);
    if (Out.Num() == 3)
    {
        TestTrue(TEXT("[0] ~= -1.5"), FMath::Abs(Out[0] - (-1.5f)) < 1e-4f);
        TestTrue(TEXT("[1] ~= 2.0"), FMath::Abs(Out[1] - 2.0f) < 1e-4f);
        TestTrue(TEXT("[2] ~= -3.0"), FMath::Abs(Out[2] - (-3.0f)) < 1e-4f);
    }
    return true;
}

// ============================================================================
// URLab.XmlUtils.ParseFloatArray_ClearsExisting
//   Calling ParseFloatArray on a non-empty array should clear it first
// ============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMjXmlUtilsParseFloatArrayClears,
    "URLab.XmlUtils.ParseFloatArray_ClearsExisting",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMjXmlUtilsParseFloatArrayClears::RunTest(const FString& Parameters)
{
    TArray<float> Out = { 99.0f, 88.0f, 77.0f };
    MjXmlUtils::ParseFloatArray(TEXT("1.0 2.0"), Out);
    TestEqual(TEXT("Count == 2 (old values cleared)"), Out.Num(), 2);
    if (Out.Num() == 2)
    {
        TestTrue(TEXT("[0] ~= 1.0"), FMath::Abs(Out[0] - 1.0f) < 1e-4f);
        TestTrue(TEXT("[1] ~= 2.0"), FMath::Abs(Out[1] - 2.0f) < 1e-4f);
    }
    return true;
}

// ============================================================================
// URLab.XmlUtils.ParseBool_True
//   "true" → true, "1" → true
// ============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMjXmlUtilsParseBoolTrue,
    "URLab.XmlUtils.ParseBool_True",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMjXmlUtilsParseBoolTrue::RunTest(const FString& Parameters)
{
    TestTrue(TEXT("\"true\" → true"), MjXmlUtils::ParseBool(TEXT("true")));
    TestTrue(TEXT("\"True\" → true"), MjXmlUtils::ParseBool(TEXT("True")));
    TestTrue(TEXT("\"TRUE\" → true"), MjXmlUtils::ParseBool(TEXT("TRUE")));
    TestTrue(TEXT("\"1\" → true"), MjXmlUtils::ParseBool(TEXT("1")));
    return true;
}

// ============================================================================
// URLab.XmlUtils.ParseBool_False
//   "false" → false, "0" → false
// ============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMjXmlUtilsParseBoolFalse,
    "URLab.XmlUtils.ParseBool_False",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMjXmlUtilsParseBoolFalse::RunTest(const FString& Parameters)
{
    TestFalse(TEXT("\"false\" → false"), MjXmlUtils::ParseBool(TEXT("false")));
    TestFalse(TEXT("\"False\" → false"), MjXmlUtils::ParseBool(TEXT("False")));
    TestFalse(TEXT("\"0\" → false"), MjXmlUtils::ParseBool(TEXT("0")));
    return true;
}

// ============================================================================
// URLab.XmlUtils.ParseBool_Empty
//   "" with default=false → false, "" with default=true → true
// ============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMjXmlUtilsParseBoolEmpty,
    "URLab.XmlUtils.ParseBool_Empty",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMjXmlUtilsParseBoolEmpty::RunTest(const FString& Parameters)
{
    TestFalse(TEXT("Empty default=false → false"), MjXmlUtils::ParseBool(TEXT(""), false));
    TestTrue(TEXT("Empty default=true → true"), MjXmlUtils::ParseBool(TEXT(""), true));
    return true;
}

// ============================================================================
// URLab.XmlUtils.ParseBool_MujocoEnable
//   MuJoCo uses "enable"/"disable" in some attributes — verify behavior.
//   These should return false since they're not "true"/"1".
// ============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMjXmlUtilsParseBoolMujoco,
    "URLab.XmlUtils.ParseBool_MujocoEnable",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMjXmlUtilsParseBoolMujoco::RunTest(const FString& Parameters)
{
    // MuJoCo often uses "enable"/"disable" — ParseBool only handles "true"/"1"
    // This documents the current behavior (not necessarily correct for all use cases)
    TestFalse(TEXT("\"enable\" → false (not handled)"), MjXmlUtils::ParseBool(TEXT("enable")));
    TestFalse(TEXT("\"disable\" → false"), MjXmlUtils::ParseBool(TEXT("disable")));
    return true;
}

// ============================================================================
// URLab.XmlUtils.ParseFloatArray_ManyValues
//   11-value polycoef style: "0 1 0 0 0 0 0 0 0 0 0"
// ============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMjXmlUtilsParseFloatArrayMany,
    "URLab.XmlUtils.ParseFloatArray_ManyValues",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMjXmlUtilsParseFloatArrayMany::RunTest(const FString& Parameters)
{
    TArray<float> Out;
    MjXmlUtils::ParseFloatArray(TEXT("0 1 0 0 0 0 0 0 0 0 0"), Out);
    TestEqual(TEXT("Count == 11"), Out.Num(), 11);
    if (Out.Num() >= 2)
    {
        TestTrue(TEXT("[0] ~= 0"), FMath::Abs(Out[0]) < 1e-4f);
        TestTrue(TEXT("[1] ~= 1"), FMath::Abs(Out[1] - 1.0f) < 1e-4f);
    }
    return true;
}

// ============================================================================
// URLab.XmlUtils.ParseFloatArray_Partial
//   MuJoCo allows partial arrays: friction="1" sets only [0], rest stay default.
//   Verify ParseFloatArray produces the correct count for partial input.
// ============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMjXmlUtilsParseFloatArrayPartial,
    "URLab.XmlUtils.ParseFloatArray_Partial",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMjXmlUtilsParseFloatArrayPartial::RunTest(const FString& Parameters)
{
    // friction="1" → only 1 element, not 3
    TArray<float> Out;
    MjXmlUtils::ParseFloatArray(TEXT("1"), Out);
    TestEqual(TEXT("friction='1' → 1 element"), Out.Num(), 1);
    if (Out.Num() >= 1) TestTrue(TEXT("[0] ~= 1.0"), FMath::Abs(Out[0] - 1.0f) < 1e-4f);

    // solimp="0.9 0.95" → only 2 of 5
    MjXmlUtils::ParseFloatArray(TEXT("0.9 0.95"), Out);
    TestEqual(TEXT("solimp='0.9 0.95' → 2 elements"), Out.Num(), 2);

    // range="-1.57 1.57" → exactly 2
    MjXmlUtils::ParseFloatArray(TEXT("-1.57 1.57"), Out);
    TestEqual(TEXT("range='-1.57 1.57' → 2 elements"), Out.Num(), 2);
    if (Out.Num() == 2)
    {
        TestTrue(TEXT("[0] ~= -1.57"), FMath::Abs(Out[0] - (-1.57f)) < 1e-3f);
        TestTrue(TEXT("[1] ~= 1.57"), FMath::Abs(Out[1] - 1.57f) < 1e-3f);
    }

    return true;
}

// ============================================================================
// URLab.XmlUtils.ParseVector_ScientificNotation
//   "1e-3 2.5e2 -3.0e1" — MuJoCo XML can contain scientific notation
// ============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMjXmlUtilsParseVectorScientific,
    "URLab.XmlUtils.ParseVector_ScientificNotation",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMjXmlUtilsParseVectorScientific::RunTest(const FString& Parameters)
{
    FVector V = MjXmlUtils::ParseVector(TEXT("1e-3 2.5e2 -3.0e1"));
    TestTrue(TEXT("X ~= 0.001"), FMath::Abs(V.X - 0.001) < 1e-6);
    TestTrue(TEXT("Y ~= 250.0"), FMath::Abs(V.Y - 250.0) < 1e-2);
    TestTrue(TEXT("Z ~= -30.0"), FMath::Abs(V.Z - (-30.0)) < 1e-2);
    return true;
}

// ============================================================================
// URLab.XmlUtils.ReadAttrFloat
//   Tests the new ReadAttrFloat helper using a real FXmlNode.
// ============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMjXmlUtilsReadAttrFloat,
    "URLab.XmlUtils.ReadAttrFloat",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMjXmlUtilsReadAttrFloat::RunTest(const FString& Parameters)
{
    FXmlFile Xml(TEXT("<geom damping=\"0.5\" mass=\"1.25\"/>"), EConstructMethod::ConstructFromBuffer);
    const FXmlNode* Node = Xml.GetRootNode();
    if (!TestNotNull(TEXT("XML parsed"), Node)) return false;

    float Damping = 0.0f;
    bool bOvr = false;

    // Present attribute
    bool bFound = MjXmlUtils::ReadAttrFloat(Node, TEXT("damping"), Damping, bOvr);
    TestTrue(TEXT("damping found"), bFound);
    TestTrue(TEXT("bOverride set"), bOvr);
    TestTrue(TEXT("damping ~= 0.5"), FMath::Abs(Damping - 0.5f) < 1e-4f);

    // Missing attribute
    float Gap = -1.0f;
    bool bOvrGap = false;
    bFound = MjXmlUtils::ReadAttrFloat(Node, TEXT("gap"), Gap, bOvrGap);
    TestFalse(TEXT("gap not found"), bFound);
    TestFalse(TEXT("gap bOverride not set"), bOvrGap);
    TestTrue(TEXT("gap value unchanged"), FMath::Abs(Gap - (-1.0f)) < 1e-4f);

    // Second present attribute
    float Mass = 0.0f;
    bool bOvrMass = false;
    MjXmlUtils::ReadAttrFloat(Node, TEXT("mass"), Mass, bOvrMass);
    TestTrue(TEXT("mass bOverride set"), bOvrMass);
    TestTrue(TEXT("mass ~= 1.25"), FMath::Abs(Mass - 1.25f) < 1e-4f);

    return true;
}

// ============================================================================
// URLab.XmlUtils.ReadAttrInt
// ============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMjXmlUtilsReadAttrInt,
    "URLab.XmlUtils.ReadAttrInt",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMjXmlUtilsReadAttrInt::RunTest(const FString& Parameters)
{
    FXmlFile Xml(TEXT("<geom condim=\"3\" group=\"2\"/>"), EConstructMethod::ConstructFromBuffer);
    const FXmlNode* Node = Xml.GetRootNode();
    if (!TestNotNull(TEXT("XML parsed"), Node)) return false;

    int32 Condim = 0;
    bool bOvr = false;
    MjXmlUtils::ReadAttrInt(Node, TEXT("condim"), Condim, bOvr);
    TestTrue(TEXT("condim found"), bOvr);
    TestEqual(TEXT("condim == 3"), Condim, 3);

    int32 Priority = -1;
    bool bOvrP = false;
    MjXmlUtils::ReadAttrInt(Node, TEXT("priority"), Priority, bOvrP);
    TestFalse(TEXT("priority not found"), bOvrP);
    TestEqual(TEXT("priority unchanged"), Priority, -1);

    return true;
}

// ============================================================================
// URLab.XmlUtils.ReadAttrFloatArray
// ============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMjXmlUtilsReadAttrFloatArray,
    "URLab.XmlUtils.ReadAttrFloatArray",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMjXmlUtilsReadAttrFloatArray::RunTest(const FString& Parameters)
{
    FXmlFile Xml(TEXT("<geom friction=\"0.8 0.1\" solref=\"0.02 1\"/>"), EConstructMethod::ConstructFromBuffer);
    const FXmlNode* Node = Xml.GetRootNode();
    if (!TestNotNull(TEXT("XML parsed"), Node)) return false;

    // Partial friction (2 of 3)
    TArray<float> Friction;
    bool bOvr = false;
    MjXmlUtils::ReadAttrFloatArray(Node, TEXT("friction"), Friction, bOvr);
    TestTrue(TEXT("friction found"), bOvr);
    TestEqual(TEXT("friction count == 2 (partial)"), Friction.Num(), 2);
    if (Friction.Num() >= 2)
    {
        TestTrue(TEXT("friction[0] ~= 0.8"), FMath::Abs(Friction[0] - 0.8f) < 1e-4f);
        TestTrue(TEXT("friction[1] ~= 0.1"), FMath::Abs(Friction[1] - 0.1f) < 1e-4f);
    }

    // Missing attribute
    TArray<float> SolImp;
    bool bOvrS = false;
    MjXmlUtils::ReadAttrFloatArray(Node, TEXT("solimp"), SolImp, bOvrS);
    TestFalse(TEXT("solimp not found"), bOvrS);
    TestEqual(TEXT("solimp array empty"), SolImp.Num(), 0);

    return true;
}

// ============================================================================
// URLab.XmlUtils.ReadAttrBool
// ============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMjXmlUtilsReadAttrBool,
    "URLab.XmlUtils.ReadAttrBool",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMjXmlUtilsReadAttrBool::RunTest(const FString& Parameters)
{
    FXmlFile Xml(TEXT("<joint limited=\"true\" actlimited=\"false\"/>"), EConstructMethod::ConstructFromBuffer);
    const FXmlNode* Node = Xml.GetRootNode();
    if (!TestNotNull(TEXT("XML parsed"), Node)) return false;

    bool Limited = false;
    bool bOvr = false;
    MjXmlUtils::ReadAttrBool(Node, TEXT("limited"), Limited, bOvr);
    TestTrue(TEXT("limited found"), bOvr);
    TestTrue(TEXT("limited == true"), Limited);

    bool ActLimited = true;
    bool bOvrA = false;
    MjXmlUtils::ReadAttrBool(Node, TEXT("actlimited"), ActLimited, bOvrA);
    TestTrue(TEXT("actlimited found"), bOvrA);
    TestFalse(TEXT("actlimited == false"), ActLimited);

    bool Missing = true;
    bool bOvrM = false;
    MjXmlUtils::ReadAttrBool(Node, TEXT("nope"), Missing, bOvrM);
    TestFalse(TEXT("nope not found"), bOvrM);
    TestTrue(TEXT("missing value unchanged"), Missing);

    return true;
}

// ============================================================================
// URLab.XmlUtils.ReadAttrString
// ============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMjXmlUtilsReadAttrString,
    "URLab.XmlUtils.ReadAttrString",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMjXmlUtilsReadAttrString::RunTest(const FString& Parameters)
{
    FXmlFile Xml(TEXT("<body name=\"torso\" childclass=\"robot\"/>"), EConstructMethod::ConstructFromBuffer);
    const FXmlNode* Node = Xml.GetRootNode();
    if (!TestNotNull(TEXT("XML parsed"), Node)) return false;

    FString Name;
    TestTrue(TEXT("name found"), MjXmlUtils::ReadAttrString(Node, TEXT("name"), Name));
    TestEqual(TEXT("name == torso"), Name, TEXT("torso"));

    FString Missing;
    TestFalse(TEXT("mocap not found"), MjXmlUtils::ReadAttrString(Node, TEXT("mocap"), Missing));
    TestTrue(TEXT("missing string empty"), Missing.IsEmpty());

    return true;
}
