#pragma once

#include "CoreMinimal.h"

namespace DReyeVR
{

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

    /// Release the device. Safe to call repeatedly and after a failed Initialize().
    virtual void Shutdown() = 0;
};

} // namespace DReyeVR
