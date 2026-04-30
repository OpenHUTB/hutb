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

class FXmlNode;

/**
 * Compiler settings parsed from the <compiler> element of an MJCF file.
 * These affect how orientation attributes are interpreted.
 */
struct FMjCompilerSettings
{
    /**
     * If true, angles (euler, axisangle, joint range) are in degrees.
     * MuJoCo's upstream default when no <compiler> tag is present is "degree",
     * so we match that. Without this default, models relying on the implicit
     * default (e.g. tendon_arm/arm26.xml) get joint ranges like 0..120 treated
     * as radians — the compile-time muscle lengthrange solver then diverges.
     */
    bool bAngleInDegrees = true;

    /** Euler rotation sequence, e.g. "xyz", "XYZ", "zyx". Default "xyz" (intrinsic). */
    FString EulerSeq = TEXT("xyz");

    /** Base directory for mesh file paths (from `meshdir` attribute). Empty = same dir as XML. */
    FString MeshDir;

    /** Base directory for all asset paths (from `assetdir` attribute). Overrides meshdir for meshes if both present. */
    FString AssetDir;

    /** If true, joints/tendons with a `range` attribute are automatically treated as limited (from `autolimits`). */
    bool bAutoLimits = false;

    /** Helper: resolve a relative mesh path using MeshDir / AssetDir with the given XML directory as the root. */
    FString ResolveMeshPath(const FString& RelPath, const FString& XmlDir) const
    {
        if (RelPath.IsEmpty()) return RelPath;
        // AssetDir takes priority for mesh lookups
        FString Base = AssetDir.IsEmpty() ? MeshDir : AssetDir;
        if (Base.IsEmpty()) Base = XmlDir;
        return FPaths::Combine(Base, RelPath);
    }
};

/**
 * Centralized utility for converting MuJoCo orientation attributes to quaternions.
 * 
 * MuJoCo supports 5 orientation representations on spatial-frame elements:
 *   quat, axisangle, euler, xyaxes, zaxis
 * 
 * Only one should be specified per element. Priority order (matching MuJoCo):
 *   quat > axisangle > euler > xyaxes > zaxis
 * 
 * All functions produce MuJoCo-frame quaternions (w, x, y, z).
 * Use MjUtils::MjToUERotation() to convert to Unreal frame afterwards.
 */
class URLAB_API MjOrientationUtils
{
public:
    /**
     * Parse the <compiler> element from an MJCF XML root and extract orientation-related settings.
     * @param RootNode  The root <mujoco> node of the XML document.
     * @return Parsed compiler settings (angle mode, euler sequence).
     */
    static FMjCompilerSettings ParseCompilerSettings(const FXmlNode* RootNode);

    /**
     * Read orientation attributes from an XML element and convert to a MuJoCo-frame quaternion.
     * Checks quat, axisangle, euler, xyaxes, zaxis in priority order.
     * 
     * @param Node      The XML element (e.g. <body>, <geom>, <site>, etc.)
     * @param Settings  Compiler settings (angle units, euler sequence)
     * @param OutQuat   Output quaternion as double[4] in MuJoCo order (w, x, y, z)
     * @return true if any orientation attribute was found and converted, false if none present (identity)
     */
    static bool OrientationToMjQuat(const FXmlNode* Node, const FMjCompilerSettings& Settings, double OutQuat[4]);

    // --- Low-level converters (all produce MuJoCo quaternion w,x,y,z) ---

    /** Convert axis-angle (x, y, z, angle_rad) to quaternion. Axis need not be normalized. */
    static void AxisAngleToQuat(double x, double y, double z, double AngleRad, double OutQuat[4]);

    /** 
     * Convert Euler angles to quaternion given a 3-character euler sequence string.
     * Lowercase = intrinsic (rotating frame), uppercase = extrinsic (fixed frame).
     * Angles must already be in radians. 
     */
    static void EulerToQuat(double e1, double e2, double e3, const FString& EulerSeq, double OutQuat[4]);

    /** Convert xyaxes (6 values: x-axis 3, y-axis 3) to quaternion. Y is orthogonalized. */
    static void XYAxesToQuat(const double XYAxes[6], double OutQuat[4]);

    /** Convert zaxis (3 values) to quaternion via minimal rotation from (0,0,1). */
    static void ZAxisToQuat(const double ZAxis[3], double OutQuat[4]);

private:
    /** Create a quaternion for rotation around a single axis (0=x, 1=y, 2=z) by AngleRad. */
    static void ElementalRotQuat(int AxisIndex, double AngleRad, double OutQuat[4]);

    /** Multiply two quaternions: Result = A * B (Hamilton product). All in w,x,y,z order. */
    static void QuatMul(const double A[4], const double B[4], double Result[4]);

    /** Normalize a quaternion in-place. */
    static void QuatNormalize(double Q[4]);

    /** Convert a 3x3 rotation matrix (row-major) to quaternion (w,x,y,z). */
    static void RotMatToQuat(const double R[9], double OutQuat[4]);
};
