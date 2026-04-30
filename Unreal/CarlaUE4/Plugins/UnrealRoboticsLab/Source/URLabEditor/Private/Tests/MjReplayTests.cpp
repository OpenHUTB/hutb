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

// ============================================================================
// MjReplayTests.cpp
//
// Tests for the AMjReplayManager plugin class.
//
// Each test spawns an AMjReplayManager in a temporary UE world and exercises
// its public API (StartRecording, StopRecording, ClearRecording,
// SaveRecordingToFile, LoadRecordingFromFile) without relying on BeginPlay or
// a live physics thread. Sessions map is public so tests can populate it
// directly to set up save/load scenarios.
// ============================================================================

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "Engine/World.h"
#include "Replay/MjReplayManager.h"
#include "Replay/MjReplayTypes.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"

// ---------------------------------------------------------------------------
// Helper: create a minimal FMjReplayFrame with a given timestamp and one joint
// ---------------------------------------------------------------------------
static FMjReplayFrame MakeTestFrame(double Timestamp, const FString& JointName, double QPos)
{
    FMjReplayFrame Frame;
    Frame.Timestamp = Timestamp;

    FMjBodyKinematics K;
    K.QPos.Add(QPos);
    K.QVel.Add(0.0);
    Frame.JointStates.Add(JointName, K);
    return Frame;
}

// ---------------------------------------------------------------------------
// Helper: create a temporary world, spawn AMjReplayManager, return both.
// ---------------------------------------------------------------------------
struct FReplayTestSession
{
    UWorld*            World   = nullptr;
    AMjReplayManager*  Replay  = nullptr;

    bool Init()
    {
        World = UWorld::CreateWorld(EWorldType::Game, false);
        if (!World) return false;

        FWorldContext& C = GEngine->CreateNewWorldContext(EWorldType::Game);
        C.SetCurrentWorld(World);

        FActorSpawnParameters P;
        Replay = World->SpawnActor<AMjReplayManager>(P);
        if (Replay)
        {
            // Ensure live session exists (normally done in BeginPlay)
            Replay->Sessions.FindOrAdd(AMjReplayManager::LiveSessionName);
            Replay->ActiveSessionName = AMjReplayManager::LiveSessionName;
        }
        return Replay != nullptr;
    }

    TArray<FMjReplayFrame>& GetLiveFrames()
    {
        return Replay->Sessions.FindOrAdd(AMjReplayManager::LiveSessionName).Frames;
    }

    void Cleanup()
    {
        if (World)
        {
            World->DestroyWorld(false);
            GEngine->DestroyWorldContext(World);
            World  = nullptr;
            Replay = nullptr;
        }
    }

    ~FReplayTestSession() { Cleanup(); }
};

// ============================================================================
// URLab.Replay.InitialState
// ============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMjReplayInitialState,
    "URLab.Replay.InitialState",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMjReplayInitialState::RunTest(const FString& Parameters)
{
    FReplayTestSession S;
    if (!S.Init())
    {
        AddError(TEXT("Failed to spawn AMjReplayManager"));
        return false;
    }

    TestFalse(TEXT("bIsRecording should be false initially"),    S.Replay->bIsRecording);
    TestFalse(TEXT("bIsReplaying should be false initially"),    S.Replay->bIsReplaying);
    TestEqual(TEXT("Live session should be empty initially"), S.GetLiveFrames().Num(), 0);

    S.Cleanup();
    return true;
}

// ============================================================================
// URLab.Replay.StartStopRecording
// ============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMjReplayStartStopRecording,
    "URLab.Replay.StartStopRecording",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMjReplayStartStopRecording::RunTest(const FString& Parameters)
{
    FReplayTestSession S;
    if (!S.Init())
    {
        AddError(TEXT("Failed to spawn AMjReplayManager"));
        return false;
    }

    TestFalse(TEXT("bIsRecording should be false before StartRecording()"), S.Replay->bIsRecording);

    S.Replay->StartRecording();
    TestTrue(TEXT("bIsRecording should be true after StartRecording()"), S.Replay->bIsRecording);

    S.Replay->StopRecording();
    TestFalse(TEXT("bIsRecording should be false after StopRecording()"), S.Replay->bIsRecording);

    S.Cleanup();
    return true;
}

// ============================================================================
// URLab.Replay.ClearRecording
// ============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMjReplayClearRecording,
    "URLab.Replay.ClearRecording",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMjReplayClearRecording::RunTest(const FString& Parameters)
{
    FReplayTestSession S;
    if (!S.Init())
    {
        AddError(TEXT("Failed to spawn AMjReplayManager"));
        return false;
    }

    // Populate live session directly
    TArray<FMjReplayFrame>& LiveFrames = S.GetLiveFrames();
    LiveFrames.Add(MakeTestFrame(0.0,  TEXT("joint0"), 0.0));
    LiveFrames.Add(MakeTestFrame(0.1,  TEXT("joint0"), 0.1));
    LiveFrames.Add(MakeTestFrame(0.2,  TEXT("joint0"), 0.2));
    S.Replay->PlaybackTime = 0.15f;

    TestEqual(TEXT("Buffer should have 3 frames before clear"), LiveFrames.Num(), 3);

    S.Replay->ClearRecording();

    TestEqual(TEXT("Live session should be empty after ClearRecording()"),
        S.GetLiveFrames().Num(), 0);
    TestTrue(TEXT("PlaybackTime should be 0.0f after ClearRecording()"),
        FMath::Abs(S.Replay->PlaybackTime) < 1e-6f);

    S.Cleanup();
    return true;
}

// ============================================================================
// URLab.Replay.SaveAndLoadFile
// ============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMjReplaySaveAndLoadFile,
    "URLab.Replay.SaveAndLoadFile",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMjReplaySaveAndLoadFile::RunTest(const FString& Parameters)
{
    FReplayTestSession S;
    if (!S.Init())
    {
        AddError(TEXT("Failed to spawn AMjReplayManager"));
        return false;
    }

    const int NumFrames = 5;
    const FString TestFileName = TEXT("__test_replay_save_load__.json");

    // Populate live session with known frames
    TArray<FMjReplayFrame>& LiveFrames = S.GetLiveFrames();
    for (int i = 0; i < NumFrames; ++i)
    {
        LiveFrames.Add(
            MakeTestFrame(i * 0.01, TEXT("joint0"), (double)i * 0.1));
    }

    TestEqual(TEXT("Buffer should have NumFrames before save"),
        LiveFrames.Num(), NumFrames);

    // Save (saves the active session, which is Live Recording)
    const bool bSaved = S.Replay->SaveRecordingToFile(TestFileName);
    TestTrue(TEXT("SaveRecordingToFile should return true"), bSaved);

    if (!bSaved)
    {
        AddInfo(TEXT("Skipping load test because save failed"));
        S.Cleanup();
        return false;
    }

    // Clear live session and verify empty
    S.Replay->ClearRecording();
    TestEqual(TEXT("Live session should be empty after ClearRecording()"),
        S.GetLiveFrames().Num(), 0);

    // Load — creates a new session named after the file
    const bool bLoaded = S.Replay->LoadRecordingFromFile(TestFileName);
    TestTrue(TEXT("LoadRecordingFromFile should return true"), bLoaded);

    if (bLoaded)
    {
        // The loaded session is named after the file basename
        FString SessionName = FPaths::GetBaseFilename(TestFileName);
        FReplaySession* LoadedSession = S.Replay->Sessions.Find(SessionName);
        TestTrue(TEXT("Loaded session should exist"), LoadedSession != nullptr);

        if (LoadedSession)
        {
            TestEqual(TEXT("Loaded session frame count should equal NumFrames"),
                LoadedSession->Frames.Num(), NumFrames);

            if (LoadedSession->Frames.Num() == NumFrames)
            {
                TestTrue(TEXT("Frame[0].Timestamp should be ~0.0"),
                    FMath::Abs(LoadedSession->Frames[0].Timestamp) < 1e-9);
                TestTrue(TEXT("Frame[last].Timestamp should be ~(NumFrames-1)*0.01"),
                    FMath::Abs(LoadedSession->Frames.Last().Timestamp - (NumFrames - 1) * 0.01) < 1e-9);
            }
        }
    }

    // Clean up the temp file
    const FString FilePath = FPaths::ProjectSavedDir() / TEXT("Replays") / TestFileName;
    IFileManager::Get().Delete(*FilePath);

    S.Cleanup();
    return true;
}
