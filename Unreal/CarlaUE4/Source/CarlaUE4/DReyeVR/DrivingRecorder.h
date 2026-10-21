#pragma once

#include "CoreMinimal.h"
#include "Carla/Sensor/DReyeVRData.h"
#include "EyeTracking/IEyeSource.h"
#include "SaccadeDetector.h"
#include "Serialization/Archive.h"

class AEgoVehicle;
class UWorld;

// Sample only on the game thread, immediately after the existing gaze/vehicle snapshot.
// No second raycast, extra rendering or network client. The eye tracker runs on a ~120 Hz
// grid while the render loop can be far slower, so gaze goes to its own file instead of
// being squeezed into the driving rows (which would either duplicate or drop samples).
class FDrivingRecorder
{
  public:
    /// The five event columns appended to a gaze row (saccade_state plus the four event
    /// fields). An event's rows are written when it closes, so all rows of one event carry
    /// the same final values; rows outside an event get an empty stamp.
    struct FGazeEventStamp
    {
        FString State = TEXT("fixation"); // fixation | saccade | rejected | saccade_open
        FString OnsetSimTimeS;            // below the floors, or for an event that never
        FString AmplitudeDeg;             // closed, these stay empty and only Samples is
        FString PeakDegS;                 // filled: a reader can see what was discarded
        FString Samples;
    };

    /// Append the stamp's five columns to every row of one event. Pure: no file, no world,
    /// no GPU, so the verdict logic is unit-testable on rows built in memory.
    static void StampEventRows(TArray<TArray<FString>> &Rows, const FGazeEventStamp &Stamp);

    ~FDrivingRecorder() { Close(); }
    void Start();
    void Record(AEgoVehicle &Vehicle, const DReyeVR::AggregateData &Data,
                const DReyeVR::FocusInfo &Focus, const FString &EyeSource);
    /// Append every gaze sample produced since the previous call, oldest first. Samples
    /// at or before the last written device timestamp are dropped, so callers may pass
    /// overlapping frames without duplicating rows. Returns the number of rows written.
    /// SimTime uses the same clock as the driving rows so the two streams align.
    /// The rows of an open saccade are held back until the detector closes the event and can
    /// report a verdict, so the returned count can lag the samples handed in by up to
    /// MaxBufferedEventRows; Close() writes whatever is still held.
    int32 RecordGaze(const TArray<DReyeVR::FEyeSample> &Samples, UWorld &World, uint64 Frame);
    void Close();
    bool IsRecording() const { return File.IsValid(); }
    /// Returns the CSV path minus the .csv extension; empty when not recording.
    FString GetSessionBasePath() const;

  private:
    void Write(const FString &Text);
    void WriteGaze(const FString &Text);
    /// Stamp buffered rows with one verdict and write them; returns the number written.
    int32 WriteGazeRows(TArray<TArray<FString>> &Rows, const FGazeEventStamp &Stamp);
    TUniquePtr<FArchive> File;
    TUniquePtr<FArchive> GazeFile;
    FString GazePath;
    double LastGazeSourceTime = -1.0;
    uint64 GazeRows = 0;
    // Header widths are counted once so every row can be checked against them: a
    // mismatched column silently shifts the rest of the record instead of failing.
    int32 CsvFieldCount = 0;
    int32 GazeCsvFieldCount = 0;
    FString Path;
    double StartHost = 0.0, LastSample = -1.0, NextSample = 0.0, LastFlush = 0.0, LastGazeFlush = 0.0;
    double Interval = 0.05;
    float LastYaw = 0.f;

    // --- Saccade detection (I-VT: Identification by Velocity Threshold) ---
    enum class ESaccadeState : uint8 { Fixation, Saccade };
    static constexpr float SaccadeOnsetThreshold = 30.f;  // deg/s: fixation -> saccade
    static constexpr float SaccadeOffsetThreshold = 20.f; // deg/s: saccade  -> fixation
    ESaccadeState SaccadeState = ESaccadeState::Fixation;
    FVector LastGazeLocal = FVector::ZeroVector; // previous frame gaze_local (for angular velocity)
    bool bHasLastGaze = false;
    double SaccadeOnsetTime = -1.0;   // sim_time when current saccade started
    FVector SaccadeStartDir = FVector::ZeroVector; // gaze direction at saccade onset
    float SaccadePeakVelocity = 0.f;  // peak angular velocity during current saccade (deg/s)

    // Sample-rate-correct I-VT detector for the high-rate gaze stream (FDReyeVRSaccadeDetector).
    // Null unless the high-rate eye pipeline is on, in which case the legacy per-frame state
    // machine above keeps serving the driving rows exactly as before.
    TUniquePtr<FDReyeVRSaccadeDetector> GazeEvents;

    // The rows of the event that is currently open. Their verdict only exists once the
    // detector closes the event, so they cannot be written any earlier; they are contiguous
    // at the tail of the stream, so stamping them later never reorders the file.
    TArray<TArray<FString>> PendingGazeRows;
    bool bEventOverflowed = false;
    /// Hard cap on that buffer: 100 rows is ~0.8 s on the device's own timestamp grid, far
    /// past any real saccade, so a longer run is flushed mid-stream as saccade_open rather
    /// than buffered forever.
    static constexpr int32 MaxBufferedEventRows = 100;
};
