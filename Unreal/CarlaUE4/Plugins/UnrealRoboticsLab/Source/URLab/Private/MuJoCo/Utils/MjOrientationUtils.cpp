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

#include "MuJoCo/Utils/MjOrientationUtils.h"
#include "XmlNode.h"
#include "MuJoCo/Utils/MjXmlUtils.h"
#include "Utils/URLabLogging.h"

// ============================================================================
// Compiler Settings
// ============================================================================

FMjCompilerSettings MjOrientationUtils::ParseCompilerSettings(const FXmlNode* RootNode)
{
    FMjCompilerSettings Settings;
    if (!RootNode) return Settings;

    // Search for <compiler> element among children of the root <mujoco> node
    const FXmlNode* CompilerNode = nullptr;
    for (const FXmlNode* Child : RootNode->GetChildrenNodes())
    {
        if (Child->GetTag().Equals(TEXT("compiler"), ESearchCase::IgnoreCase))
        {
            CompilerNode = Child;
            break;
        }
        // Also check includes that might contain a compiler element at top level
        if (Child->GetTag().Equals(TEXT("include"), ESearchCase::IgnoreCase))
        {
            // Includes are resolved elsewhere; compiler in includes would need
            // the included file to be loaded. For now, we only check direct children.
        }
    }

    if (!CompilerNode) return Settings;

    // angle: "degree" or "radian". MuJoCo's default is "degree" (Settings default).
    FString AngleStr = CompilerNode->GetAttribute(TEXT("angle"));
    if (AngleStr.Equals(TEXT("radian"), ESearchCase::IgnoreCase))
    {
        Settings.bAngleInDegrees = false;
    }
    else if (AngleStr.Equals(TEXT("degree"), ESearchCase::IgnoreCase))
    {
        Settings.bAngleInDegrees = true;
    }

    // eulerseq: 3-character string (default "xyz")
    FString EulerSeqStr = CompilerNode->GetAttribute(TEXT("eulerseq"));
    if (!EulerSeqStr.IsEmpty() && EulerSeqStr.Len() == 3)
    {
        Settings.EulerSeq = EulerSeqStr;
    }

    // meshdir: base directory for mesh file lookups
    FString MeshDirStr = CompilerNode->GetAttribute(TEXT("meshdir"));
    if (!MeshDirStr.IsEmpty())
    {
        Settings.MeshDir = MeshDirStr;
    }

    // assetdir: base directory for all assets (overrides meshdir for mesh lookups)
    FString AssetDirStr = CompilerNode->GetAttribute(TEXT("assetdir"));
    if (!AssetDirStr.IsEmpty())
    {
        Settings.AssetDir = AssetDirStr;
    }

    // autolimits: joints/tendons with range are automatically limited
    FString AutoLimitsStr = CompilerNode->GetAttribute(TEXT("autolimits"));
    if (AutoLimitsStr.Equals(TEXT("true"), ESearchCase::IgnoreCase))
    {
        Settings.bAutoLimits = true;
    }

    UE_LOG(LogURLabImport, Log, TEXT("[MjOrientationUtils] Compiler settings: angle=%s, eulerseq=%s, meshdir=%s, assetdir=%s, autolimits=%s"),
        Settings.bAngleInDegrees ? TEXT("degree") : TEXT("radian"),
        *Settings.EulerSeq,
        *Settings.MeshDir,
        *Settings.AssetDir,
        Settings.bAutoLimits ? TEXT("true") : TEXT("false"));

    return Settings;
}

// ============================================================================
// Main Entry Point
// ============================================================================

bool MjOrientationUtils::OrientationToMjQuat(const FXmlNode* Node, const FMjCompilerSettings& Settings, double OutQuat[4])
{
    if (!Node)
    {
        OutQuat[0] = 1.0; OutQuat[1] = 0.0; OutQuat[2] = 0.0; OutQuat[3] = 0.0;
        return false;
    }

    // Priority 1: quat (w x y z)
    FString QuatStr = Node->GetAttribute(TEXT("quat"));
    if (!QuatStr.IsEmpty())
    {
        TArray<float> Parts;
        MjXmlUtils::ParseFloatArray(QuatStr, Parts);
        if (Parts.Num() >= 4)
        {
            OutQuat[0] = (double)Parts[0];
            OutQuat[1] = (double)Parts[1];
            OutQuat[2] = (double)Parts[2];
            OutQuat[3] = (double)Parts[3];
            QuatNormalize(OutQuat);
            return true;
        }
    }

    // Priority 2: axisangle (x y z angle)
    FString AxisAngleStr = Node->GetAttribute(TEXT("axisangle"));
    if (!AxisAngleStr.IsEmpty())
    {
        TArray<float> Parts;
        MjXmlUtils::ParseFloatArray(AxisAngleStr, Parts);
        if (Parts.Num() >= 4)
        {
            double Angle = (double)Parts[3];
            if (Settings.bAngleInDegrees)
            {
                Angle = FMath::DegreesToRadians(Angle);
            }
            AxisAngleToQuat((double)Parts[0], (double)Parts[1], (double)Parts[2], Angle, OutQuat);
            return true;
        }
    }

    // Priority 3: euler (e1 e2 e3) — sequence from compiler settings
    FString EulerStr = Node->GetAttribute(TEXT("euler"));
    if (!EulerStr.IsEmpty())
    {
        TArray<float> Parts;
        MjXmlUtils::ParseFloatArray(EulerStr, Parts);
        if (Parts.Num() >= 3)
        {
            double e1 = (double)Parts[0];
            double e2 = (double)Parts[1];
            double e3 = (double)Parts[2];

            if (Settings.bAngleInDegrees)
            {
                e1 = FMath::DegreesToRadians(e1);
                e2 = FMath::DegreesToRadians(e2);
                e3 = FMath::DegreesToRadians(e3);
            }

            EulerToQuat(e1, e2, e3, Settings.EulerSeq, OutQuat);
            return true;
        }
    }

    // Priority 4: xyaxes (x1 x2 x3 y1 y2 y3)
    FString XYAxesStr = Node->GetAttribute(TEXT("xyaxes"));
    if (!XYAxesStr.IsEmpty())
    {
        TArray<float> Parts;
        MjXmlUtils::ParseFloatArray(XYAxesStr, Parts);
        if (Parts.Num() >= 6)
        {
            double Vals[6];
            for (int i = 0; i < 6; ++i) Vals[i] = (double)Parts[i];
            XYAxesToQuat(Vals, OutQuat);
            return true;
        }
    }

    // Priority 5: zaxis (z1 z2 z3)
    FString ZAxisStr = Node->GetAttribute(TEXT("zaxis"));
    if (!ZAxisStr.IsEmpty())
    {
        TArray<float> Parts;
        MjXmlUtils::ParseFloatArray(ZAxisStr, Parts);
        if (Parts.Num() >= 3)
        {
            double Vals[3] = { (double)Parts[0], (double)Parts[1], (double)Parts[2] };
            ZAxisToQuat(Vals, OutQuat);
            return true;
        }
    }

    // No orientation attribute found — identity
    OutQuat[0] = 1.0; OutQuat[1] = 0.0; OutQuat[2] = 0.0; OutQuat[3] = 0.0;
    return false;
}

// ============================================================================
// Axis-Angle → Quaternion
// ============================================================================

void MjOrientationUtils::AxisAngleToQuat(double x, double y, double z, double AngleRad, double OutQuat[4])
{
    // Normalize axis
    double Len = FMath::Sqrt(x * x + y * y + z * z);
    if (Len < 1e-10)
    {
        OutQuat[0] = 1.0; OutQuat[1] = 0.0; OutQuat[2] = 0.0; OutQuat[3] = 0.0;
        return;
    }

    double InvLen = 1.0 / Len;
    double nx = x * InvLen;
    double ny = y * InvLen;
    double nz = z * InvLen;

    double HalfAngle = AngleRad * 0.5;
    double s = FMath::Sin(HalfAngle);
    double c = FMath::Cos(HalfAngle);

    OutQuat[0] = c;
    OutQuat[1] = s * nx;
    OutQuat[2] = s * ny;
    OutQuat[3] = s * nz;
    QuatNormalize(OutQuat);
}

// ============================================================================
// Euler → Quaternion
// ============================================================================

void MjOrientationUtils::ElementalRotQuat(int AxisIndex, double AngleRad, double OutQuat[4])
{
    double HalfAngle = AngleRad * 0.5;
    double s = FMath::Sin(HalfAngle);
    double c = FMath::Cos(HalfAngle);

    OutQuat[0] = c;
    OutQuat[1] = 0.0;
    OutQuat[2] = 0.0;
    OutQuat[3] = 0.0;

    // AxisIndex: 0=x, 1=y, 2=z
    OutQuat[1 + AxisIndex] = s;
}

void MjOrientationUtils::EulerToQuat(double e1, double e2, double e3, const FString& EulerSeq, double OutQuat[4])
{
    if (EulerSeq.Len() != 3)
    {
        UE_LOG(LogURLabImport, Warning, TEXT("[MjOrientationUtils] Invalid eulerseq '%s', defaulting to identity"), *EulerSeq);
        OutQuat[0] = 1.0; OutQuat[1] = 0.0; OutQuat[2] = 0.0; OutQuat[3] = 0.0;
        return;
    }

    double Angles[3] = { e1, e2, e3 };
    int AxisIndices[3];
    bool bExtrinsic[3];

    for (int i = 0; i < 3; ++i)
    {
        TCHAR Ch = EulerSeq[i];
        switch (Ch)
        {
            case 'x': AxisIndices[i] = 0; bExtrinsic[i] = false; break;
            case 'y': AxisIndices[i] = 1; bExtrinsic[i] = false; break;
            case 'z': AxisIndices[i] = 2; bExtrinsic[i] = false; break;
            case 'X': AxisIndices[i] = 0; bExtrinsic[i] = true;  break;
            case 'Y': AxisIndices[i] = 1; bExtrinsic[i] = true;  break;
            case 'Z': AxisIndices[i] = 2; bExtrinsic[i] = true;  break;
            default:
                UE_LOG(LogURLabImport, Warning, TEXT("[MjOrientationUtils] Invalid euler axis character '%c'"), Ch);
                AxisIndices[i] = 0; bExtrinsic[i] = false;
                break;
        }
    }

    // Build three elemental rotation quaternions
    double Q1[4], Q2[4], Q3[4];
    ElementalRotQuat(AxisIndices[0], Angles[0], Q1);
    ElementalRotQuat(AxisIndices[1], Angles[1], Q2);
    ElementalRotQuat(AxisIndices[2], Angles[2], Q3);

    // Per-axis composition following MuJoCo convention (user_objects.cc):
    //   Lowercase (intrinsic): rotate in body-fixed frame → accumulate on the right: R = R_prev * Ri
    //   Uppercase (extrinsic): rotate in world-fixed frame → accumulate on the left:  R = Ri * R_prev
    // This handles pure intrinsic, pure extrinsic, and mixed sequences correctly.

    const double* Qi[3] = { Q1, Q2, Q3 };
    // Start with identity
    double Result[4] = { 1.0, 0.0, 0.0, 0.0 };
    double Temp[4];

    for (int i = 0; i < 3; ++i)
    {
        if (bExtrinsic[i])
        {
            // Extrinsic: new = Qi * Result
            QuatMul(Qi[i], Result, Temp);
        }
        else
        {
            // Intrinsic: new = Result * Qi
            QuatMul(Result, Qi[i], Temp);
        }
        Result[0] = Temp[0]; Result[1] = Temp[1]; Result[2] = Temp[2]; Result[3] = Temp[3];
    }

    OutQuat[0] = Result[0]; OutQuat[1] = Result[1]; OutQuat[2] = Result[2]; OutQuat[3] = Result[3];

    QuatNormalize(OutQuat);
}

// ============================================================================
// XY-Axes → Quaternion
// ============================================================================

void MjOrientationUtils::XYAxesToQuat(const double XYAxes[6], double OutQuat[4])
{
    // X axis
    double Xx = XYAxes[0], Xy = XYAxes[1], Xz = XYAxes[2];
    double XLen = FMath::Sqrt(Xx * Xx + Xy * Xy + Xz * Xz);
    if (XLen < 1e-10)
    {
        OutQuat[0] = 1.0; OutQuat[1] = 0.0; OutQuat[2] = 0.0; OutQuat[3] = 0.0;
        return;
    }
    Xx /= XLen; Xy /= XLen; Xz /= XLen;

    // Y axis (orthogonalize against X)
    double Yx = XYAxes[3], Yy = XYAxes[4], Yz = XYAxes[5];
    double Dot = Xx * Yx + Xy * Yy + Xz * Yz;
    Yx -= Dot * Xx;
    Yy -= Dot * Xy;
    Yz -= Dot * Xz;
    double YLen = FMath::Sqrt(Yx * Yx + Yy * Yy + Yz * Yz);
    if (YLen < 1e-10)
    {
        OutQuat[0] = 1.0; OutQuat[1] = 0.0; OutQuat[2] = 0.0; OutQuat[3] = 0.0;
        return;
    }
    Yx /= YLen; Yy /= YLen; Yz /= YLen;

    // Z axis = X cross Y
    double Zx = Xy * Yz - Xz * Yy;
    double Zy = Xz * Yx - Xx * Yz;
    double Zz = Xx * Yy - Xy * Yx;

    // Build rotation matrix (row-major): row 0 = X, row 1 = Y, row 2 = Z
    double R[9] = {
        Xx, Xy, Xz,
        Yx, Yy, Yz,
        Zx, Zy, Zz
    };

    RotMatToQuat(R, OutQuat);
}

// ============================================================================
// Z-Axis → Quaternion (minimal rotation from (0,0,1))
// ============================================================================

void MjOrientationUtils::ZAxisToQuat(const double ZAxis[3], double OutQuat[4])
{
    double Zx = ZAxis[0], Zy = ZAxis[1], Zz = ZAxis[2];
    double Len = FMath::Sqrt(Zx * Zx + Zy * Zy + Zz * Zz);
    if (Len < 1e-10)
    {
        OutQuat[0] = 1.0; OutQuat[1] = 0.0; OutQuat[2] = 0.0; OutQuat[3] = 0.0;
        return;
    }
    Zx /= Len; Zy /= Len; Zz /= Len;

    // Source direction: (0, 0, 1)
    // Target direction: (Zx, Zy, Zz)
    // Rotation axis = cross(source, target), angle = acos(dot(source, target))

    double DotVal = Zz; // dot((0,0,1), (Zx,Zy,Zz)) = Zz

    if (DotVal > 0.9999999)
    {
        // Nearly identical — identity rotation
        OutQuat[0] = 1.0; OutQuat[1] = 0.0; OutQuat[2] = 0.0; OutQuat[3] = 0.0;
        return;
    }

    if (DotVal < -0.9999999)
    {
        // Nearly opposite — 180 degree rotation around any perpendicular axis
        // Choose X axis as the rotation axis
        OutQuat[0] = 0.0; OutQuat[1] = 1.0; OutQuat[2] = 0.0; OutQuat[3] = 0.0;
        return;
    }

    // cross((0,0,1), (Zx,Zy,Zz)) = (-Zy, Zx, 0)
    double Ax = -Zy;
    double Ay = Zx;
    double Az = 0.0;

    double Angle = FMath::Acos(FMath::Clamp(DotVal, -1.0, 1.0));
    AxisAngleToQuat(Ax, Ay, Az, Angle, OutQuat);
}

// ============================================================================
// Quaternion Helpers
// ============================================================================

void MjOrientationUtils::QuatMul(const double A[4], const double B[4], double Result[4])
{
    // Hamilton product: A * B  with layout [w, x, y, z]
    Result[0] = A[0]*B[0] - A[1]*B[1] - A[2]*B[2] - A[3]*B[3];
    Result[1] = A[0]*B[1] + A[1]*B[0] + A[2]*B[3] - A[3]*B[2];
    Result[2] = A[0]*B[2] - A[1]*B[3] + A[2]*B[0] + A[3]*B[1];
    Result[3] = A[0]*B[3] + A[1]*B[2] - A[2]*B[1] + A[3]*B[0];
}

void MjOrientationUtils::QuatNormalize(double Q[4])
{
    double Len = FMath::Sqrt(Q[0]*Q[0] + Q[1]*Q[1] + Q[2]*Q[2] + Q[3]*Q[3]);
    if (Len < 1e-10)
    {
        Q[0] = 1.0; Q[1] = 0.0; Q[2] = 0.0; Q[3] = 0.0;
        return;
    }
    double InvLen = 1.0 / Len;
    Q[0] *= InvLen; Q[1] *= InvLen; Q[2] *= InvLen; Q[3] *= InvLen;
}

void MjOrientationUtils::RotMatToQuat(const double R[9], double OutQuat[4])
{
    // Rotation matrix in row-major order:
    //   R[0] R[1] R[2]     =  row 0 (X axis)
    //   R[3] R[4] R[5]     =  row 1 (Y axis)
    //   R[6] R[7] R[8]     =  row 2 (Z axis)
    //
    // MuJoCo stores rotation matrices as 3x3 row-major where columns are frame axes.
    // For xyaxes, we constructed R with rows = axes, so the rotation matrix for MuJoCo
    // frame would have the columns as the axes. Since our R has rows as axes,
    // the equivalent rotation matrix (transforms world to local) has columns as the axes.
    // We need to convert from this rotation matrix to quaternion.
    //
    // Standard Shepperd method for robust matrix-to-quaternion conversion:

    double Trace = R[0] + R[4] + R[8];

    if (Trace > 0.0)
    {
        double s = FMath::Sqrt(Trace + 1.0) * 2.0; // s = 4*w
        OutQuat[0] = 0.25 * s;
        OutQuat[1] = (R[5] - R[7]) / s;  // (R[1][2] - R[2][1]) / s
        OutQuat[2] = (R[6] - R[2]) / s;  // (R[2][0] - R[0][2]) / s
        OutQuat[3] = (R[1] - R[3]) / s;  // (R[0][1] - R[1][0]) / s
    }
    else if (R[0] > R[4] && R[0] > R[8])
    {
        double s = FMath::Sqrt(1.0 + R[0] - R[4] - R[8]) * 2.0; // s = 4*x
        OutQuat[0] = (R[5] - R[7]) / s;
        OutQuat[1] = 0.25 * s;
        OutQuat[2] = (R[1] + R[3]) / s;
        OutQuat[3] = (R[2] + R[6]) / s;
    }
    else if (R[4] > R[8])
    {
        double s = FMath::Sqrt(1.0 + R[4] - R[0] - R[8]) * 2.0; // s = 4*y
        OutQuat[0] = (R[6] - R[2]) / s;
        OutQuat[1] = (R[1] + R[3]) / s;
        OutQuat[2] = 0.25 * s;
        OutQuat[3] = (R[5] + R[7]) / s;
    }
    else
    {
        double s = FMath::Sqrt(1.0 + R[8] - R[0] - R[4]) * 2.0; // s = 4*z
        OutQuat[0] = (R[1] - R[3]) / s;
        OutQuat[1] = (R[2] + R[6]) / s;
        OutQuat[2] = (R[5] + R[7]) / s;
        OutQuat[3] = 0.25 * s;
    }

    QuatNormalize(OutQuat);
}
