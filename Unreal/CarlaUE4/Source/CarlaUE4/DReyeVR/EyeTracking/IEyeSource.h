#pragma once

#include "CoreMinimal.h"

namespace DReyeVR
{

/// One eye's visual axis, in the EgoVehicle camera's local space.
struct FEyeGaze
{
    /// Unit direction of this eye. Only meaningful when bValid.
    FVector Direction = FVector::ZeroVector;
    /// Horizontal / vertical rotation in radians, after the backend's sign fixes.
    float HRad = 0.f;
    float VRad = 0.f;
    bool bValid = false;
};

/// One eye-tracking sample, expressed in the EgoVehicle camera's local space
/// (UE4 convention: +X forward, +Y right, +Z up).
struct FEyeSample
{
    /// Gaze ray origin in camera-local space. The Pimax PVR 1.26 API does not expose a
    /// physical eye origin, so backends without one report ZeroVector (gaze assumed to
    /// start at the camera origin) and consumers must treat it as an approximation.
    FVector LocalOrigin = FVector::ZeroVector;

    /// Normalized combined (cyclopean) gaze direction in camera-local space.
    FVector LocalDirection = FVector::ForwardVector;

    /// Per-eye gaze. PVR reports both eyes' angles, so averaging them away would throw
    /// away the only information that makes vergence computable.
    FEyeGaze Left;
    FEyeGaze Right;

    /// Angle between the two eyes' axes in radians: positive when converged, zero when
    /// parallel, negative when diverged. Returns 0 when either eye is missing.
    float VergenceRad() const
    {
        if (!Left.bValid || !Right.bValid)
            return 0.f;
        const float Dot = FMath::Clamp(FVector::DotProduct(Left.Direction, Right.Direction), -1.f, 1.f);
        const float Sign = FMath::Sign(Left.HRad - Right.HRad); // eyes crossed => left points right of right
        return FMath::Acos(Dot) * (Sign == 0.f ? 1.f : Sign);
    }

    /// True only when every validity gate passed (call success, finite values,
    /// normalizable direction, fresh timestamp, device state OK). Consumers must
    /// check this before using the sample.
    bool bValid = false;

    /// Device-clock timestamp in seconds. Only meaningful when bSourceTimeKnown.
    double SourceTime = 0.0;
    bool bSourceTimeKnown = false;

    /// Host time (FPlatformTime::Seconds) when this sample was polled.
    double ReceivedTime = 0.0;

    /// Monotonic counter, incremented once per PollLatest() call.
    uint64 Sequence = 0;
};

/// Abstract eye-tracking data source. All methods are called from the game thread.
class IEyeSource
{
  public:
    virtual ~IEyeSource() = default;

    /// Connect to the device. Returns true when samples can be expected.
    virtual bool Initialize() = 0;

    /// Fetch the most recent sample. Never blocks waiting for new data; returns an
    /// invalid sample (bValid == false) whenever fresh data is unavailable.
    virtual FEyeSample PollLatest() = 0;

    /// Every sample available since the previous call, oldest first. The list always
    /// holds exactly one entry per call when the backend cannot recover the samples a
    /// poll skipped, so callers can use Last() as a drop-in for PollLatest().
    /// The device usually runs faster than the render loop; polling once per frame
    /// silently discards everything in between.
    virtual TArray<FEyeSample> PollAllAvailable()
    {
        TArray<FEyeSample> Out;
        Out.Add(PollLatest());
        return Out;
    }

    /// Release the device. Safe to call repeatedly and after a failed Initialize().
    virtual void Shutdown() = 0;
};

} // namespace DReyeVR
