#include "SaccadeDetector.h"

void FDReyeVRSaccadeDetector::AddSample(double DeviceTimeS, double SimTimeS, float HRad, float VRad)
{
    bJustClosedEvent = false; // describes this call only
    if (!FMath::IsFinite(DeviceTimeS) || !FMath::IsFinite(SimTimeS) || !FMath::IsFinite(HRad) || !FMath::IsFinite(VRad))
        return;

    if (!bHasPrevious)
    {
        PrevDeviceTime = DeviceTimeS;
        PrevSimTime = SimTimeS;
        PrevHRad = HRad;
        PrevVRad = VRad;
        bHasPrevious = true;
        return; // no speed is definable without a predecessor
    }

    const double Delta = DeviceTimeS - PrevDeviceTime;
    if (!(Delta > 0.0)) return; // repeated/rewound device time: keep the previous baseline

    // Shortest signed angle per axis, then the 2D magnitude. This is the metric the offline
    // threshold sweep measured the noise band with, so the configured degrees/second mean
    // what they were calibrated against.
    const float DeltaH =
        FMath::FindDeltaAngleDegrees(FMath::RadiansToDegrees(PrevHRad), FMath::RadiansToDegrees(HRad));
    const float DeltaV =
        FMath::FindDeltaAngleDegrees(FMath::RadiansToDegrees(PrevVRad), FMath::RadiansToDegrees(VRad));
    const float AngleDeg = FMath::Sqrt(DeltaH * DeltaH + DeltaV * DeltaV);
    const float Velocity = static_cast<float>(AngleDeg / Delta); // Delta is device time, never 1/Hz
    if (!FMath::IsFinite(Velocity)) return;

    bHasVelocity = true;
    LastVelocityDegS = Velocity;

    if (!bInSaccade)
    {
        if (Velocity > Config.OnsetDegS)
        {
            bInSaccade = true;
            OnsetDeviceTime = PrevDeviceTime; // the eye still held the fixation on that sample
            OnsetSimTime = PrevSimTime;
            StartHRad = PrevHRad;
            StartVRad = PrevVRad;
            EventAmplitudeDeg = 0.f;
            EventPeakDegS = Velocity;
            EventSamples = 2; // the baseline sample plus this one
            EventEndDeviceTime = DeviceTimeS;
            TrackAmplitude(HRad, VRad);
        }
    }
    else if (Velocity < Config.OffsetDegS)
    {
        ++EventSamples;
        EventEndDeviceTime = DeviceTimeS;
        TrackAmplitude(HRad, VRad); // the target is reached on the sample that stopped moving
        CloseEvent();
    }
    else
    {
        ++EventSamples;
        EventEndDeviceTime = DeviceTimeS;
        if (Velocity > EventPeakDegS) EventPeakDegS = Velocity;
        TrackAmplitude(HRad, VRad);
    }

    PrevDeviceTime = DeviceTimeS;
    PrevSimTime = SimTimeS;
    PrevHRad = HRad;
    PrevVRad = VRad;
}

void FDReyeVRSaccadeDetector::TrackAmplitude(float HRad, float VRad)
{
    const float DeltaH =
        FMath::FindDeltaAngleDegrees(FMath::RadiansToDegrees(StartHRad), FMath::RadiansToDegrees(HRad));
    const float DeltaV =
        FMath::FindDeltaAngleDegrees(FMath::RadiansToDegrees(StartVRad), FMath::RadiansToDegrees(VRad));
    const float Distance = FMath::Sqrt(DeltaH * DeltaH + DeltaV * DeltaV);
    if (Distance > EventAmplitudeDeg) EventAmplitudeDeg = Distance;
}

void FDReyeVRSaccadeDetector::CloseEvent()
{
    bInSaccade = false;
    bJustClosedEvent = true;
    const double Duration = EventEndDeviceTime - OnsetDeviceTime;
    if (Duration >= Config.MinDurationSeconds && EventAmplitudeDeg >= Config.MinAmplitudeDeg)
    {
        ++CompletedEvents;
        LastEndTimeDevice = EventEndDeviceTime;
        bLastEventAccepted = true;
    }
    else
    {
        ++RejectedEvents; // a sub-floor blip is discarded silently, not reported as a saccade
        bLastEventAccepted = false;
    }
}

void FDReyeVRSaccadeDetector::Reset()
{
    bHasPrevious = false;
    bJustClosedEvent = false;
    bLastEventAccepted = false;
    PrevHRad = PrevVRad = 0.f;
    PrevDeviceTime = PrevSimTime = 0.0;
    bHasVelocity = false;
    LastVelocityDegS = 0.f;
    bInSaccade = false;
    OnsetDeviceTime = OnsetSimTime = -1.0;
    EventEndDeviceTime = 0.0;
    StartHRad = StartVRad = 0.f;
    EventAmplitudeDeg = 0.f;
    EventPeakDegS = 0.f;
    EventSamples = 0;
    CompletedEvents = 0;
    RejectedEvents = 0;
    LastEndTimeDevice = -1.0;
}

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"

namespace
{
constexpr double SampleDt = 1.0 / 120.0; // the tracker's grid
constexpr int32 RampStartStep = 36;      // 300 ms of fixation at 0 deg in front of the saccade
constexpr int32 RampEndStep = 41; // 5 steps = 41.7 ms for 8 deg -> 192 deg/s on every interval
constexpr int32 LastStep = 80;    // and ~330 ms of fixation at 8 deg behind it

float WaveformHDeg(int32 Step)
{
    if (Step <= RampStartStep) return 0.f;
    if (Step >= RampEndStep) return 8.f;
    return 8.f * (Step - RampStartStep) / static_cast<float>(RampEndStep - RampStartStep);
}

/// The ramp waveform at 120 Hz. Dropping one step turns that interval into 16.7 ms, the way
/// a skipped poll looks on the real device; the angle profile is time-based, so the true
/// speed across the gap stays 192 deg/s while a nominal-1/120 s assumption would read 384.
void FeedWaveform(FDReyeVRSaccadeDetector &Detector, int32 SkippedStep)
{
    for (int32 Step = 0; Step <= LastStep; ++Step)
    {
        if (Step == SkippedStep) continue;
        Detector.AddSample(Step * SampleDt, 100.0 + Step * SampleDt, FMath::DegreesToRadians(WaveformHDeg(Step)), 0.f);
    }
}

/// Deterministic jitter: a failing run has to reproduce exactly, so the global RNG is out.
class FFixedRandom
{
  public:
    explicit FFixedRandom(uint32 Seed) : State(Seed ? Seed : 1u) {}
    float Uniform(float Min, float Max)
    {
        State ^= State << 13;
        State ^= State >> 17;
        State ^= State << 5;
        return Min + (Max - Min) * static_cast<float>(State) / static_cast<float>(0xFFFFFFFFu);
    }

  private:
    uint32 State;
};

void FeedNoise(FDReyeVRSaccadeDetector &Detector, float AmplitudeDeg, int32 StepCount)
{
    FFixedRandom Rand(0x51ED2701u);
    for (int32 Step = 0; Step < StepCount; ++Step)
    {
        const float H = Rand.Uniform(-AmplitudeDeg, AmplitudeDeg);
        const float V = Rand.Uniform(-AmplitudeDeg, AmplitudeDeg);
        Detector.AddSample(Step * SampleDt, Step * SampleDt, FMath::DegreesToRadians(H), FMath::DegreesToRadians(V));
    }
}

bool InRange(double Value, double Min, double Max)
{
    return Value >= Min && Value <= Max;
}
} // namespace

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDReyeVRSaccadeDetectorTest, "HUTB.DReyeVR.Gaze.SaccadeDetector",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDReyeVRSaccadeDetectorTest::RunTest(const FString &Parameters)
{
    const FDReyeVRSaccadeDetector::FConfig Calibrated; // the values the offline sweep picked

    // 1. Fixation - saccade - fixation gives exactly one event with physiological numbers.
    FDReyeVRSaccadeDetector Clean(Calibrated);
    FeedWaveform(Clean, -1);
    TestEqual(TEXT("One completed event"), Clean.GetCompletedEvents(), 1);
    TestEqual(TEXT("Nothing rejected"), Clean.GetRejectedEvents(), 0);
    TestFalse(TEXT("State machine back in fixation"), Clean.IsSaccade());
    TestTrue(TEXT("Duration within the 40 ms range"),
             InRange(Clean.LastEndTimeS() - Clean.OnsetDeviceTimeS(), 0.020, 0.080));
    TestTrue(TEXT("Amplitude tracks the 8 deg step"), InRange(Clean.AmplitudeDeg(), 6.f, 12.f));
    TestTrue(TEXT("Peak above 150 deg/s"), Clean.PeakDegS() > 150.f);
    TestTrue(TEXT("At least three samples in the event"), Clean.SampleCount() >= 3);
    const float CleanPeak = Clean.PeakDegS();

    // 2. A 16.7 ms gap inside the saccade must not move the peak: pinning the rule that
    //    intervals come from the device timestamps rather than the nominal period.
    FDReyeVRSaccadeDetector Gapped(Calibrated);
    FeedWaveform(Gapped, RampStartStep + 3);
    TestEqual(TEXT("Gap still yields one completed event"), Gapped.GetCompletedEvents(), 1);
    TestTrue(TEXT("Gap keeps at least three samples in the event"), Gapped.SampleCount() >= 3);
    TestTrue(TEXT("Peak within 25% of the gap-free peak"), FMath::Abs(Gapped.PeakDegS() - CleanPeak) <= 0.25f * CleanPeak);
    TestTrue(TEXT("Amplitude still the 8 deg step"), InRange(Gapped.AmplitudeDeg(), 6.f, 12.f));

    // 3. Tiny fixation jitter stays out of the event list entirely.
    FDReyeVRSaccadeDetector Quiet(Calibrated);
    FeedNoise(Quiet, 0.03f, 600);
    TestEqual(TEXT("No completed events from +/-0.03 deg jitter"), Quiet.GetCompletedEvents(), 0);
    TestEqual(TEXT("Nothing even rejected"), Quiet.GetRejectedEvents(), 0);
    TestFalse(TEXT("Never entered a saccade"), Quiet.IsSaccade());

    // 4. Jitter strong enough to reach the measured noise band (the real recordings hit
    //    p90 70-100 deg/s inside fixation; +/-0.3 deg on a 1/120 s grid tops out at 102
    //    deg/s). The calibrated floors report nothing, while the legacy 30/20 thresholds
    //    sat inside that band and reported events - which is why they were replaced.
    const int32 NoiseSteps = 600;
    FDReyeVRSaccadeDetector BandCalibrated(Calibrated);
    FeedNoise(BandCalibrated, 0.3f, NoiseSteps);
    TestEqual(TEXT("Calibrated settings reject the whole noise band as events"), BandCalibrated.GetCompletedEvents(), 0);
    TestFalse(TEXT("Calibrated settings never leave fixation"), BandCalibrated.IsSaccade());

    FDReyeVRSaccadeDetector::FConfig Legacy;
    Legacy.OnsetDegS = 30.f;
    Legacy.OffsetDegS = 20.f;
    Legacy.MinDurationSeconds = 0.0;
    Legacy.MinAmplitudeDeg = 0.f;
    FDReyeVRSaccadeDetector BandLegacy(Legacy);
    FeedNoise(BandLegacy, 0.3f, NoiseSteps);
    TestTrue(TEXT("Legacy thresholds report events on the same noise"), BandLegacy.GetCompletedEvents() > 0);

    // 5. A slow drift below every threshold is not a saccade, and its speed is measured right.
    FDReyeVRSaccadeDetector Drift(Calibrated);
    for (int32 Step = 0; Step <= LastStep; ++Step)
    {
        const float StepHDeg = static_cast<float>(5.0 * Step * SampleDt); // 5 deg/s in degrees per step
        Drift.AddSample(Step * SampleDt, Step * SampleDt, FMath::DegreesToRadians(StepHDeg), 0.f);
    }
    TestEqual(TEXT("No event from a 5 deg/s drift"), Drift.GetCompletedEvents(), 0);
    TestFalse(TEXT("Drift never opens an event"), Drift.IsSaccade());
    TestTrue(TEXT("Drift measured at about 5 deg/s"), FMath::Abs(Drift.AngularSpeedDegS() - 5.f) < 0.1f);
    return true;
}
#endif // WITH_DEV_AUTOMATION_TESTS
