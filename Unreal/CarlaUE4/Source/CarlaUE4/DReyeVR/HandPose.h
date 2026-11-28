#pragma once

#include "CoreMinimal.h"

/// One controller's pose in the OpenVR standing tracking space (metres) plus its
/// legacy trigger/grip axes. Poses are tracking-space, NOT UE world space: the
/// offline musculoskeletal replay consumes them directly.
struct FDReyeVRHandPose
{
    bool bValid = false;
    FVector Location = FVector::ZeroVector;   // metres
    FRotator Rotation = FRotator::ZeroRotator; // degrees
    float Trigger = -1.f;                      // -1 = unknown
    float Grip = -1.f;                         // -1 = unknown
};

struct FDReyeVRHandPosePair
{
    FDReyeVRHandPose Left;
    FDReyeVRHandPose Right;
};

/// Decomposes a right-handed OpenVR 3x4 pose matrix (X right, Y up, Z back,
/// column-vector basis) into intrinsic Y-X-Z Euler angles (Yaw around +Y,
/// Pitch around +X, Roll around +Z) in degrees, stored in FRotator(Pitch, Yaw, Roll).
FRotator OpenVRMatrixToRotator(const float M[3][4]);

/// Samples both controllers. Returns false when no SteamVR session is available
/// (non-Windows builds, or the runtime is not initialized).
bool SampleHandPoses(FDReyeVRHandPosePair &Out);

/// Appends the 18 CSV fields (9 per hand: valid,x,y,z,pitch,yaw,roll,trigger,grip)
/// to Row. Empty strings when the pose is invalid, so the column count never changes.
void AppendHandPoseColumns(TArray<FString> &Row, const FDReyeVRHandPosePair &Poses);

/// One-shot VR & steering calibration detector: waits until the driver's head stays
/// within (MaxHeadPosDriftCm, MaxHeadAngleDriftDeg) continuously for >= RequiredStillSeconds,
/// and then triggers once as soon as the steering wheel rotates by >= SteerStartDelta
/// from its resting angle. Pure state machine (no engine globals) so it is unit-testable.
struct FDReyeVRAutoCalibrator
{
    struct FConfig
    {
        double RequiredStillSeconds = 5.0;
        float MaxHeadPosDriftCm = 5.0f;
        float MaxHeadAngleDriftDeg = 6.0f;
        float QuietSteerRatePerSec = 0.008f;
        float SteerStartDelta = 0.015f; // ~6.8 deg on a 900 deg lock-to-lock wheel
    };

    FDReyeVRAutoCalibrator() = default;
    explicit FDReyeVRAutoCalibrator(const FConfig &InConfig) : Config(InConfig) {}

    bool Tick(double DeltaSeconds, const FVector &HeadPosCm, const FRotator &HeadRot, float NormalizedSteer);
    bool HasCalibrated() const { return bCalibrated; }
    double GetStillSeconds() const { return StillSeconds; }

  private:
    FConfig Config;
    FVector AnchorPosCm = FVector::ZeroVector;
    FQuat AnchorQuat = FQuat::Identity;
    double StillSeconds = 0.0;
    float SteerRest = 0.f;
    float PrevSteer = 0.f;
    bool bHasAnchor = false;
    bool bCalibrated = false;
};
