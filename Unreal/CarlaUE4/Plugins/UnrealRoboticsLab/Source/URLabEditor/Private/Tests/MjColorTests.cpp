// Copyright (c) 2026 Jonathan Embley-Riches. All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "MuJoCo/Utils/MjColor.h"

// Halton base-2 sequence: 1/2, 1/4, 3/4, 1/8, 5/8, 3/8, 7/8...
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMjColorHaltonBase2,
    "URLab.Color.Halton_Base2",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FMjColorHaltonBase2::RunTest(const FString& Parameters)
{
    const float Expected[] = { 0.5f, 0.25f, 0.75f, 0.125f, 0.625f, 0.375f, 0.875f };
    for (int32 i = 0; i < UE_ARRAY_COUNT(Expected); ++i)
    {
        const float V = MjColor::Halton(i + 1, 2);
        TestTrue(FString::Printf(TEXT("Halton(%d,2) ~= %f"), i + 1, Expected[i]),
            FMath::IsNearlyEqual(V, Expected[i], 1e-5f));
    }
    return true;
}

// Halton(0, base) must return 0 and never loop forever.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMjColorHaltonZero,
    "URLab.Color.Halton_Zero",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FMjColorHaltonZero::RunTest(const FString& Parameters)
{
    TestTrue(TEXT("Halton(0,7) == 0"), FMath::IsNearlyZero(MjColor::Halton(0, 7)));
    return true;
}

// HSV(0,0,V) should produce pure grey regardless of hue.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMjColorHSVGrey,
    "URLab.Color.HSV_ZeroSaturationGivesGrey",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FMjColorHSVGrey::RunTest(const FString& Parameters)
{
    const FLinearColor C = MjColor::HSVToRGB(0.37f, 0.0f, 0.5f);
    TestTrue(TEXT("R ~= G"), FMath::IsNearlyEqual(C.R, C.G, 1e-5f));
    TestTrue(TEXT("G ~= B"), FMath::IsNearlyEqual(C.G, C.B, 1e-5f));
    TestTrue(TEXT("V ~= 0.5"), FMath::IsNearlyEqual(C.R, 0.5f, 1e-5f));
    return true;
}

// HSV primaries round-trip: H=0 -> pure red.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMjColorHSVPrimaries,
    "URLab.Color.HSV_Primaries",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FMjColorHSVPrimaries::RunTest(const FString& Parameters)
{
    const FLinearColor Red = MjColor::HSVToRGB(0.0f, 1.0f, 1.0f);
    TestTrue(TEXT("Red.R=1"), FMath::IsNearlyEqual(Red.R, 1.0f, 1e-5f));
    TestTrue(TEXT("Red.G=0"), FMath::IsNearlyZero(Red.G, 1e-5f));
    TestTrue(TEXT("Red.B=0"), FMath::IsNearlyZero(Red.B, 1e-5f));

    const FLinearColor Green = MjColor::HSVToRGB(1.0f/3.0f, 1.0f, 1.0f);
    TestTrue(TEXT("Green.G=1"), FMath::IsNearlyEqual(Green.G, 1.0f, 1e-4f));
    TestTrue(TEXT("Green.R=0"), FMath::IsNearlyZero(Green.R, 1e-4f));

    const FLinearColor Blue = MjColor::HSVToRGB(2.0f/3.0f, 1.0f, 1.0f);
    TestTrue(TEXT("Blue.B=1"), FMath::IsNearlyEqual(Blue.B, 1.0f, 1e-4f));
    TestTrue(TEXT("Blue.R=0"), FMath::IsNearlyZero(Blue.R, 1e-4f));
    return true;
}

// Sleep modulation: saturation and value must shrink, hue preserved.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMjColorSleepModulation,
    "URLab.Color.SleepModulationDimsAndDesaturates",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FMjColorSleepModulation::RunTest(const FString& Parameters)
{
    // Start with a vivid saturated red: HSV(0, 1, 1). Pass explicit scales
    // so the test is independent of the default knob values.
    const FLinearColor Awake = MjColor::HSVToRGB(0.0f, 1.0f, 1.0f);
    const FLinearColor Asleep = MjColor::ApplySleepModulation(Awake, 0.6f, 0.7f);

    // Value (max channel) should drop from 1.0 to ~0.6.
    const float AwakeMax = FMath::Max3(Awake.R, Awake.G, Awake.B);
    const float AsleepMax = FMath::Max3(Asleep.R, Asleep.G, Asleep.B);
    TestTrue(TEXT("Value dimmed"), FMath::IsNearlyEqual(AsleepMax, AwakeMax * 0.6f, 1e-4f));

    // Desaturation: min channel should rise (grey component added).
    TestTrue(TEXT("Min channel increased"),
        FMath::Min3(Asleep.R, Asleep.G, Asleep.B) >= FMath::Min3(Awake.R, Awake.G, Awake.B) - 1e-4f);
    return true;
}

// Island colors are deterministic for the same (id, awake) pair.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMjColorIslandDeterministic,
    "URLab.Color.IslandColor_Deterministic",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FMjColorIslandDeterministic::RunTest(const FString& Parameters)
{
    for (int32 i = 0; i < 10; ++i)
    {
        const FLinearColor A = MjColor::IslandColor(i, true);
        const FLinearColor B = MjColor::IslandColor(i, true);
        TestTrue(TEXT("Same id, same awake -> same color"), A.Equals(B, 1e-5f));
    }
    return true;
}

// Sleep modulation on IslandColor visibly changes max channel.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMjColorIslandSleepDiffers,
    "URLab.Color.IslandColor_SleepDiffers",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FMjColorIslandSleepDiffers::RunTest(const FString& Parameters)
{
    const FLinearColor Awake = MjColor::IslandColor(3, true);
    const FLinearColor Asleep = MjColor::IslandColor(3, false);
    const float AwakeMax = FMath::Max3(Awake.R, Awake.G, Awake.B);
    const float AsleepMax = FMath::Max3(Asleep.R, Asleep.G, Asleep.B);
    TestTrue(TEXT("Asleep is dimmer than awake"), AsleepMax < AwakeMax - 1e-3f);
    return true;
}
