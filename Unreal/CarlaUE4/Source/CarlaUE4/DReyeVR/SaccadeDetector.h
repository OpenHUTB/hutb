#pragma once

#include "CoreMinimal.h"

/// Sample-level I-VT (Identification by Velocity Threshold) saccade detector for the
/// high-rate gaze stream.
///
/// Angular speed comes from each sample's own device timestamp, never from a nominal 1/Hz
/// period: the tracker runs on an ~8.3 ms grid but skips a grid point on roughly 1.8% of
/// consecutive samples, and assuming the nominal period would mis-scale exactly those
/// velocities - and with them the peak of any event they fall inside.
///
/// Carries no file or world state, so tests can drive it with a synthetic sample vector.
class FDReyeVRSaccadeDetector
{
  public:
    struct FConfig
    {
        float OnsetDegS = 120.f;           // deg/s: fixation -> saccade
        float OffsetDegS = 60.f;           // deg/s: saccade  -> fixation
        double MinDurationSeconds = 0.020; // shorter closures are noise, not saccades
        float MinAmplitudeDeg = 0.5f;
    };

    FDReyeVRSaccadeDetector() = default;
    explicit FDReyeVRSaccadeDetector(const FConfig &InConfig) : Config(InConfig) {}

    /// Consumes one eye-in-head gaze angle pair. HRad/VRad are radians; DeviceTimeS is the
    /// tracker's own clock in seconds and is the only time base used for velocities. SimTimeS
    /// is carried along so an event can be aligned with the driving rows.
    void AddSample(double DeviceTimeS, double SimTimeS, float HRad, float VRad);

    /// The event getters below describe the event that is currently open, or the most recent
    /// one that closed: a caller can still read the final statistics of a saccade on the
    /// sample that ended it.
    bool IsSaccade() const { return bInSaccade; }
    double OnsetDeviceTimeS() const { return OnsetDeviceTime; }
    double OnsetSimTimeS() const { return OnsetSimTime; }
    float AmplitudeDeg() const { return EventAmplitudeDeg; }
    float PeakDegS() const { return EventPeakDegS; }
    int32 SampleCount() const { return EventSamples; }

    /// True only for the single AddSample() call that closed an event. Whether an event
    /// passes the duration/amplitude floors is only knowable at closure, so a caller that
    /// writes one row per sample must hold its rows until this fires: in a 165.9 s real
    /// session 39.5% of the runs labelled "saccade" by IsSaccade() alone are shorter than
    /// the 20 ms floor the detector itself rejects.
    bool JustClosedEvent() const { return bJustClosedEvent; }
    /// Whether the event that has just closed passed both floors. Only meaningful together
    /// with JustClosedEvent(); it describes the most recent closure.
    bool LastEventAccepted() const { return bLastEventAccepted; }

    /// Angular speed of the most recent sample. False before the second sample and whenever
    /// the device timestamp did not advance, in which case no speed can be computed.
    bool HasAngularSpeed() const { return bHasVelocity; }
    float AngularSpeedDegS() const { return LastVelocityDegS; }

    int32 GetCompletedEvents() const { return CompletedEvents; }
    /// Events closed below the duration/amplitude floors: counted for diagnostics only,
    /// never reported as saccades.
    int32 GetRejectedEvents() const { return RejectedEvents; }
    /// Device time of the last sample of the most recent accepted event.
    double LastEndTimeS() const { return LastEndTimeDevice; }

    void Reset();

  private:
    void TrackAmplitude(float HRad, float VRad);
    void CloseEvent();

    FConfig Config;

    bool bHasPrevious = false;
    float PrevHRad = 0.f, PrevVRad = 0.f;
    double PrevDeviceTime = 0.0, PrevSimTime = 0.0;

    bool bHasVelocity = false;
    float LastVelocityDegS = 0.f;

    bool bInSaccade = false;
    bool bJustClosedEvent = false;
    bool bLastEventAccepted = false;
    double OnsetDeviceTime = -1.0, OnsetSimTime = -1.0;
    double EventEndDeviceTime = 0.0;
    float StartHRad = 0.f, StartVRad = 0.f;
    float EventAmplitudeDeg = 0.f;
    float EventPeakDegS = 0.f;
    int32 EventSamples = 0;

    int32 CompletedEvents = 0;
    int32 RejectedEvents = 0;
    double LastEndTimeDevice = -1.0;
};
