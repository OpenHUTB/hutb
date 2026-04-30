// Copyright (c) 2026 Jonathan Embley-Riches. All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0

#pragma once

#include "CoreMinimal.h"

/**
 * Colour helpers matching MuJoCo's upstream visualizer (engine_vis_visualize.c).
 * Used by UMjDebugVisualizer to paint bodies in Island / Segmentation modes.
 */
namespace MjColor
{
    /** Radical-inverse (van der Corput / Halton) for a given integer seed and prime base. */
    URLAB_API float Halton(int32 Seed, int32 Base);

    /** HSV -> linear RGB. h,s,v all in [0,1]. */
    URLAB_API FLinearColor HSVToRGB(float H, float S, float V);

    /**
     * MuJoCo's `islandColor()` formula, with optional sleep modulation.
     * hue = halton(id+1, 7); sat = 0.5 + 0.5*halton(id+1, 3); val = 0.6 + 0.4*halton(id+1, 5).
     * If !awake: val *= ValueScale, sat *= SaturationScale (MuJoCo upstream uses 0.6, 0.7).
     */
    URLAB_API FLinearColor IslandColor(int32 IslandId, bool bAwake, float SleepValueScale = 0.35f, float SleepSaturationScale = 0.9f);

    /**
     * Instance segmentation — each body gets a unique Halton-keyed colour,
     * distinct even within the same articulation.
     */
    URLAB_API FLinearColor InstanceSegmentationColor(uint32 ArticulationHash, int32 BodyIndex, bool bAwake, float SleepValueScale = 0.35f, float SleepSaturationScale = 0.9f);

    /**
     * Semantic segmentation — every body owned by the same actor (articulation
     * or Quick-Convert component) shares a single Halton-keyed colour.
     */
    URLAB_API FLinearColor SemanticSegmentationColor(uint32 ArticulationHash, bool bAwake, float SleepValueScale = 0.35f, float SleepSaturationScale = 0.9f);

    /** Apply the sleep modulation (val*=ValueScale, sat*=SaturationScale) to an existing linear-RGB colour. */
    URLAB_API FLinearColor ApplySleepModulation(const FLinearColor& InColor, float SleepValueScale = 0.35f, float SleepSaturationScale = 0.9f);
}
