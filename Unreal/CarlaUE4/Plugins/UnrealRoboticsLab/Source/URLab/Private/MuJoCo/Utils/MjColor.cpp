// Copyright (c) 2026 Jonathan Embley-Riches. All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0

#include "MuJoCo/Utils/MjColor.h"

namespace MjColor
{
    float Halton(int32 Seed, int32 Base)
    {
        float F = 1.0f;
        float R = 0.0f;
        int32 I = Seed;
        while (I > 0)
        {
            F /= (float)Base;
            R += F * (float)(I % Base);
            I /= Base;
        }
        return R;
    }

    FLinearColor HSVToRGB(float H, float S, float V)
    {
        // Standard HSV -> RGB (matches MuJoCo's hsv2rgb).
        H = FMath::Fmod(H, 1.0f);
        if (H < 0.0f) H += 1.0f;
        const float C = V * S;
        const float Hp = H * 6.0f;
        const float X = C * (1.0f - FMath::Abs(FMath::Fmod(Hp, 2.0f) - 1.0f));
        float R = 0, G = 0, B = 0;
        if      (Hp < 1) { R = C; G = X; }
        else if (Hp < 2) { R = X; G = C; }
        else if (Hp < 3) { G = C; B = X; }
        else if (Hp < 4) { G = X; B = C; }
        else if (Hp < 5) { R = X; B = C; }
        else             { R = C; B = X; }
        const float M = V - C;
        return FLinearColor(R + M, G + M, B + M, 1.0f);
    }

    FLinearColor ApplySleepModulation(const FLinearColor& InColor, float SleepValueScale, float SleepSaturationScale)
    {
        // Re-derive HSV, dim value and desaturate, return RGB.
        const float R = InColor.R, G = InColor.G, B = InColor.B;
        const float Max = FMath::Max3(R, G, B);
        const float Min = FMath::Min3(R, G, B);
        const float V = Max;
        const float S = (Max > 0.0f) ? (Max - Min) / Max : 0.0f;
        float H = 0.0f;
        const float Delta = Max - Min;
        if (Delta > KINDA_SMALL_NUMBER)
        {
            if      (Max == R) H = FMath::Fmod((G - B) / Delta, 6.0f);
            else if (Max == G) H = (B - R) / Delta + 2.0f;
            else               H = (R - G) / Delta + 4.0f;
            H /= 6.0f;
            if (H < 0.0f) H += 1.0f;
        }
        return HSVToRGB(H, S * SleepSaturationScale, V * SleepValueScale);
    }

    FLinearColor IslandColor(int32 IslandId, bool bAwake, float SleepValueScale, float SleepSaturationScale)
    {
        // Match MuJoCo's islandColor() exactly. Upstream special-cases seed=-1
        // (body not in any island and no tree fallback) to neutral grey 0.7.
        if (IslandId < 0)
        {
            float V = 0.7f;
            if (!bAwake) V *= SleepValueScale;
            return FLinearColor(V, V, V);
        }
        // seed = id+1; Halton bases 7/3/5 -> HSV.
        const int32 Seed = IslandId + 1;
        const float H = Halton(Seed, 7);
        float S = 0.5f + 0.5f * Halton(Seed, 3);
        float V = 0.6f + 0.4f * Halton(Seed, 5);
        if (!bAwake)
        {
            V *= SleepValueScale;
            S *= SleepSaturationScale;
        }
        return HSVToRGB(H, S, V);
    }

    FLinearColor InstanceSegmentationColor(uint32 ArticulationHash, int32 BodyIndex, bool bAwake, float SleepValueScale, float SleepSaturationScale)
    {
        // Per-body unique colour via Halton, mixed with articulation hash so
        // different articulations still diverge and adjacent bodies differ.
        // +1 avoids Halton(0)=0 collapsing all channels.
        const int32 Seed = (int32)((ArticulationHash * 2654435761u) ^ (uint32)BodyIndex) + 1;
        const float H = Halton(Seed, 7);
        float S = 0.5f + 0.5f * Halton(Seed, 3);
        float V = 0.6f + 0.4f * Halton(Seed, 5);
        if (!bAwake)
        {
            V *= SleepValueScale;
            S *= SleepSaturationScale;
        }
        return HSVToRGB(H, S, V);
    }

    FLinearColor SemanticSegmentationColor(uint32 ArticulationHash, bool bAwake, float SleepValueScale, float SleepSaturationScale)
    {
        // All bodies of the same articulation/actor share this colour.
        const int32 Seed = (int32)ArticulationHash + 1;
        const float H = Halton(Seed, 7);
        float S = 0.5f + 0.5f * Halton(Seed, 3);
        float V = 0.6f + 0.4f * Halton(Seed, 5);
        if (!bAwake)
        {
            V *= SleepValueScale;
            S *= SleepSaturationScale;
        }
        return HSVToRGB(H, S, V);
    }
}
