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

#pragma once

#include "CoreMinimal.h"
#include "mujoco/mujoco.h"

struct GeomView;

/**
 * @class MjUtils
 * @brief Static utility class for common MuJoCo <-> Unreal Engine conversions and helper functions.
 * 
 * This class provides standardized methods for:
 * - Coordinate system conversion (Right-Handed Z-up <-> Left-Handed Z-up)
 * - String conversions (char* <-> FString)
 * - Common math type mappings
 */
class URLAB_API MjUtils
{
public:
    /**
     * @brief Converts a MuJoCo position array (double[3]) to an Unreal Engine FVector.
     * Applies scaling (*100) and coordinate axis swizzling (Y-inversion) to match UE's coordinate system.
     * 
     * @param pos Pointer to the MuJoCo position array (size 3).
     * @return FVector The corresponding Unreal Engine position in centimeters.
     */
    static FVector MjToUEPosition(const double* pos);
    static FVector MjToUEPosition(const float* pos);

    /**
     * @brief Converts an Unreal Engine FVector to a MuJoCo position array.
     * Applies scaling (/100) and coordinate axis swizzling (Y-inversion).
     * 
     * @param pos The Unreal Engine position in centimeters.
     * @param outPos Pointer to the output MuJoCo position array (size 3).
     */
    static void UEToMjPosition(const FVector& pos, double* outPos);

    /**
     * @brief Converts a MuJoCo quaternion array (double[4]: w, x, y, z) to an Unreal Engine FQuat.
     * Applies necessary axis flips to account for coordinate system differences.
     * 
     * @param quat Pointer to the MuJoCo quaternion array (size 4).
     * @return FQuat The corresponding Unreal Engine quaternion.
     */
    static FQuat MjToUERotation(const double* quat);

    /**
     * @brief Converts an Unreal Engine FQuat to a MuJoCo quaternion array.
     * Applies necessary axis flips.
     * 
     * @param quat The Unreal Engine quaternion.
     * @param outQuat Pointer to the output MuJoCo quaternion array (size 4).
     */
    static void UEToMjRotation(const FQuat& quat, double* outQuat);

    /**
     * @brief Converts a C-style string (possibly null) to an Unreal Engine FString.
     * 
     * @param text Pointer to the C-string.
     * @return FString The converted string, or empty if input is null.
     */
    static FString MjToString(const char* text);

    /**
     * @brief Copies an Unreal Engine FString into a fixed-size char buffer.
     * Ensures null-termination.
     * 
     * @param text The Unreal Engine string to convert.
     * @param buffer Pointer to the destination char buffer.
     * @param bufferSize The size of the destination buffer.
     */
    static void StringToMj(const FString& text, char* buffer, int bufferSize);
    /**
     * @brief Parses a "fromto" string ("x1 y1 z1 x2 y2 z2") into Start and End vectors.
     * 
     * @param FromToStr The string containing 6 float values.
     * @param OutStart The output start vector.
     * @param OutEnd The output end vector.
     * @return true if successful (6 values parsed), false otherwise.
     */
    static bool ParseFromTo(const FString& FromToStr, FVector& OutStart, FVector& OutEnd);

    /**
     * @brief Renders the collision geometries for a specific MuJoCo Geom (Primitives and Convex Hulls).
     * 
     * @param World The UWorld context.
     * @param m The MuJoCo model.
     * @param geom_view The geometry view containing state and configuration.
     * @param DrawColor The color to draw the wireframes.
     * @param Multiplier Scaling factor for coordinate conversion.
     */
    static void DrawDebugGeom(UWorld* World, const mjModel* m, const GeomView& geom_view, const FColor& DrawColor = FColor::Magenta, float Multiplier = 100.0f);

    /**
     * @brief Draws joint range arc (hinge) or range bar (slide) with position indicator.
     *
     * @param World The UE world.
     * @param Anchor World-space anchor position (UE coordinates, cm).
     * @param Axis World-space axis direction (unit vector, UE coordinates).
     * @param JointType MuJoCo joint type (mjJNT_HINGE or mjJNT_SLIDE).
     * @param bLimited Whether the joint has limits enabled.
     * @param RangeMin Lower limit (radians for hinge, meters for slide).
     * @param RangeMax Upper limit (radians for hinge, meters for slide).
     * @param CurrentPos Current joint position (radians or meters). Use NaN to skip.
     * @param RefPos Reference position (qpos0). Use NaN to skip.
     * @param ArcRadius Radius of the range arc in cm (default 10cm).
     */
    static void DrawDebugJoint(UWorld* World, const FVector& Anchor, const FVector& Axis,
        int JointType, bool bLimited, float RangeMin, float RangeMax,
        float CurrentPos = NAN, float RefPos = NAN, float ArcRadius = 10.0f);

    /**
     * @brief Prettifies an Unreal/MuJoCo name by stripping common unique ID suffixes (like _UAID_).
     * 
     * @param Name The original raw name.
     * @param PrefixToStrip Optional prefix (e.g. robot name) to also remove.
     * @return FString The cleaned, human-readable name.
     */
    static FString PrettifyName(const FString& Name, const FString& PrefixToStrip = TEXT(""));
};
