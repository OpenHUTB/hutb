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
#include "Replay/MjReplayManager.h"
#include "Utils/URLabLogging.h"
#include "MuJoCo/Core/AMjManager.h"
#include "MuJoCo/Core/MjPhysicsEngine.h"
#include "MuJoCo/Utils/MjUtils.h"
#include "JsonObjectConverter.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Algo/BinarySearch.h"
#include "DesktopPlatformModule.h"
#include "Framework/Application/SlateApplication.h"

const FString AMjReplayManager::LiveSessionName = TEXT("Live Recording");

AMjReplayManager::AMjReplayManager()
{
	PrimaryActorTick.bCanEverTick = true;
}

void AMjReplayManager::BeginPlay()
{
	Super::BeginPlay();

    // Ensure live session exists
    Sessions.FindOrAdd(LiveSessionName);
    ActiveSessionName = LiveSessionName;

    // Auto-connect to manager if possible
    Manager = AAMjManager::GetManager();
    if (Manager)
    {
        // Sets OnPostStep directly rather than RegisterPostStepCallback -- replay
        // needs exclusive control of the callback, not append-to behavior.
        if (Manager->PhysicsEngine)
        {
            Manager->PhysicsEngine->OnPostStep = [this](mjModel* m, mjData* d) {
                this->OnPostStep(m, d);
            };
        }
    }

    if (bAutoRecord)
    {
        StartRecording();
    }

    // Auto-load any CSV/JSON replays from Saved/URLab/Replays/
    {
        FString ReplayDir = FPaths::ProjectSavedDir() / TEXT("URLab") / TEXT("Replays");
        IFileManager::Get().MakeDirectory(*ReplayDir, true);

        TArray<FString> FoundFiles;
        IFileManager::Get().FindFiles(FoundFiles, *(ReplayDir / TEXT("*.csv")), true, false);

        TArray<FString> JsonFiles;
        IFileManager::Get().FindFiles(JsonFiles, *(ReplayDir / TEXT("*.json")), true, false);
        FoundFiles.Append(JsonFiles);

        for (const FString& FileName : FoundFiles)
        {
            FString FullPath = ReplayDir / FileName;
            FString SessionName = FPaths::GetBaseFilename(FileName);

            if (Sessions.Contains(SessionName)) continue;

            if (FileName.EndsWith(TEXT(".csv")))
            {
                LoadFromCSV(FullPath);
            }
            else if (FileName.EndsWith(TEXT(".json")))
            {
                FString JsonString;
                if (FFileHelper::LoadFileToString(JsonString, *FullPath))
                {
                    TSharedPtr<FJsonObject> RootObject;
                    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);
                    if (FJsonSerializer::Deserialize(Reader, RootObject) && RootObject.IsValid())
                    {
                        const TArray<TSharedPtr<FJsonValue>>* JsonFrames;
                        if (RootObject->TryGetArrayField(TEXT("Frames"), JsonFrames))
                        {
                            FReplaySession& Session = Sessions.FindOrAdd(SessionName);
                            Session.SourceFile = FullPath;
                            for (const auto& Val : *JsonFrames)
                            {
                                const TSharedPtr<FJsonObject>* FrameObj;
                                if (Val->TryGetObject(FrameObj))
                                {
                                    FMjReplayFrame Frame;
                                    if (FJsonObjectConverter::JsonObjectToUStruct((*FrameObj).ToSharedRef(), &Frame))
                                    {
                                        Session.Frames.Add(Frame);
                                    }
                                }
                            }
                        }
                    }
                }
            }

            UE_LOG(LogURLabReplay, Log, TEXT("ReplayManager: Auto-loaded replay '%s' (%d frames)"),
                *SessionName, Sessions.FindOrAdd(SessionName).Frames.Num());
        }
    }

    // Keep Live Recording as active
    ActiveSessionName = LiveSessionName;
}

void AMjReplayManager::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    Super::EndPlay(EndPlayReason);

    if (Manager && Manager->PhysicsEngine)
    {
        Manager->PhysicsEngine->OnPostStep = nullptr;
        Manager->PhysicsEngine->ClearCustomStepHandler();
    }
}

void AMjReplayManager::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

    // Read playback time from physics thread (atomic, no race)
    if (bIsReplaying)
    {
        PlaybackTime = (float)PhysicsPlaybackTime.load(std::memory_order_relaxed);
    }
}

// --- Session Accessors ---

TArray<FMjReplayFrame>& AMjReplayManager::GetActiveFrames()
{
    return Sessions.FindOrAdd(ActiveSessionName).Frames;
}

TArray<FMjReplayFrame>& AMjReplayManager::GetLiveFrames()
{
    return Sessions.FindOrAdd(LiveSessionName).Frames;
}

TArray<FString> AMjReplayManager::GetSessionNames() const
{
    TArray<FString> Names;
    Sessions.GetKeys(Names);
    // Ensure Live Recording is always first
    Names.Remove(LiveSessionName);
    Names.Insert(LiveSessionName, 0);
    return Names;
}

void AMjReplayManager::SetActiveSession(const FString& Name)
{
    if (Sessions.Contains(Name))
    {
        ActiveSessionName = Name;
        RebuildArticulationBindings();
        UE_LOG(LogURLabReplay, Log, TEXT("ReplayManager: Active session set to '%s' (%d frames, %d articulation bindings)"),
            *Name, Sessions[Name].Frames.Num(), ArticulationBindings.Num());
    }
    else
    {
        UE_LOG(LogURLabReplay, Warning, TEXT("ReplayManager: Session '%s' not found"), *Name);
    }
}

void AMjReplayManager::RebuildArticulationBindings()
{
    // Preserve existing settings (enabled/relpos) for articulations that still match
    TMap<FString, FReplayArticulationBinding> OldBindings;
    for (const FReplayArticulationBinding& B : ArticulationBindings)
    {
        if (B.Articulation.IsValid())
        {
            OldBindings.Add(B.Articulation->GetName(), B);
        }
    }
    ArticulationBindings.Empty();

    if (!Manager) Manager = AAMjManager::GetManager();
    if (!Manager) return;

    TArray<FMjReplayFrame>& Frames = GetActiveFrames();
    if (Frames.Num() == 0) return;

    const FMjReplayFrame& FirstFrame = Frames[0];
    TArray<AMjArticulation*> Articulations = Manager->GetAllArticulations();
    UE_LOG(LogURLabReplay, Log, TEXT("RebuildArticulationBindings: %d articulations, %d CSV joints in session '%s'"),
        Articulations.Num(), FirstFrame.JointStates.Num(), *ActiveSessionName);

    for (AMjArticulation* Art : Articulations)
    {
        if (!Art) continue;

        FString ActorName = Art->GetName();

        TArray<FString> JointNames = Art->GetJointNames();
        UE_LOG(LogURLabReplay, Log, TEXT("  Checking articulation '%s' — %d joint names"), *ActorName, JointNames.Num());
        bool bHasMatch = false;
        FString FoundPrefix;

        for (const FString& UEJointName : JointNames)
        {
            // Extract the bare joint name by finding the last occurrence of a known separator
            // MuJoCo names are like "g1_29dof_beyondmimic_C_1_left_hip_pitch_joint"
            // We want to compare just "left_hip_pitch_joint" — find it by stripping the actor prefix
            FString BareJointName = UEJointName;
            if (UEJointName.StartsWith(ActorName + TEXT("_")))
            {
                BareJointName = UEJointName.Mid(ActorName.Len() + 1);
            }

            for (auto& Pair : FirstFrame.JointStates)
            {
                // Compare bare names: strip prefix from CSV key too
                if (Pair.Key.EndsWith(BareJointName) || BareJointName.Equals(Pair.Key) || UEJointName.Equals(Pair.Key))
                {
                    bHasMatch = true;
                    break;
                }
            }
            if (bHasMatch) break;
        }

        if (bHasMatch)
        {
            FReplayArticulationBinding Binding;
            Binding.Articulation = Art;
            Binding.InitialPosition = Art->GetActorLocation();
            Binding.bInitialsCaptured = false;

            // Restore previous settings if this articulation was already bound
            if (FReplayArticulationBinding* Old = OldBindings.Find(ActorName))
            {
                Binding.bEnabled = Old->bEnabled;
                Binding.bRelativePosition = Old->bRelativePosition;
            }
            else
            {
                Binding.bEnabled = true;
                // Live recordings already have correct absolute positions — RelPos off by default.
                // External CSVs need RelPos on since their coordinate system differs.
                FReplaySession* Session = Sessions.Find(ActiveSessionName);
                Binding.bRelativePosition = Session && !Session->SourceFile.IsEmpty();
            }

            ArticulationBindings.Add(Binding);
            UE_LOG(LogURLabReplay, Log, TEXT("ReplayManager: Bound articulation '%s'"), *ActorName);
        }
    }
}

// --- Control API ---

void AMjReplayManager::StartRecording()
{
    if (bIsReplaying) StopReplay();

    GetLiveFrames().Empty();
    ActiveSessionName = LiveSessionName;
    bIsRecording = true;
    bCacheValid = false;
    CachedJointNames.Empty();

    // Re-register the PostStep callback (it gets cleared by StopRecording)
    // Sets OnPostStep directly rather than RegisterPostStepCallback -- replay
    // needs exclusive control of the callback, not append-to behavior.
    if (Manager && Manager->PhysicsEngine)
    {
        Manager->PhysicsEngine->OnPostStep = [this](mjModel* m, mjData* d) {
            this->OnPostStep(m, d);
        };
    }

    UE_LOG(LogURLabReplay, Log, TEXT("ReplayManager: Started Recording"));
}

void AMjReplayManager::StopRecording()
{
    bIsRecording = false;
    UE_LOG(LogURLabReplay, Log, TEXT("ReplayManager: Stopped Recording. Captured %d frames."), GetLiveFrames().Num());
}

void AMjReplayManager::StartReplay()
{
    if (bIsRecording) StopRecording();

    TArray<FMjReplayFrame>& Frames = GetActiveFrames();
    if (Frames.Num() == 0)
    {
        UE_LOG(LogURLabReplay, Warning, TEXT("ReplayManager: No frames in session '%s' to replay!"), *ActiveSessionName);
        return;
    }

    if (!Manager) Manager = AAMjManager::GetManager();
    if (!Manager) return;

    bIsReplaying = true;
    PlaybackTime = 0.0f;
    PhysicsPlaybackTime.store(0.0, std::memory_order_relaxed);
    bFirstReplayFrame = true;

    // Rebuild bindings (initial MuJoCo positions will be captured on first replay frame
    // after the reset completes on the physics thread)
    RebuildArticulationBindings();

    // Reset simulation so initial positions are clean (from the model's qpos0)
    Manager->ResetSimulation();

    // Auto-unpause so replay starts immediately
    if (Manager->PhysicsEngine && Manager->PhysicsEngine->bIsPaused)
    {
        Manager->PhysicsEngine->SetPaused(false);
    }

    Manager->PhysicsEngine->SetCustomStepHandler([this](mjModel* m, mjData* d) {
        this->OnReplayStep(m, d);
    });

    UE_LOG(LogURLabReplay, Log, TEXT("ReplayManager: Started Replay of '%s' (%d frames)"),
        *ActiveSessionName, Frames.Num());
}

void AMjReplayManager::StopReplay()
{
    bIsReplaying = false;

    if (Manager && Manager->PhysicsEngine)
    {
        Manager->PhysicsEngine->ClearCustomStepHandler();
    }

    UE_LOG(LogURLabReplay, Log, TEXT("ReplayManager: Stopped Replay"));
}

void AMjReplayManager::ClearRecording()
{
    GetLiveFrames().Empty();
    PlaybackTime = 0.0f;
    PhysicsPlaybackTime.store(0.0, std::memory_order_relaxed);
}

bool AMjReplayManager::SaveRecordingToFile(FString FileName)
{
    TArray<FMjReplayFrame>& Frames = GetActiveFrames();
    if (Frames.Num() == 0) return false;

    TSharedPtr<FJsonObject> RootObject = MakeShareable(new FJsonObject());
    TArray<TSharedPtr<FJsonValue>> JsonFrames;

    for (const FMjReplayFrame& Frame : Frames)
    {
        TSharedPtr<FJsonObject> FrameObj = FJsonObjectConverter::UStructToJsonObject(Frame);
        JsonFrames.Add(MakeShareable(new FJsonValueObject(FrameObj)));
    }

    RootObject->SetArrayField(TEXT("Frames"), JsonFrames);

    FString JsonString;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&JsonString);
    FJsonSerializer::Serialize(RootObject.ToSharedRef(), Writer);

    FString FilePath = FPaths::ProjectSavedDir() / TEXT("URLab") / TEXT("Replays") / FileName;
    return FFileHelper::SaveStringToFile(JsonString, *FilePath);
}

bool AMjReplayManager::LoadRecordingFromFile(FString FileName)
{
    FString FilePath = FPaths::ProjectSavedDir() / TEXT("URLab") / TEXT("Replays") / FileName;
    FString JsonString;
    if (!FFileHelper::LoadFileToString(JsonString, *FilePath)) return false;

    TSharedPtr<FJsonObject> RootObject;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);

    if (FJsonSerializer::Deserialize(Reader, RootObject) && RootObject.IsValid())
    {
        const TArray<TSharedPtr<FJsonValue>>* JsonFrames;
        if (RootObject->TryGetArrayField(TEXT("Frames"), JsonFrames))
        {
            // Create a new named session from the filename
            FString SessionName = FPaths::GetBaseFilename(FileName);
            FReplaySession& Session = Sessions.FindOrAdd(SessionName);
            Session.Frames.Empty();
            Session.SourceFile = FilePath;

            for (const auto& Val : *JsonFrames)
            {
                 const TSharedPtr<FJsonObject>* FrameObj;
                 if (Val->TryGetObject(FrameObj))
                 {
                     FMjReplayFrame Frame;
                     if (FJsonObjectConverter::JsonObjectToUStruct((*FrameObj).ToSharedRef(), &Frame))
                     {
                         Session.Frames.Add(Frame);
                     }
                 }
            }

            ActiveSessionName = SessionName;
            UE_LOG(LogURLabReplay, Log, TEXT("ReplayManager: Loaded %d frames into session '%s'."),
                Session.Frames.Num(), *SessionName);
            return true;
        }
    }
    return false;
}

// --- CSV Import ---

bool AMjReplayManager::LoadFromCSV(FString FilePath, float Timestep)
{
    TArray<FString> Lines;
    if (!FFileHelper::LoadFileToStringArray(Lines, *FilePath))
    {
        UE_LOG(LogURLabReplay, Error, TEXT("ReplayManager: Failed to load CSV file: %s"), *FilePath);
        return false;
    }

    if (Lines.Num() < 2)
    {
        UE_LOG(LogURLabReplay, Error, TEXT("ReplayManager: CSV file has no data rows: %s"), *FilePath);
        return false;
    }

    // Parse header
    TArray<FString> Headers;
    Lines[0].ParseIntoArray(Headers, TEXT(","), false);
    for (FString& H : Headers) H.TrimStartAndEndInline();

    // Find time column
    int32 TimeCol = INDEX_NONE;
    for (int32 i = 0; i < Headers.Num(); ++i)
    {
        if (Headers[i].Equals(TEXT("time"), ESearchCase::IgnoreCase) ||
            Headers[i].Equals(TEXT("timestamp"), ESearchCase::IgnoreCase))
        {
            TimeCol = i;
            break;
        }
    }

    // Group columns by joint name
    // Detect bracket notation: "name[N]" -> group under "name"
    // Everything else is a 1-DOF joint column
    struct FColumnGroup
    {
        FString JointName;
        TArray<int32> ColumnIndices; // Ordered by bracket index or appearance order
    };
    TMap<FString, FColumnGroup> JointColumnMap;

    // Camera column indices
    int32 CamXCol = INDEX_NONE, CamYCol = INDEX_NONE, CamZCol = INDEX_NONE;
    int32 CamPitchCol = INDEX_NONE, CamYawCol = INDEX_NONE, CamRollCol = INDEX_NONE;

    for (int32 i = 0; i < Headers.Num(); ++i)
    {
        if (i == TimeCol) continue;

        FString ColName = Headers[i];

        // Camera columns
        if (ColName.Equals(TEXT("cam_x"), ESearchCase::IgnoreCase)) { CamXCol = i; continue; }
        if (ColName.Equals(TEXT("cam_y"), ESearchCase::IgnoreCase)) { CamYCol = i; continue; }
        if (ColName.Equals(TEXT("cam_z"), ESearchCase::IgnoreCase)) { CamZCol = i; continue; }
        if (ColName.Equals(TEXT("cam_pitch"), ESearchCase::IgnoreCase)) { CamPitchCol = i; continue; }
        if (ColName.Equals(TEXT("cam_yaw"), ESearchCase::IgnoreCase)) { CamYawCol = i; continue; }
        if (ColName.Equals(TEXT("cam_roll"), ESearchCase::IgnoreCase)) { CamRollCol = i; continue; }

        // Check for bracket notation: "name[N]"
        int32 BracketIdx = ColName.Find(TEXT("["));
        if (BracketIdx != INDEX_NONE && ColName.EndsWith(TEXT("]")))
        {
            FString BaseName = ColName.Left(BracketIdx);
            FString IdxStr = ColName.Mid(BracketIdx + 1, ColName.Len() - BracketIdx - 2);
            int32 SubIdx = FCString::Atoi(*IdxStr);

            FColumnGroup& Group = JointColumnMap.FindOrAdd(BaseName);
            Group.JointName = BaseName;
            if (Group.ColumnIndices.Num() <= SubIdx)
            {
                Group.ColumnIndices.SetNum(SubIdx + 1);
            }
            Group.ColumnIndices[SubIdx] = i;
        }
        else
        {
            FColumnGroup& Group = JointColumnMap.FindOrAdd(ColName);
            Group.JointName = ColName;
            Group.ColumnIndices.Add(i);
        }
    }

    bool bHasCameraColumns = (CamXCol != INDEX_NONE && CamYCol != INDEX_NONE && CamZCol != INDEX_NONE);

    // Parse data rows
    FString SessionName = FPaths::GetBaseFilename(FilePath);
    FReplaySession& Session = Sessions.FindOrAdd(SessionName);
    Session.Frames.Empty();
    Session.SourceFile = FilePath;
    Session.Frames.Reserve(Lines.Num() - 1);

    for (int32 LineIdx = 1; LineIdx < Lines.Num(); ++LineIdx)
    {
        FString& Line = Lines[LineIdx];
        if (Line.TrimStartAndEnd().IsEmpty()) continue;

        TArray<FString> Values;
        Line.ParseIntoArray(Values, TEXT(","), false);

        // Get timestamp
        double Time = (TimeCol != INDEX_NONE && TimeCol < Values.Num())
            ? FCString::Atod(*Values[TimeCol])
            : (double)(LineIdx - 1) * (double)Timestep;

        FMjReplayFrame Frame;
        Frame.Timestamp = Time;

        for (auto& Pair : JointColumnMap)
        {
            const FColumnGroup& Group = Pair.Value;
            FMjBodyKinematics Kinematics;
            Kinematics.QPos.SetNum(Group.ColumnIndices.Num());

            bool bValid = true;
            for (int32 k = 0; k < Group.ColumnIndices.Num(); ++k)
            {
                int32 ColIdx = Group.ColumnIndices[k];
                if (ColIdx < Values.Num())
                {
                    Kinematics.QPos[k] = FCString::Atod(*Values[ColIdx]);
                }
                else
                {
                    bValid = false;
                    break;
                }
            }

            if (bValid)
            {
                // QVel left empty — mj_forward doesn't need it for kinematics
                Frame.JointStates.Add(Group.JointName, MoveTemp(Kinematics));
            }
        }

        // Parse camera data if present
        if (bHasCameraColumns)
        {
            Frame.bHasCameraData = true;
            if (CamXCol < Values.Num()) Frame.CameraPosition.X = FCString::Atod(*Values[CamXCol]);
            if (CamYCol < Values.Num()) Frame.CameraPosition.Y = FCString::Atod(*Values[CamYCol]);
            if (CamZCol < Values.Num()) Frame.CameraPosition.Z = FCString::Atod(*Values[CamZCol]);
            if (CamPitchCol < Values.Num()) Frame.CameraRotation.Pitch = FCString::Atod(*Values[CamPitchCol]);
            if (CamYawCol < Values.Num()) Frame.CameraRotation.Yaw = FCString::Atod(*Values[CamYawCol]);
            if (CamRollCol < Values.Num()) Frame.CameraRotation.Roll = FCString::Atod(*Values[CamRollCol]);
        }

        Session.Frames.Add(MoveTemp(Frame));
    }

    ActiveSessionName = SessionName;
    RebuildArticulationBindings();

    UE_LOG(LogURLabReplay, Log, TEXT("ReplayManager: Loaded CSV '%s' -> session '%s': %d frames, %d joints"),
        *FilePath, *SessionName, Session.Frames.Num(), JointColumnMap.Num());

    for (auto& Pair : JointColumnMap)
    {
        UE_LOG(LogURLabReplay, Log, TEXT("  Joint '%s': %d DOF"), *Pair.Key, Pair.Value.ColumnIndices.Num());
    }

    return Session.Frames.Num() > 0;
}

bool AMjReplayManager::BrowseAndLoadCSV(float Timestep)
{
    IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
    if (!DesktopPlatform) return false;

    FString DefaultDir = FPaths::ProjectSavedDir() / TEXT("URLab") / TEXT("Replays");
    IFileManager::Get().MakeDirectory(*DefaultDir, true);

    TArray<FString> OutFiles;
    bool bOpened = DesktopPlatform->OpenFileDialog(
        FSlateApplication::Get().FindBestParentWindowHandleForDialogs(nullptr),
        TEXT("Load Replay"),
        DefaultDir,
        TEXT(""),
        TEXT("CSV Files (*.csv)|*.csv|JSON Files (*.json)|*.json"),
        EFileDialogFlags::None,
        OutFiles);

    if (bOpened && OutFiles.Num() > 0)
    {
        if (OutFiles[0].EndsWith(TEXT(".json")))
        {
            // Load JSON directly from the full path
            FString JsonString;
            if (!FFileHelper::LoadFileToString(JsonString, *OutFiles[0])) return false;

            TSharedPtr<FJsonObject> RootObject;
            TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);
            if (FJsonSerializer::Deserialize(Reader, RootObject) && RootObject.IsValid())
            {
                const TArray<TSharedPtr<FJsonValue>>* JsonFrames;
                if (RootObject->TryGetArrayField(TEXT("Frames"), JsonFrames))
                {
                    FString SessionName = FPaths::GetBaseFilename(OutFiles[0]);
                    FReplaySession& Session = Sessions.FindOrAdd(SessionName);
                    Session.Frames.Empty();
                    Session.SourceFile = OutFiles[0];

                    for (const auto& Val : *JsonFrames)
                    {
                        const TSharedPtr<FJsonObject>* FrameObj;
                        if (Val->TryGetObject(FrameObj))
                        {
                            FMjReplayFrame Frame;
                            if (FJsonObjectConverter::JsonObjectToUStruct((*FrameObj).ToSharedRef(), &Frame))
                            {
                                Session.Frames.Add(Frame);
                            }
                        }
                    }
                    ActiveSessionName = SessionName;
                    RebuildArticulationBindings();
                    UE_LOG(LogURLabReplay, Log, TEXT("ReplayManager: Loaded %d frames from JSON '%s'"),
                        Session.Frames.Num(), *OutFiles[0]);
                    return Session.Frames.Num() > 0;
                }
            }
            return false;
        }
        return LoadFromCSV(OutFiles[0], Timestep);
    }
    return false;
}

bool AMjReplayManager::BrowseAndSaveRecording()
{
    TArray<FMjReplayFrame>& Frames = GetActiveFrames();
    if (Frames.Num() == 0)
    {
        UE_LOG(LogURLabReplay, Warning, TEXT("ReplayManager: No frames in active session '%s' to save. Record some data first or select a session with frames."), *ActiveSessionName);
        return false;
    }

    IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
    if (!DesktopPlatform) return false;

    // Ensure save directory exists
    FString SaveDir = FPaths::ProjectSavedDir() / TEXT("URLab") / TEXT("Replays");
    IFileManager::Get().MakeDirectory(*SaveDir, true);

    FString DefaultName = ActiveSessionName + TEXT(".csv");
    TArray<FString> OutFiles;
    bool bOpened = DesktopPlatform->SaveFileDialog(
        FSlateApplication::Get().FindBestParentWindowHandleForDialogs(nullptr),
        TEXT("Save Replay Recording"),
        SaveDir,
        DefaultName,
        TEXT("CSV Files (*.csv)|*.csv|JSON Files (*.json)|*.json"),
        EFileDialogFlags::None,
        OutFiles);

    if (!bOpened || OutFiles.Num() == 0) return false;

    FString SavePath = OutFiles[0];

    if (SavePath.EndsWith(TEXT(".csv")))
    {
        // Export as CSV with joint-name headers
        if (Frames.Num() == 0) return false;

        // Build headers from first frame's joint names
        TArray<FString> Headers;
        Headers.Add(TEXT("time"));
        TArray<FString> JointOrder;

        for (auto& Pair : Frames[0].JointStates)
        {
            JointOrder.Add(Pair.Key);
            if (Pair.Value.QPos.Num() == 1)
            {
                Headers.Add(Pair.Key);
            }
            else
            {
                for (int32 k = 0; k < Pair.Value.QPos.Num(); ++k)
                {
                    Headers.Add(FString::Printf(TEXT("%s[%d]"), *Pair.Key, k));
                }
            }
        }

        // Add camera columns if any frame has camera data
        bool bHasCamera = false;
        for (const FMjReplayFrame& F : Frames)
        {
            if (F.bHasCameraData) { bHasCamera = true; break; }
        }
        if (bHasCamera)
        {
            Headers.Add(TEXT("cam_x"));
            Headers.Add(TEXT("cam_y"));
            Headers.Add(TEXT("cam_z"));
            Headers.Add(TEXT("cam_pitch"));
            Headers.Add(TEXT("cam_yaw"));
            Headers.Add(TEXT("cam_roll"));
        }

        FString CSV = FString::Join(Headers, TEXT(",")) + TEXT("\n");
        for (const FMjReplayFrame& Frame : Frames)
        {
            CSV += FString::Printf(TEXT("%.6f"), Frame.Timestamp);
            for (const FString& JointName : JointOrder)
            {
                if (const FMjBodyKinematics* K = Frame.JointStates.Find(JointName))
                {
                    for (int32 k = 0; k < K->QPos.Num(); ++k)
                    {
                        CSV += FString::Printf(TEXT(",%.8f"), K->QPos[k]);
                    }
                }
            }
            if (bHasCamera)
            {
                CSV += FString::Printf(TEXT(",%.4f,%.4f,%.4f,%.4f,%.4f,%.4f"),
                    Frame.CameraPosition.X, Frame.CameraPosition.Y, Frame.CameraPosition.Z,
                    Frame.CameraRotation.Pitch, Frame.CameraRotation.Yaw, Frame.CameraRotation.Roll);
            }
            CSV += TEXT("\n");
        }

        bool bSaved = FFileHelper::SaveStringToFile(CSV, *SavePath);
        UE_LOG(LogURLabReplay, Log, TEXT("ReplayManager: Saved %d frames as CSV to '%s'"), Frames.Num(), *SavePath);
        return bSaved;
    }
    else
    {
        // Save as JSON (existing format)
        // Save directly to chosen path as JSON
        TSharedPtr<FJsonObject> RootObject = MakeShareable(new FJsonObject());
        TArray<TSharedPtr<FJsonValue>> JsonFrames;
        for (const FMjReplayFrame& Frame : Frames)
        {
            TSharedPtr<FJsonObject> FrameObj = FJsonObjectConverter::UStructToJsonObject(Frame);
            JsonFrames.Add(MakeShareable(new FJsonValueObject(FrameObj)));
        }
        RootObject->SetArrayField(TEXT("Frames"), JsonFrames);

        FString JsonString;
        TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&JsonString);
        FJsonSerializer::Serialize(RootObject.ToSharedRef(), Writer);

        bool bSaved = FFileHelper::SaveStringToFile(JsonString, *SavePath);
        UE_LOG(LogURLabReplay, Log, TEXT("ReplayManager: Saved %d frames as JSON to '%s'"), Frames.Num(), *SavePath);
        return bSaved;
    }
}

// --- Physics Thread Callbacks ---

void AMjReplayManager::OnPostStep(mjModel* m, mjData* d)
{
    if (!bIsRecording || !m || !d || !d->qpos || !d->qvel) return;

    if (!bCacheValid || CachedJointNames.Num() != m->njnt)
    {
        UpdateCache(m);
    }

    FMjReplayFrame Frame;
    Frame.Timestamp = d->time;

    for (int i = 0; i < m->njnt; ++i)
    {
        FString Name = CachedJointNames[i];
        if (Name.IsEmpty()) continue;

        int qpos_adr = m->jnt_qposadr[i];
        int qvel_adr = m->jnt_dofadr[i];

        int qpos_dim = 0;
        int qvel_dim = 0;

        int jnt_type = m->jnt_type[i];
        if (jnt_type == mjJNT_FREE) { qpos_dim = 7; qvel_dim = 6; }
        else if (jnt_type == mjJNT_BALL) { qpos_dim = 4; qvel_dim = 3; }
        else if (jnt_type == mjJNT_SLIDE) { qpos_dim = 1; qvel_dim = 1; }
        else if (jnt_type == mjJNT_HINGE) { qpos_dim = 1; qvel_dim = 1; }

        if (qpos_dim > 0)
        {
            FMjBodyKinematics Kinematics;
            Kinematics.QPos.SetNum(qpos_dim);
            Kinematics.QVel.SetNum(qvel_dim);

            for (int k=0; k<qpos_dim; ++k) Kinematics.QPos[k] = d->qpos[qpos_adr + k];
            for (int k=0; k<qvel_dim; ++k) Kinematics.QVel[k] = d->qvel[qvel_adr + k];

            Frame.JointStates.Add(Name, Kinematics);
        }
    }

    TArray<FMjReplayFrame>& LiveFrames = GetLiveFrames();
    LiveFrames.Add(Frame);

    if (LiveFrames.Num() > 1 && MaxRecordDuration > 0)
    {
        const double Cutoff = LiveFrames.Last().Timestamp - (double)MaxRecordDuration;
        int32 FirstKeep = Algo::LowerBound(LiveFrames, Cutoff,
            [](const FMjReplayFrame& F, double T) { return F.Timestamp < T; });
        if (FirstKeep > 0)
        {
            LiveFrames.RemoveAt(0, FirstKeep);
        }
    }

    if (LiveFrames.Num() % 60 == 0)
    {
         UE_LOG(LogURLabReplay, Verbose, TEXT("ReplayManager: Captured Frame at %f with %d joints recorded."),
            Frame.Timestamp, Frame.JointStates.Num());
    }
}

void AMjReplayManager::OnReplayStep(mjModel* m, mjData* d)
{
    if (!bIsReplaying || !m || !d) return;

    TArray<FMjReplayFrame>& Frames = GetActiveFrames();
    if (Frames.Num() == 0) return;

    // Advance playback time on the physics thread (no race condition)
    double PhysDt = m->opt.timestep;
    double CurrentPT = PhysicsPlaybackTime.load(std::memory_order_relaxed);
    if (Manager->PhysicsEngine && !Manager->PhysicsEngine->bIsPaused)
    {
        CurrentPT += PhysDt;
        double TotalDuration = Frames.Last().Timestamp - Frames[0].Timestamp;
        if (TotalDuration > 0 && CurrentPT > TotalDuration)
        {
            CurrentPT = 0.0;  // Loop
        }
        PhysicsPlaybackTime.store(CurrentPT, std::memory_order_relaxed);
    }

    double StartTime = Frames[0].Timestamp;
    double TargetTime = StartTime + CurrentPT;

    int32 FoundIdx = Algo::LowerBound(Frames, TargetTime,
        [](const FMjReplayFrame& Frame, double T) { return Frame.Timestamp < T; });
    if (FoundIdx > 0) --FoundIdx;
    FoundIdx = FMath::Clamp(FoundIdx, 0, Frames.Num() - 1);

    // Compute interpolation alpha between FoundIdx and FoundIdx+1
    int32 NextIdx = FMath::Min(FoundIdx + 1, Frames.Num() - 1);
    double Alpha = 0.0;
    if (bInterpolateFrames && NextIdx > FoundIdx)
    {
        double T0 = Frames[FoundIdx].Timestamp;
        double T1 = Frames[NextIdx].Timestamp;
        double Span = T1 - T0;
        if (Span > 1e-9)
        {
            Alpha = FMath::Clamp((TargetTime - T0) / Span, 0.0, 1.0);
        }
    }

    const FMjReplayFrame& Frame = Frames[FoundIdx];

    bool bFirstFrame = bFirstReplayFrame;
    if (d->time - LastReplayLogTime > 1.0)
    {
        UE_LOG(LogURLabReplay, Log, TEXT("ReplayManager: Playing '%s' Frame %f (PlaybackTime: %f). Index %d/%d"),
            *ActiveSessionName, Frame.Timestamp, PlaybackTime, FoundIdx, Frames.Num());
        LastReplayLogTime = d->time;
    }

    int32 MatchedJoints = 0;
    int32 UnmatchedJoints = 0;

    // Pass 1: Suffix-match all joints and collect prefixes of matched articulations
    TSet<FString> MatchedPrefixes;
    TArray<const FMjBodyKinematics*> ResolvedKinematics;
    ResolvedKinematics.SetNum(m->njnt);
    for (int i = 0; i < m->njnt; ++i) ResolvedKinematics[i] = nullptr;

    for (int i = 0; i < m->njnt; ++i)
    {
        const char* name_ptr = mj_id2name(m, mjOBJ_JOINT, i);
        if (!name_ptr) continue;
        FString Name = UTF8_TO_TCHAR(name_ptr);
        if (Name.IsEmpty()) continue;

        // Exact match
        const FMjBodyKinematics* Kinematics = Frame.JointStates.Find(Name);
        if (Kinematics)
        {
            ResolvedKinematics[i] = Kinematics;
            continue;
        }

        // Suffix match (CSV key is suffix of model name, or vice versa)
        for (auto& Pair : Frame.JointStates)
        {
            if (Name.EndsWith(Pair.Key) && (Name.Len() == Pair.Key.Len() || Name[Name.Len() - Pair.Key.Len() - 1] == '_'))
            {
                ResolvedKinematics[i] = &Pair.Value;
                FString Prefix = Name.Left(Name.Len() - Pair.Key.Len());
                MatchedPrefixes.Add(Prefix);
                break;
            }
        }

        // Bare name match: strip instance prefix from both model and CSV names and compare
        // This handles multi-instance replay (e.g. _C_0 matching data recorded from _C_1)
        if (!ResolvedKinematics[i])
        {
            // Find which binding this joint belongs to and extract its bare name
            for (const FReplayArticulationBinding& B : ArticulationBindings)
            {
                if (!B.bEnabled || !B.Articulation.IsValid()) continue;
                FString ArtName = B.Articulation->GetName();
                FString ArtPrefix = ArtName + TEXT("_");

                if (Name.StartsWith(ArtPrefix))
                {
                    FString BareName = Name.Mid(ArtPrefix.Len());
                    // Search CSV keys for one that ends with the same bare name
                    for (auto& Pair : Frame.JointStates)
                    {
                        if (Pair.Key.EndsWith(BareName))
                        {
                            ResolvedKinematics[i] = &Pair.Value;
                            MatchedPrefixes.Add(ArtPrefix);
                            break;
                        }
                    }
                    break;
                }
            }
        }
    }

    // Pass 2: Free joint fallback — only for articulations that had other suffix matches
    for (int i = 0; i < m->njnt; ++i)
    {
        if (ResolvedKinematics[i]) continue;
        if (m->jnt_type[i] != mjJNT_FREE) continue;

        const char* name_ptr = mj_id2name(m, mjOBJ_JOINT, i);
        if (!name_ptr) continue;
        FString Name = UTF8_TO_TCHAR(name_ptr);

        // Check if this free joint's prefix matches a known matched articulation
        for (const FString& Prefix : MatchedPrefixes)
        {
            if (Name.StartsWith(Prefix))
            {
                // Find the 7-DOF CSV entry
                for (auto& Pair : Frame.JointStates)
                {
                    if (Pair.Value.QPos.Num() == 7)
                    {
                        ResolvedKinematics[i] = &Pair.Value;
                        break;
                    }
                }
                break;
            }
        }
    }

    // Pass 3: Apply resolved kinematics with per-articulation filtering
    // Cache CSV start position from first frame for RelPos
    const FMjReplayFrame& FirstFrame = Frames[0];

    for (int i = 0; i < m->njnt; ++i)
    {
        const char* name_ptr = mj_id2name(m, mjOBJ_JOINT, i);
        if (!name_ptr) continue;
        FString Name = UTF8_TO_TCHAR(name_ptr);
        if (Name.IsEmpty()) continue;

        const FMjBodyKinematics* Kinematics = ResolvedKinematics[i];
        if (Kinematics)
        {
            // Find which articulation binding this joint belongs to
            FReplayArticulationBinding* Binding = nullptr;
            for (FReplayArticulationBinding& B : ArticulationBindings)
            {
                if (B.Articulation.IsValid() && Name.Contains(B.Articulation->GetName()))
                {
                    Binding = &B;
                    break;
                }
            }

            // Skip if this articulation is disabled
            if (Binding && !Binding->bEnabled)
            {
                UnmatchedJoints++;
                continue;
            }

            MatchedJoints++;
            int qpos_adr = m->jnt_qposadr[i];
            int type = m->jnt_type[i];
            int expected_dim = (type == mjJNT_FREE) ? 7 : (type == mjJNT_BALL ? 4 : 1);

            if (Kinematics->QPos.Num() == expected_dim)
            {
                // Get next frame kinematics for interpolation (if available)
                const FMjBodyKinematics* NextKinematics = nullptr;
                if (Alpha > 0.0 && NextIdx > FoundIdx)
                {
                    const FMjReplayFrame& NextFrame = Frames[NextIdx];
                    NextKinematics = NextFrame.JointStates.Find(Name);
                    if (NextKinematics && NextKinematics->QPos.Num() != expected_dim)
                        NextKinematics = nullptr;  // Dimension mismatch
                }

                // Helper: lerp a qpos value between frames
                auto LerpQPos = [&](int k) -> double {
                    double v0 = Kinematics->QPos[k];
                    if (NextKinematics && Alpha > 0.0)
                        return v0 + Alpha * (NextKinematics->QPos[k] - v0);
                    return v0;
                };

                // Apply RelPos offset for free joints
                if (type == mjJNT_FREE && Binding && Binding->bRelativePosition && expected_dim == 7)
                {
                    // On first frame, capture both the CSV start position and the current MuJoCo position
                    if (!Binding->bInitialsCaptured)
                    {
                        Binding->InitialMjPosition = FVector(d->qpos[qpos_adr], d->qpos[qpos_adr + 1], d->qpos[qpos_adr + 2]);

                        for (auto& Pair : FirstFrame.JointStates)
                        {
                            if (Pair.Value.QPos.Num() == 7)
                            {
                                Binding->CsvStartPosition = FVector(Pair.Value.QPos[0], Pair.Value.QPos[1], Pair.Value.QPos[2]);
                                break;
                            }
                        }
                        Binding->bInitialsCaptured = true;

                        UE_LOG(LogURLabReplay, Log, TEXT("RelPos captured for '%s': MjPos=(%.4f,%.4f,%.4f) CsvStart=(%.4f,%.4f,%.4f)"),
                            *Binding->Articulation->GetName(),
                            Binding->InitialMjPosition.X, Binding->InitialMjPosition.Y, Binding->InitialMjPosition.Z,
                            Binding->CsvStartPosition.X, Binding->CsvStartPosition.Y, Binding->CsvStartPosition.Z);
                    }

                    double offset_x = Binding->InitialMjPosition.X - Binding->CsvStartPosition.X;
                    double offset_y = Binding->InitialMjPosition.Y - Binding->CsvStartPosition.Y;
                    double offset_z = Binding->InitialMjPosition.Z - Binding->CsvStartPosition.Z;

                    d->qpos[qpos_adr + 0] = LerpQPos(0) + offset_x;
                    d->qpos[qpos_adr + 1] = LerpQPos(1) + offset_y;
                    d->qpos[qpos_adr + 2] = LerpQPos(2) + offset_z;

                    // Quaternion: use slerp for rotation interpolation
                    if (NextKinematics && Alpha > 0.0)
                    {
                        FQuat Q0(Kinematics->QPos[4], Kinematics->QPos[5], Kinematics->QPos[6], Kinematics->QPos[3]);
                        FQuat Q1(NextKinematics->QPos[4], NextKinematics->QPos[5], NextKinematics->QPos[6], NextKinematics->QPos[3]);
                        FQuat QInterp = FQuat::Slerp(Q0, Q1, (float)Alpha);
                        d->qpos[qpos_adr + 3] = QInterp.W;  // MuJoCo quat: w,x,y,z
                        d->qpos[qpos_adr + 4] = QInterp.X;
                        d->qpos[qpos_adr + 5] = QInterp.Y;
                        d->qpos[qpos_adr + 6] = QInterp.Z;
                    }
                    else
                    {
                        for (int k = 3; k < 7; ++k) d->qpos[qpos_adr + k] = Kinematics->QPos[k];
                    }
                }
                else
                {
                    for (int k = 0; k < expected_dim; ++k) d->qpos[qpos_adr + k] = LerpQPos(k);
                }
            }

             int qvel_adr = m->jnt_dofadr[i];
             int expected_vel_dim = (type == mjJNT_FREE) ? 6 : (type == mjJNT_BALL ? 3 : 1);
             if (Kinematics->QVel.Num() == expected_vel_dim)
             {
                 for(int k=0; k<expected_vel_dim; ++k) d->qvel[qvel_adr+k] = Kinematics->QVel[k];
             }
        }
        else
        {
            UnmatchedJoints++;
            if (bFirstFrame)
            {
                UE_LOG(LogURLabReplay, Warning, TEXT("ReplayManager: Model joint '%s' NOT found in CSV data"), *Name);
            }
        }
    }

    if (bFirstFrame)
    {
        // Log CSV keys for comparison
        UE_LOG(LogURLabReplay, Log, TEXT("ReplayManager: Joint match results: %d matched, %d unmatched out of %d model joints"),
            MatchedJoints, UnmatchedJoints, m->njnt);
        UE_LOG(LogURLabReplay, Log, TEXT("ReplayManager: CSV contains %d joint entries"), Frame.JointStates.Num());
        for (auto& Pair : Frame.JointStates)
        {
            UE_LOG(LogURLabReplay, Log, TEXT("  CSV joint: '%s' (%d DOF)"), *Pair.Key, Pair.Value.QPos.Num());
        }
        bFirstReplayFrame = false;
    }

    mj_forward(m, d);
}

void AMjReplayManager::UpdateCache(mjModel* m)
{
    CachedJointNames.Empty();
    CachedJointNames.SetNum(m->njnt);
    bCacheValid = true;

    for (int i = 0; i < m->njnt; ++i)
    {
        const char* name = mj_id2name(m, mjOBJ_JOINT, i);
        if (name)
        {
            CachedJointNames[i] = UTF8_TO_TCHAR(name);
        }
        else
        {
            CachedJointNames[i] = FString::Printf(TEXT("Joint_%d"), i);
        }
        UE_LOG(LogURLabReplay, Log, TEXT("ReplayManager Cache: ID %d -> Name '%s'"), i, *CachedJointNames[i]);
    }
}
