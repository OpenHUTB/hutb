#include "DrivingRecorder.h"
#include "DrivingStudy.h"
#include "EgoVehicle.h"
#include "DReyeVRPawn.h"
#include "CarlaUE4.h"
#include "Carla/Game/CarlaStatics.h"
#include "Carla/Traffic/TrafficLightBase.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformTime.h"
#include "Misc/Paths.h"
#include "Misc/Guid.h"
#include "GameFramework/Character.h"

namespace DrivingStudy
{
inline FString Csv(FString Value)
{
    Value.ReplaceInline(TEXT("\""), TEXT("\"\""));
    return TEXT("\"") + Value + TEXT("\"");
}

inline FString Motion(float SpeedKmh, float YawRateDegS)
{
    if (SpeedKmh < 1.f) return TEXT("stopped");
    if (YawRateDegS > 3.f) return TEXT("right");
    if (YawRateDegS < -3.f) return TEXT("left");
    return TEXT("straight");
}
} // namespace DrivingStudy

namespace
{
FString Number(double Value) { return FMath::IsFinite(Value) ? FString::Printf(TEXT("%.6f"), Value) : TEXT(""); }
void Vector(TArray<FString> &Row, FVector Value, bool Valid = true)
{
    Row.Add(Valid ? Number(Value.X) : TEXT(""));
    Row.Add(Valid ? Number(Value.Y) : TEXT(""));
    Row.Add(Valid ? Number(Value.Z) : TEXT(""));
}
FVector GlobalLocation(UWorld *World, const FVector &Local)
{
    const auto *LargeMap = UCarlaStatics::GetLargeMapManager(World);
    return LargeMap ? LargeMap->LocalToGlobalLocation(Local) : Local;
}
FString ActorId(UCarlaEpisode *Episode, AActor *Actor)
{
    const auto *Registered = Episode && Actor ? Episode->FindCarlaActor(Actor) : nullptr;
    return Registered ? FString::FromInt(Registered->GetActorId()) : TEXT("");
}
FString LightState(ATrafficLightBase *Light)
{
    if (!Light) return TEXT("unknown");
    switch (Light->GetTrafficLightState())
    {
    case ETrafficLightState::Red: return TEXT("red");
    case ETrafficLightState::Yellow: return TEXT("yellow");
    case ETrafficLightState::Green: return TEXT("green");
    default: return TEXT("off");
    }
}
/// Field count of a CSV header line (trailing newline is irrelevant to the comma count).
int32 CountFields(const FString &HeaderLine)
{
    int32 Commas = 0;
    for (int32 i = 0; i < HeaderLine.Len(); ++i)
        if (HeaderLine[i] == TEXT(','))
            ++Commas;
    return Commas + 1;
}
/// Optional numeric override of one detector parameter. A missing or non-finite value leaves
/// the calibrated default alone instead of silently turning detection off.
void ReadOverride(const TCHAR *Key, float &Value)
{
    float Parsed = 0.f;
    if (FParse::Value(FCommandLine::Get(), Key, Parsed) && FMath::IsFinite(static_cast<double>(Parsed)))
        Value = Parsed;
}
/// The session's time base: shared by the driving rows and the gaze stream so the two
/// files can be joined without a third clock in the middle.
double SimTimeOf(UWorld *World)
{
    if (!World) return 0.0;
    auto *Episode = UCarlaStatics::GetCurrentEpisode(World);
    return Episode ? Episode->GetElapsedGameTime() : World->GetTimeSeconds();
}
}

void FDrivingRecorder::Start()
{
    if (!FParse::Param(FCommandLine::Get(), TEXT("DReyeVRRecord"))) return;
    FString Directory;
    FParse::Value(FCommandLine::Get(), TEXT("DReyeVRRecordDir="), Directory);
    if (Directory.IsEmpty()) Directory = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("DrivingStudy"));
    Directory = FPaths::ConvertRelativePathToFull(Directory);
    float Hz = 120.f;
    FParse::Value(FCommandLine::Get(), TEXT("DReyeVRRecordHz="), Hz);
    Interval = 1.0 / (FMath::IsFinite(Hz) ? FMath::Clamp(Hz, 1.f, 500.f) : 120.f);
    Path = FPaths::Combine(Directory, FDateTime::UtcNow().ToString(TEXT("%Y%m%dT%H%M%SZ")) + TEXT("-") +
                           FGuid::NewGuid().ToString(EGuidFormats::Digits) + TEXT(".csv"));
    if (IFileManager::Get().MakeDirectory(*Directory, true))
        File.Reset(IFileManager::Get().CreateFileWriter(*Path, FILEWRITE_NoReplaceExisting));
    if (!File)
    {
        LOG_ERROR("Driving study: cannot create CSV at %s; recording is OFF", *Path);
        return;
    }
    // UTF-8 BOM allows Excel to open non-English map/actor names correctly.
    uint8 Bom[] = {0xef, 0xbb, 0xbf};
    File->Serialize(Bom, sizeof(Bom));
    const FString Header =
        TEXT("schema_version,ue_frame,sim_time_s,host_elapsed_s,eye_device_timestamp_ms,input_mode,wheel_connected,")
        TEXT("autopilot,map,vehicle_id,road_id,lane_id,road_s_m,junction_id,in_junction,motion,yaw_rate_deg_s,")
        TEXT("red_light_stop_candidate,traffic_light_id,traffic_light_state,traffic_light_distance_m,")
        TEXT("vehicle_x_m,vehicle_y_m,vehicle_z_m,vehicle_yaw_deg,speed_kmh,steer_input,throttle_input,brake_input,")
        TEXT("previous_applied_steer,previous_applied_throttle,previous_applied_brake,previous_reverse,")
        TEXT("eye_source,gaze_valid,gaze_origin_x_m,gaze_origin_y_m,gaze_origin_z_m,gaze_dir_x,gaze_dir_y,gaze_dir_z,")
        TEXT("gaze_local_x,gaze_local_y,gaze_local_z,focus_hit,focus_actor_id,focus_actor_name,focus_kind,")
        TEXT("focus_x_m,focus_y_m,focus_z_m,focus_distance_m,focus_traffic_light_state,looking_at_affecting_light,")
        // --- schema v2: new columns appended below ---
        TEXT("head_x_m,head_y_m,head_z_m,head_pitch_deg,head_yaw_deg,head_roll_deg,")
        TEXT("gaze_h_deg,gaze_v_deg,")
        TEXT("focus_actor_yaw_deg,focus_actor_pitch_deg,focus_actor_roll_deg,")
        TEXT("gaze_velocity_deg_s,saccade_state,saccade_onset_s,saccade_amplitude_deg,saccade_peak_velocity_deg_s,")
        TEXT("sample_dt_ms\r\n");
    CsvFieldCount = CountFields(Header);
    Write(Header);

    // Gaze runs on the tracker's own grid, far faster than the driving rows, so it gets
    // its own file. Both carry device / host / sim timestamps for post-hoc alignment.
    GazePath = GetSessionBasePath() + TEXT("_gaze.csv");
    GazeFile.Reset(IFileManager::Get().CreateFileWriter(*GazePath, FILEWRITE_NoReplaceExisting));
    if (GazeFile)
    {
        GazeFile->Serialize(Bom, sizeof(Bom));
        const FString GazeHeader = TEXT("device_time_ms,host_recv_s,sim_time_s,ue_frame,")
                                   TEXT("comb_h_deg,comb_v_deg,l_h_deg,l_v_deg,r_h_deg,r_v_deg,")
                                   TEXT("vergence_deg,valid,")
                                   // gaze_vel_deg_s and saccade_state carry a value on every sample,
                                   // the four saccade_* fields only inside an event. Event columns hold
                                   // the CLOSED event's final onset/amplitude/peak/sample count, so they
                                   // are identical across all the rows of one event: an open event has no
                                   // verdict yet (39.5% of the runs a live IsSaccade() flag would mark
                                   // are below the detector's own floors). saccade_state is one of
                                   // "fixation" (no event), "saccade" (passed the floors), "rejected"
                                   // (closed below a floor) or "saccade_open" (never closed, or longer
                                   // than the buffer cap: only saccade_samples is filled).
                                   TEXT("gaze_vel_deg_s,saccade_state,saccade_onset_s,saccade_amp_deg,")
                                   TEXT("saccade_peak_deg_s,saccade_samples\r\n");
        GazeCsvFieldCount = CountFields(GazeHeader);
        WriteGaze(GazeHeader);
    }
    else
    {
        LOG_WARN("Driving study: cannot create gaze CSV at %s; gaze stream is OFF", *GazePath);
    }
    // The per-sample detector only earns its thresholds when the eye tracker runs on its own
    // thread: at render rate the intervals are tens of milliseconds wide and the calibrated
    // numbers do not apply. Switch is the same one AEgoSensor uses to start that thread.
    float EyeThreadHz = 0.f;
    GeneralParams.Get("Pimax", "EyeThreadHz", EyeThreadHz);
    float CmdEyeThreadHz = 0.f;
    if (FParse::Value(FCommandLine::Get(), TEXT("DReyeVREyeThreadHz="), CmdEyeThreadHz) && CmdEyeThreadHz > 0.f)
        EyeThreadHz = CmdEyeThreadHz;
    if (GazeFile && EyeThreadHz > 0.f)
    {
        FDReyeVRSaccadeDetector::FConfig DetectorConfig;
        float MinDurationMs = static_cast<float>(DetectorConfig.MinDurationSeconds * 1000.0);
        ReadOverride(TEXT("DReyeVRSaccadeOnsetDegS="), DetectorConfig.OnsetDegS);
        ReadOverride(TEXT("DReyeVRSaccadeOffsetDegS="), DetectorConfig.OffsetDegS);
        ReadOverride(TEXT("DReyeVRSaccadeMinDurationMs="), MinDurationMs);
        ReadOverride(TEXT("DReyeVRSaccadeMinAmplitudeDeg="), DetectorConfig.MinAmplitudeDeg);
        if (MinDurationMs >= 0.f) DetectorConfig.MinDurationSeconds = static_cast<double>(MinDurationMs) * 0.001;
        GazeEvents = MakeUnique<FDReyeVRSaccadeDetector>(DetectorConfig);
        LOG("Driving study: gaze saccade events on (%.0f/%.0f deg/s, min %.0f ms / %.2f deg)",
            DetectorConfig.OnsetDegS, DetectorConfig.OffsetDegS, DetectorConfig.MinDurationSeconds * 1000.0,
            DetectorConfig.MinAmplitudeDeg);
    }
    PendingGazeRows.Reset();
    bEventOverflowed = false;
    StartHost = LastFlush = LastGazeFlush = FPlatformTime::Seconds();
    LOG("Driving study: recording %.1f Hz CSV to %s (gaze stream: %s)", 1.0 / Interval, *Path,
        GazeFile ? *GazePath : TEXT("off"));
}

void FDrivingRecorder::Write(const FString &Text)
{
    if (!File) return;
    FTCHARToUTF8 Bytes(*Text);
    File->Serialize(const_cast<ANSICHAR *>(Bytes.Get()), Bytes.Length());
    if (File->IsError())
    {
        LOG_ERROR("Driving study: write failed (disk full/unavailable); recording stopped: %s", *Path);
        Close();
    }
}

void FDrivingRecorder::WriteGaze(const FString &Text)
{
    if (!GazeFile) return;
    FTCHARToUTF8 Bytes(*Text);
    GazeFile->Serialize(const_cast<ANSICHAR *>(Bytes.Get()), Bytes.Length());
    if (GazeFile->IsError())
    {
        LOG_ERROR("Driving study: gaze write failed; gaze stream stopped: %s", *GazePath);
        GazeFile->Close();
        GazeFile.Reset();
    }
}

void FDrivingRecorder::StampEventRows(TArray<TArray<FString>> &Rows, const FGazeEventStamp &Stamp)
{
    for (TArray<FString> &Row : Rows)
    {
        Row.Add(Stamp.State);
        Row.Add(Stamp.OnsetSimTimeS);
        Row.Add(Stamp.AmplitudeDeg);
        Row.Add(Stamp.PeakDegS);
        Row.Add(Stamp.Samples);
    }
}

int32 FDrivingRecorder::WriteGazeRows(TArray<TArray<FString>> &Rows, const FGazeEventStamp &Stamp)
{
    StampEventRows(Rows, Stamp);
    int32 Written = 0;
    for (TArray<FString> &Row : Rows)
    {
        ensureMsgf(Row.Num() == GazeCsvFieldCount,
                   TEXT("Gaze row has %d fields but the header declares %d"), Row.Num(), GazeCsvFieldCount);
        if (Row.Num() != GazeCsvFieldCount) continue; // a shifted column is worse than a missing row
        WriteGaze(FString::Join(Row, TEXT(",")) + TEXT("\r\n"));
        ++Written;
        ++GazeRows;
    }
    Rows.Reset();
    return Written;
}

int32 FDrivingRecorder::RecordGaze(const TArray<DReyeVR::FEyeSample> &Samples, UWorld &World, uint64 Frame)
{
    if (!GazeFile) return 0;
    const double SimTime = SimTimeOf(&World);
    int32 Written = 0;
    for (const DReyeVR::FEyeSample &S : Samples)
    {
        if (!S.bValid || !S.bSourceTimeKnown) continue;
        if (S.SourceTime <= LastGazeSourceTime) continue; // frames may overlap; never duplicate a row
        LastGazeSourceTime = S.SourceTime;

        // The cyclopean direction is stored as a unit vector: dir = (cosH*cosV, sinH*cosV, sinV).
        const float CombH = FMath::RadiansToDegrees(FMath::Atan2(S.LocalDirection.Y, S.LocalDirection.X));
        const float CombV = FMath::RadiansToDegrees(FMath::Asin(FMath::Clamp(S.LocalDirection.Z, -1.f, 1.f)));
        TArray<FString> Row = {
            LexToString(static_cast<int64>(S.SourceTime * 1000.0)), Number(S.ReceivedTime - StartHost), Number(SimTime),
            LexToString(Frame), Number(CombH), Number(CombV), Number(FMath::RadiansToDegrees(S.Left.HRad)),
            Number(FMath::RadiansToDegrees(S.Left.VRad)), Number(FMath::RadiansToDegrees(S.Right.HRad)),
            Number(FMath::RadiansToDegrees(S.Right.VRad)), Number(FMath::RadiansToDegrees(S.VergenceRad())),
            TEXT("1")};
        if (GazeEvents)
        {
            // Eye-in-head pair in the driving rows' atan2 form; CombV above uses Asin and stays
            // untouched so older gaze files keep their bytes. The PVR source normalizes
            // LocalDirection, and on a unit direction the two forms agree.
            const FVector &Dir = S.LocalDirection;
            const float DetV = static_cast<float>(
                FMath::RadiansToDegrees(FMath::Atan2(Dir.Z, FMath::Sqrt(Dir.X * Dir.X + Dir.Y * Dir.Y))));
            GazeEvents->AddSample(S.SourceTime, SimTime, FMath::DegreesToRadians(CombH), FMath::DegreesToRadians(DetV));
            Row.Add(GazeEvents->HasAngularSpeed() ? Number(GazeEvents->AngularSpeedDegS()) : TEXT(""));
            if (GazeEvents->JustClosedEvent())
            {
                // The sample that ended the event belongs to it, so it joins the buffer ahead of
                // it. The detector's onset is the fixation sample the eye still held, which was
                // already written as fixation: saccade_samples counts that baseline row, the
                // buffered rows are the ones the event itself covers.
                PendingGazeRows.Add(MoveTemp(Row));
                FGazeEventStamp Stamp;
                if (bEventOverflowed)
                {
                    // Once an event overflows MaxBufferedEventRows and flushes a chunk as
                    // saccade_open, its closing tail must also stay saccade_open so a single
                    // event never mixes saccade_open with saccade or rejected.
                    Stamp.State = TEXT("saccade_open");
                    Stamp.Samples = LexToString(PendingGazeRows.Num());
                    bEventOverflowed = false;
                }
                else
                {
                    if (GazeEvents->LastEventAccepted())
                    {
                        Stamp.State = TEXT("saccade");
                        Stamp.OnsetSimTimeS = Number(GazeEvents->OnsetSimTimeS());
                        Stamp.AmplitudeDeg = Number(GazeEvents->AmplitudeDeg());
                        Stamp.PeakDegS = Number(GazeEvents->PeakDegS());
                    }
                    else
                    {
                        // Closed below a floor: the three numeric columns stay empty so a reader can
                        // see what was discarded and tell it apart from a saccade without a join.
                        Stamp.State = TEXT("rejected");
                    }
                    Stamp.Samples = LexToString(GazeEvents->SampleCount());
                }
                Written += WriteGazeRows(PendingGazeRows, Stamp);
            }
            else if (GazeEvents->IsSaccade())
            {
                PendingGazeRows.Add(MoveTemp(Row));
                if (PendingGazeRows.Num() >= MaxBufferedEventRows)
                {
                    LOG_WARN("Driving study: gaze event still open after %d buffered rows; flushing them as "
                             "saccade_open instead of holding them back",
                             static_cast<int32>(MaxBufferedEventRows));
                    bEventOverflowed = true;
                    FGazeEventStamp Stamp;
                    Stamp.State = TEXT("saccade_open");
                    Stamp.Samples = LexToString(PendingGazeRows.Num());
                    Written += WriteGazeRows(PendingGazeRows, Stamp);
                }
            }
            else
            {
                // As in the driving rows: the event columns are only filled inside an event, so
                // one parser can read both files.
                TArray<TArray<FString>> Single;
                Single.Add(MoveTemp(Row));
                Written += WriteGazeRows(Single, FGazeEventStamp());
            }
        }
        else
        {
            // No detector: neither the speed nor any event column can be filled.
            Row.Add(TEXT(""));
            FGazeEventStamp Empty;
            Empty.State = TEXT("");
            TArray<TArray<FString>> Single;
            Single.Add(MoveTemp(Row));
            Written += WriteGazeRows(Single, Empty);
        }
    }
    const double Now = FPlatformTime::Seconds();
    if (Written > 0 && GazeFile && Now - LastGazeFlush >= 1.0)
    {
        GazeFile->Flush();
        LastGazeFlush = Now;
    }
    return Written;
}

void FDrivingRecorder::Close()
{
    if (GazeFile && PendingGazeRows.Num() > 0)
    {
        // The session ended inside an event, so its verdict can never be known; the rows still
        // have to reach the file.
        FGazeEventStamp Stamp;
        Stamp.State = TEXT("saccade_open");
        Stamp.Samples = LexToString(PendingGazeRows.Num());
        WriteGazeRows(PendingGazeRows, Stamp);
        LOG("Driving study: %d gaze rows of an event that never closed written as saccade_open",
            FCString::Atoi(*Stamp.Samples));
        GazeFile->Flush();
    }
    bEventOverflowed = false;
    GazeEvents.Reset(); // the open event belongs to the session that just ended
    if (GazeFile)
    {
        GazeFile->Close();
        GazeFile.Reset();
        LOG("Driving study: gaze CSV closed after %llu rows: %s", GazeRows, *GazePath);
    }
    if (File)
    {
        File->Close();
        File.Reset();
        LOG("Driving study: CSV closed: %s", *Path);
    }
}

FString FDrivingRecorder::GetSessionBasePath() const
{
    return Path.IsEmpty() ? FString() : FPaths::GetBaseFilename(Path, false);
}

void FDrivingRecorder::Record(AEgoVehicle &Vehicle, const DReyeVR::AggregateData &Data,
                              const DReyeVR::FocusInfo &Focus, const FString &EyeSource)
{
    if (!File) return;
    UWorld *World = Vehicle.GetWorld();
    if (!World || World->IsPaused()) return;
    auto *Episode = UCarlaStatics::GetCurrentEpisode(World);
    const double Time = SimTimeOf(World);
    if (LastSample >= 0.0 && Time >= LastSample && Time + 1e-6 < NextSample) return;
    NextSample = LastSample < 0.0 || Time < LastSample ? Time + Interval : NextSample + Interval;
    if (NextSample <= Time) NextSample = Time + Interval; // Never manufacture catch-up samples.
    const double Delta = Time - LastSample;
    const float Yaw = Data.GetVehicleRotation().Yaw;
    const bool HasPrevious = LastSample >= 0.0 && Delta > 0.0;
    const float YawRate = HasPrevious ? FMath::FindDeltaAngleDegrees(LastYaw, Yaw) / Delta : 0.f;
    LastSample = Time;
    LastYaw = Yaw;
    const FVector Position = GlobalLocation(World, Data.GetVehicleLocation());
    const float SpeedKmh = Vehicle.GetVelocity().Size() * 0.036f;
    const auto *Controller = Cast<AWheeledVehicleAIController>(Vehicle.GetController());
    auto *Light = Controller && IsValid(Controller->GetTrafficLight()) ? Controller->GetTrafficLight() : nullptr;
    const FString State = LightState(Light);
    const auto *Pawn = Vehicle.GetDrivingPawn();
    TArray<FString> Row = {TEXT("2"), LexToString(GFrameCounter), Number(Time),
        Number(FPlatformTime::Seconds() - StartHost), LexToString(Data.GetTimestampDevice()), DrivingStudy::InputMode(),
        Pawn && Pawn->GetIsLogiConnected() ? TEXT("1") : TEXT("0"), Vehicle.GetAutopilotStatus() ? TEXT("1") : TEXT("0"),
        DrivingStudy::Csv(Episode ? Episode->GetMapName() : World->GetMapName()), ActorId(Episode, &Vehicle)};

    // Only on-road waypoints: off-road data stays empty instead of attaching it to an unrelated road.
    const auto *Game = UCarlaStatics::GetGameMode(World);
    TArray<FString> Road = {TEXT(""), TEXT(""), TEXT(""), TEXT(""), TEXT("")};
    if (Game && Game->GetMap())
    {
        const auto &Map = *Game->GetMap();
        const auto Waypoint = Map.GetWaypoint(carla::geom::Location(Position));
        if (Waypoint)
        {
            Road = {LexToString(Waypoint->road_id), LexToString(Waypoint->lane_id), Number(Waypoint->s),
                    Map.IsJunction(Waypoint->road_id) ? LexToString(Map.GetJunctionId(Waypoint->road_id)) : TEXT(""),
                    Map.IsJunction(Waypoint->road_id) ? TEXT("1") : TEXT("0")};
        }
    }
    Row.Append(Road);
    Row.Add(HasPrevious ? DrivingStudy::Motion(SpeedKmh, YawRate) : TEXT("unknown"));
    Row.Add(HasPrevious ? Number(YawRate) : TEXT(""));
    Row.Add(State == TEXT("red") && SpeedKmh < 1.f ? TEXT("1") : TEXT("0"));
    Row.Add(ActorId(Episode, Light));
    Row.Add(State);
    Row.Add(Light ? Number(FVector::Distance(Vehicle.GetActorLocation(), Light->GetActorLocation()) * 0.01) : TEXT(""));
    Vector(Row, Position * 0.01f);
    Row.Add(Number(Yaw));
    Row.Add(Number(SpeedKmh));
    const auto &Inputs = Data.GetUserInputs();
    Row.Add(Number(Inputs.Steering)); Row.Add(Number(Inputs.Throttle)); Row.Add(Number(Inputs.Brake));
    // Snapshot is taken BEFORE TickVehicleInputs applies this frame's command. Name these explicitly.
    const auto &Applied = Vehicle.GetVehicleControl();
    Row.Add(Number(Applied.Steer)); Row.Add(Number(Applied.Throttle)); Row.Add(Number(Applied.Brake));
    Row.Add(Applied.bReverse ? TEXT("1") : TEXT("0"));
    const bool Valid = EyeSource != TEXT("dummy") && Data.GetGazeValidity();
    Row.Add(EyeSource); Row.Add(Valid ? TEXT("1") : TEXT("0"));
    const FRotator Rotation = Data.GetCameraRotationAbs();
    Vector(Row, GlobalLocation(World, Data.GetCameraLocationAbs() + Rotation.RotateVector(Data.GetGazeOrigin())) * 0.01f, Valid);
    Vector(Row, Rotation.RotateVector(Data.GetGazeDir()).GetSafeNormal(), Valid);
    Vector(Row, Data.GetGazeDir(), Valid);
    const bool Hit = Valid && Focus.bDidHit && Focus.Actor.IsValid();
    AActor *Target = Hit ? Focus.Actor.Get() : nullptr;
    Row.Add(Hit ? TEXT("1") : TEXT("0")); Row.Add(ActorId(Episode, Target));
    Row.Add(DrivingStudy::Csv(Target ? Target->GetName() : TEXT("")));
    const FString Kind = !Target ? TEXT("none") : Cast<ATrafficLightBase>(Target) ? TEXT("traffic_light") :
        Cast<ACarlaWheeledVehicle>(Target) ? TEXT("vehicle") : Cast<ACharacter>(Target) ? TEXT("pedestrian") : TEXT("other");
    Row.Add(Kind);
    Vector(Row, Hit ? GlobalLocation(World, Focus.HitPoint) * 0.01f : FVector::ZeroVector, Hit);
    Row.Add(Hit ? Number(Focus.Distance * 0.01f) : TEXT(""));
    Row.Add(LightState(Cast<ATrafficLightBase>(Target)));
    Row.Add(Valid ? (Target && Target == Light ? TEXT("1") : TEXT("0")) : TEXT(""));

    // --- schema v2: head pose (head-in-world) ---
    const FVector HeadPos = GlobalLocation(World, Data.GetCameraLocationAbs()) * 0.01f; // cm -> m
    const FRotator HeadRot = Data.GetCameraRotationAbs();
    Row.Add(Number(HeadPos.X)); Row.Add(Number(HeadPos.Y)); Row.Add(Number(HeadPos.Z));
    Row.Add(Number(HeadRot.Pitch)); Row.Add(Number(HeadRot.Yaw)); Row.Add(Number(HeadRot.Roll));

    // --- schema v2: eye-in-head H/V angles (paper 2 input) ---
    const FVector GazeLocal = Data.GetGazeDir(); // local space: +X forward, +Y right, +Z up
    if (Valid && !GazeLocal.IsNearlyZero())
    {
        const float H = FMath::RadiansToDegrees(FMath::Atan2(GazeLocal.Y, GazeLocal.X)); // horizontal: positive = right
        const float V = FMath::RadiansToDegrees(FMath::Atan2(GazeLocal.Z, FMath::Sqrt(GazeLocal.X * GazeLocal.X + GazeLocal.Y * GazeLocal.Y))); // vertical: positive = up
        Row.Add(Number(H)); Row.Add(Number(V));
    }
    else
    {
        Row.Add(TEXT("")); Row.Add(TEXT(""));
    }

    // --- schema v2: focus target actor rotation ---
    if (Target)
    {
        const FRotator TargetRot = Target->GetActorRotation();
        Row.Add(Number(TargetRot.Yaw)); Row.Add(Number(TargetRot.Pitch)); Row.Add(Number(TargetRot.Roll));
    }
    else
    {
        Row.Add(TEXT("")); Row.Add(TEXT("")); Row.Add(TEXT(""));
    }

    // --- schema v2: saccade detection (I-VT) ---
    float GazeVelocity = 0.f;
    bool bGazeVelocityValid = false;
    if (Valid && !GazeLocal.IsNearlyZero() && bHasLastGaze && HasPrevious && Delta > 0.0)
    {
        const FVector CurDir = GazeLocal.GetSafeNormal();
        const FVector PrevDir = LastGazeLocal.GetSafeNormal();
        const float Dot = FMath::Clamp(FVector::DotProduct(CurDir, PrevDir), -1.f, 1.f);
        const float AngleDeg = FMath::RadiansToDegrees(FMath::Acos(Dot));
        GazeVelocity = AngleDeg / static_cast<float>(Delta);
        bGazeVelocityValid = FMath::IsFinite(GazeVelocity);
    }

    if (!Valid || GazeLocal.IsNearlyZero())
    {
        SaccadeState = ESaccadeState::Fixation;
        bHasLastGaze = false;
    }

    if (bGazeVelocityValid)
    {
        // State machine transitions
        if (SaccadeState == ESaccadeState::Fixation && GazeVelocity > SaccadeOnsetThreshold)
        {
            SaccadeState = ESaccadeState::Saccade;
            SaccadeOnsetTime = Time;
            SaccadeStartDir = LastGazeLocal.GetSafeNormal();
            SaccadePeakVelocity = GazeVelocity;
        }
        else if (SaccadeState == ESaccadeState::Saccade && GazeVelocity < SaccadeOffsetThreshold)
        {
            SaccadeState = ESaccadeState::Fixation;
        }
        if (SaccadeState == ESaccadeState::Saccade && GazeVelocity > SaccadePeakVelocity)
        {
            SaccadePeakVelocity = GazeVelocity;
        }
    }

    Row.Add(bGazeVelocityValid ? Number(GazeVelocity) : TEXT(""));
    if (SaccadeState == ESaccadeState::Saccade)
    {
        const FVector CurDir = GazeLocal.GetSafeNormal();
        const float Dot = FMath::Clamp(FVector::DotProduct(CurDir, SaccadeStartDir), -1.f, 1.f);
        const float Amplitude = FMath::RadiansToDegrees(FMath::Acos(Dot));
        Row.Add(TEXT("saccade"));
        Row.Add(Number(SaccadeOnsetTime));
        Row.Add(Number(Amplitude));
        Row.Add(Number(SaccadePeakVelocity));
    }
    else
    {
        Row.Add(TEXT("fixation"));
        Row.Add(TEXT("")); Row.Add(TEXT("")); Row.Add(TEXT(""));
    }

    // Update previous gaze for next frame
    if (Valid && !GazeLocal.IsNearlyZero())
    {
        LastGazeLocal = GazeLocal;
        bHasLastGaze = true;
    }

    // --- schema v2: timing quality ---
    Row.Add(HasPrevious ? Number(Delta * 1000.0) : TEXT(""));

    ensureMsgf(Row.Num() == CsvFieldCount,
               TEXT("Driving row has %d fields but the header declares %d; column order drifted"), Row.Num(),
               CsvFieldCount);
    if (CsvFieldCount > 0 && Row.Num() != CsvFieldCount) return; // never write a silently shifted record
    Write(FString::Join(Row, TEXT(",")) + TEXT("\r\n"));
    if (File && FPlatformTime::Seconds() - LastFlush >= 1.0)
    {
        File->Flush(); // Buffered small text records, at most one flush per second.
        LastFlush = FPlatformTime::Seconds();
        if (File->IsError()) { LOG_ERROR("Driving study: flush failed: %s", *Path); Close(); }
    }
}

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDrivingCsvTest, "HUTB.Pimax.Driving.Csv",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDrivingCsvTest::RunTest(const FString &Parameters)
{
    TestEqual(TEXT("Quote commas and double quotes"), DrivingStudy::Csv(TEXT("road,\"A\"")), FString(TEXT("\"road,\"\"A\"\"\"")));
    TestEqual(TEXT("Empty text"), DrivingStudy::Csv(TEXT("")), FString(TEXT("\"\"")));
    TArray<FString> Row;
    Vector(Row, FVector(1,2,3), false);
    TestEqual(TEXT("Invalid eye values are empty, not fake zero gaze"), FString::Join(Row, TEXT(",")), FString(TEXT(",,")));
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDrivingMotionTest, "HUTB.Pimax.Driving.Motion",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDrivingMotionTest::RunTest(const FString &Parameters)
{
    TestEqual(TEXT("Stationary is not a turn"), DrivingStudy::Motion(0.f, 20.f), FString(TEXT("stopped")));
    TestEqual(TEXT("Left"), DrivingStudy::Motion(20.f, -4.f), FString(TEXT("left")));
    TestEqual(TEXT("Right"), DrivingStudy::Motion(20.f, 4.f), FString(TEXT("right")));
    TestEqual(TEXT("Small yaw change"), DrivingStudy::Motion(20.f, 1.f), FString(TEXT("straight")));
    TestEqual(TEXT("Yaw wrapping"), FMath::FindDeltaAngleDegrees(179.f,-179.f), 2.f);
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDrivingInputTest, "HUTB.Pimax.Driving.InputMode",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDrivingInputTest::RunTest(const FString &Parameters)
{
    TestEqual(TEXT("Existing launch behaviour"), DrivingStudy::InputMode(TEXT("")), FString(TEXT("legacy")));
    TestEqual(TEXT("Exclusive wheel"), DrivingStudy::InputMode(TEXT("-DReyeVRInput=wheel")), FString(TEXT("wheel")));
    TestEqual(TEXT("Controller"), DrivingStudy::InputMode(TEXT("-DReyeVRInput=controller")), FString(TEXT("controller")));
    return true;
}

namespace
{
constexpr double GazeSimDt = 1.0 / 120.0;
constexpr int32 GazeSimCap = 100; // FDrivingRecorder::MaxBufferedEventRows

/// Replay of FDrivingRecorder::RecordGaze's buffering rule (the real one needs an open file
/// and a UWorld) on top of the production detector and the production StampEventRows, so a
/// failing run says something about the shipped code and not about a parallel copy of it.
class FGazeStreamSim
{
  public:
    explicit FGazeStreamSim(const FDReyeVRSaccadeDetector::FConfig &Config) : Detector(Config) {}

    void Submit(int32 Step, float HDeg)
    {
        const double Time = Step * GazeSimDt;
        Detector.AddSample(Time, 100.0 + Time, FMath::DegreesToRadians(HDeg), 0.f);
        TArray<FString> Row;
        for (int32 Field = 0; Field < 13; ++Field) Row.Add(FString::FromInt(Step * 100 + Field));

        if (Detector.JustClosedEvent())
        {
            Buffer.Add(MoveTemp(Row));
            FDrivingRecorder::FGazeEventStamp Stamp;
            if (bEventOverflowed)
            {
                Stamp.State = TEXT("saccade_open");
                Stamp.Samples = LexToString(Buffer.Num());
                bEventOverflowed = false;
            }
            else
            {
                if (Detector.LastEventAccepted())
                {
                    Stamp.State = TEXT("saccade");
                    Stamp.OnsetSimTimeS = Number(Detector.OnsetSimTimeS());
                    Stamp.AmplitudeDeg = Number(Detector.AmplitudeDeg());
                    Stamp.PeakDegS = Number(Detector.PeakDegS());
                }
                else
                {
                    Stamp.State = TEXT("rejected");
                }
                Stamp.Samples = LexToString(Detector.SampleCount());
            }
            Write(Buffer, Stamp);
        }
        else if (Detector.IsSaccade())
        {
            Buffer.Add(MoveTemp(Row));
            if (Buffer.Num() >= GazeSimCap)
            {
                bEventOverflowed = true;
                FDrivingRecorder::FGazeEventStamp Stamp;
                Stamp.State = TEXT("saccade_open");
                Stamp.Samples = LexToString(Buffer.Num());
                Write(Buffer, Stamp);
                ++CapFlushes;
            }
        }
        else
        {
            TArray<TArray<FString>> Single;
            Single.Add(MoveTemp(Row));
            Write(Single, FDrivingRecorder::FGazeEventStamp()); // default stamp: fixation
        }
    }

    /// Mirrors FDrivingRecorder::Close(): an event the session never let go of still reaches
    /// the file, labelled saccade_open.
    void CloseSession()
    {
        if (Buffer.Num() > 0)
        {
            FDrivingRecorder::FGazeEventStamp Stamp;
            Stamp.State = TEXT("saccade_open");
            Stamp.Samples = LexToString(Buffer.Num());
            Write(Buffer, Stamp);
        }
        bEventOverflowed = false;
    }

    /// The rows in the order they reached the file, oldest first.
    const TArray<TArray<FString>> &Stream() const { return Rows; }
    int32 BufferedRows() const { return Buffer.Num(); }
    int32 CapFlushes = 0;

  private:
    void Write(TArray<TArray<FString>> &In, const FDrivingRecorder::FGazeEventStamp &Stamp)
    {
        FDrivingRecorder::StampEventRows(In, Stamp);
        for (TArray<FString> &Row : In) Rows.Add(Row);
        In.Reset();
    }

    FDReyeVRSaccadeDetector Detector;
    TArray<TArray<FString>> Buffer;
    TArray<TArray<FString>> Rows;
    bool bEventOverflowed = false;
};

/// The columns the stamp appended to one row.
FString GazeState(const TArray<FString> &Row) { return Row[13]; }
FString GazeColumn(const TArray<FString> &Row, int32 Index) { return Row[13 + Index]; }
} // namespace

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGazeEventStampingTest, "HUTB.DReyeVR.Gaze.SaccadeStamping",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FGazeEventStampingTest::RunTest(const FString &Parameters)
{
    const FDReyeVRSaccadeDetector::FConfig Calibrated;

    // 1. A 192 deg/s saccade that passes both floors: only the closed event's rows say
    //    saccade, all of them with the same final numbers.
    FGazeStreamSim Accepted(Calibrated);
    for (int32 Step = 0; Step <= 45; ++Step)
    {
        const float HDeg = Step <= 30 ? 0.f : (Step >= 36 ? 8.f : 1.6f * (Step - 30));
        Accepted.Submit(Step, HDeg);
    }
    Accepted.CloseSession();
    TestEqual(TEXT("Nothing buffered after the session closed"), Accepted.BufferedRows(), 0);
    const auto &Clean = Accepted.Stream();
    TestEqual(TEXT("Every sample reached the file exactly once"), Clean.Num(), 46);
    TestEqual(TEXT("Rows are complete: 13 buffered fields plus the 5 stamped ones"), Clean[0].Num(), 18);
    TestEqual(TEXT("A row outside an event is still fixation"), GazeState(Clean[0]), FString(TEXT("fixation")));
    TestEqual(TEXT("Fixation rows keep the four event columns empty"),
              GazeColumn(Clean[0], 1) + GazeColumn(Clean[0], 2) + GazeColumn(Clean[0], 3) + GazeColumn(Clean[0], 4),
              FString(TEXT("")));
    TestEqual(TEXT("The first row of the event is the sample that crossed the onset"),
              GazeState(Clean[31]), FString(TEXT("saccade")));
    TestEqual(TEXT("The sample that stopped moving belongs to the event"), GazeState(Clean[36]),
              FString(TEXT("saccade")));
    TestEqual(TEXT("The onset sample was written before the event opened"), GazeState(Clean[30]),
              FString(TEXT("fixation")));
    TestEqual(TEXT("After the closure comes fixation again"), GazeState(Clean[37]), FString(TEXT("fixation")));
    bool bIdentical = true, bAllSaccade = true;
    for (int32 Row = 32; Row <= 36; ++Row)
    {
        for (int32 Field = 13; Field < 18; ++Field) // only the stamped columns: the base
            bIdentical = bIdentical && Clean[Row][Field] == Clean[31][Field]; // columns differ per sample
        bAllSaccade = bAllSaccade && GazeState(Clean[Row]) == FString(TEXT("saccade"));
    }
    TestTrue(TEXT("Every row of the event carries the same final values"), bIdentical);
    TestTrue(TEXT("Six rows stamped as the saccade"), bAllSaccade);
    TestEqual(TEXT("The onset is the event's final onset"), GazeColumn(Clean[31], 1), Number(100.0 + 30.0 * GazeSimDt));
    TestTrue(TEXT("Amplitude filled"), !GazeColumn(Clean[31], 2).IsEmpty());
    TestTrue(TEXT("Peak filled"), !GazeColumn(Clean[31], 3).IsEmpty());
    TestEqual(TEXT("Sample count is the detector's own"), GazeColumn(Clean[31], 4), FString(TEXT("7")));

    // 2. A 16.7 ms blip: the buffered rows must say "rejected", with only the count filled.
    //    This is the 39.5% of runs an eager IsSaccade() label used to call saccades.
    FGazeStreamSim Blip(Calibrated);
    Blip.Submit(0, 0.f);
    Blip.Submit(1, 2.f);   // 240 deg/s: onset at step 0
    Blip.Submit(2, 2.01f); // 1.2 deg/s: closes 16.7 ms after the onset, below the 20 ms floor
    Blip.Submit(3, 2.01f);
    Blip.CloseSession();
    const auto &Short = Blip.Stream();
    TestEqual(TEXT("The blip closed below the duration floor"), Short[1][13], FString(TEXT("rejected")));
    TestEqual(TEXT("Every blip sample reached the file"), Short.Num(), 4);
    TestEqual(TEXT("A rejected row leaves the onset empty"), GazeColumn(Short[1], 1), FString(TEXT("")));
    TestEqual(TEXT("A rejected row leaves the amplitude empty"), GazeColumn(Short[1], 2), FString(TEXT("")));
    TestEqual(TEXT("A rejected row leaves the peak empty"), GazeColumn(Short[1], 3), FString(TEXT("")));
    TestEqual(TEXT("A rejected row still says how many samples it covered"), GazeColumn(Short[1], 4),
              FString(TEXT("3")));
    TestEqual(TEXT("Rows after a rejected event are fixation"), GazeState(Short[3]), FString(TEXT("fixation")));

    // 3. An event that never closes: the 100-row cap flushes mid-stream, Close() flushes the
    //    remainder, and nothing is ever held back beyond the cap.
    FGazeStreamSim Open(Calibrated);
    Open.Submit(0, 0.f);
    Open.Submit(1, 2.f); // opens the event
    for (int32 Step = 2; Step <= 120; ++Step)
        Open.Submit(Step, 2.f + 0.8333f * (Step - 1)); // 100 deg/s: above offset, so it never closes
    TestEqual(TEXT("A long run flushed once at the cap"), Open.CapFlushes, 1);
    const auto &Long = Open.Stream();
    TestEqual(TEXT("The cap flush held exactly 100 rows"), Long.Num(), 1 + GazeSimCap);
    TestEqual(TEXT("The cap flush is labelled saccade_open"), GazeState(Long[2]), FString(TEXT("saccade_open")));
    TestEqual(TEXT("The cap flush reports its own row count"), GazeColumn(Long[2], 4), FString::FromInt(GazeSimCap));
    TestEqual(TEXT("Nothing beyond the cap was buffered"), Open.BufferedRows(), 20);
    Open.CloseSession();
    TestEqual(TEXT("Close flushed the remainder as saccade_open"), GazeState(Open.Stream().Last()),
              FString(TEXT("saccade_open")));
    TestEqual(TEXT("Nothing buffered once the session is closed"), Open.BufferedRows(), 0);

    // 4. An event that overflows the 100-row cap and later closes: the closing tail must also
    //    stay saccade_open so a single event is never split across mixed states.
    FGazeStreamSim OverflowThenClose(Calibrated);
    OverflowThenClose.Submit(0, 0.f);
    OverflowThenClose.Submit(1, 2.f);
    for (int32 Step = 2; Step <= 105; ++Step)
        OverflowThenClose.Submit(Step, 2.f + 0.8333f * (Step - 1));
    OverflowThenClose.Submit(106, 2.f + 0.8333f * 104); // 0 deg/s: closes the event
    OverflowThenClose.Submit(107, 2.f + 0.8333f * 104); // fixation
    TestEqual(TEXT("OverflowThenClose flushed once at the cap"), OverflowThenClose.CapFlushes, 1);
    TestEqual(TEXT("OverflowThenClose has no buffered rows after closure"), OverflowThenClose.BufferedRows(), 0);
    const auto &OverflowRows = OverflowThenClose.Stream();
    TestEqual(TEXT("OverflowThenClose wrote all 108 rows"), OverflowRows.Num(), 108);
    for (int32 Row = 101; Row <= 106; ++Row)
    {
        TestEqual(TEXT("Closing tail of an overflowed event stays saccade_open"), GazeState(OverflowRows[Row]),
                  FString(TEXT("saccade_open")));
        TestEqual(TEXT("Overflowed closing tail keeps onset/amplitude/peak empty"),
                  GazeColumn(OverflowRows[Row], 1) + GazeColumn(OverflowRows[Row], 2) +
                      GazeColumn(OverflowRows[Row], 3),
                  FString(TEXT("")));
        TestEqual(TEXT("Overflowed closing tail reports its chunk sample count"), GazeColumn(OverflowRows[Row], 4),
                  FString(TEXT("6")));
    }
    TestEqual(TEXT("Sample after overflowed event closes is fixation"), GazeState(OverflowRows[107]),
              FString(TEXT("fixation")));
    return true;
}
#endif
