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

#include "Cinematics/MjKeyframeCameraActor.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/PlayerController.h"
#include "Utils/URLabLogging.h"

#if WITH_EDITOR
#include "Editor.h"
#include "LevelEditorViewport.h"
#endif

AMjKeyframeCameraActor::AMjKeyframeCameraActor()
{
    PrimaryActorTick.bCanEverTick = true;

    // Root
    USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    SetRootComponent(Root);

    // Camera
    CineCamera = CreateDefaultSubobject<UCineCameraComponent>(TEXT("CineCamera"));
    CineCamera->SetupAttachment(Root);
    CineCamera->bConstrainAspectRatio = false;

    // Spline preview
    PathSpline = CreateDefaultSubobject<USplineComponent>(TEXT("PathSpline"));
    PathSpline->SetupAttachment(Root);
    PathSpline->SetDrawDebug(true);
    PathSpline->SetClosedLoop(false);
}

void AMjKeyframeCameraActor::BeginPlay()
{
    Super::BeginPlay();

    // Sort waypoints by time
    Waypoints.Sort([](const FMjCameraWaypoint& A, const FMjCameraWaypoint& B) {
        return A.Time < B.Time;
    });

    if (bAutoActivate)
    {
        APlayerController* PC = GetWorld()->GetFirstPlayerController();
        if (PC)
        {
            PC->SetViewTarget(this);
            UE_LOG(LogURLab, Log, TEXT("MjKeyframeCamera: Activated as view target"));
        }
    }

    if (bAutoPlay)
    {
        if (StartDelay > 0.0f)
        {
            bDelayPending = true;
            DelayTimer = StartDelay;
        }
        else
        {
            Play();
        }
    }

    // Set initial position
    if (Waypoints.Num() > 0)
    {
        CineCamera->SetWorldLocation(Waypoints[0].Position);
        CineCamera->SetWorldRotation(Waypoints[0].Rotation);
    }
}

void AMjKeyframeCameraActor::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    if (bDelayPending)
    {
        DelayTimer -= DeltaTime;
        if (DelayTimer <= 0.0f)
        {
            bDelayPending = false;
            Play();
        }
        return;
    }

    if (!bIsPlaying || Waypoints.Num() < 2) return;

    PlaybackTime += DeltaTime;

    float MaxTime = Waypoints.Last().Time;
    if (PlaybackTime >= MaxTime)
    {
        if (bLoop)
        {
            PlaybackTime = FMath::Fmod(PlaybackTime, MaxTime);
        }
        else
        {
            PlaybackTime = MaxTime;
            bIsPlaying = false;
            UE_LOG(LogURLab, Log, TEXT("MjKeyframeCamera: Playback complete"));
        }
    }

    UpdateCameraTransform();
}

void AMjKeyframeCameraActor::UpdateCameraTransform()
{
    if (Waypoints.Num() < 2) return;

    // Find the two surrounding waypoints
    int32 IdxB = 0;
    for (int32 i = 0; i < Waypoints.Num(); ++i)
    {
        if (Waypoints[i].Time >= PlaybackTime)
        {
            IdxB = i;
            break;
        }
        IdxB = i;
    }
    int32 IdxA = FMath::Max(0, IdxB - 1);

    // Edge cases
    if (IdxA == IdxB)
    {
        CineCamera->SetWorldLocation(Waypoints[IdxA].Position);
        CineCamera->SetWorldRotation(Waypoints[IdxA].Rotation);
        return;
    }

    // Interpolation alpha
    float TimeA = Waypoints[IdxA].Time;
    float TimeB = Waypoints[IdxB].Time;
    float Span = TimeB - TimeA;
    float Alpha = (Span > KINDA_SMALL_NUMBER) ? (PlaybackTime - TimeA) / Span : 0.0f;
    Alpha = FMath::Clamp(Alpha, 0.0f, 1.0f);

    // Smooth step for cubic interpolation
    if (bSmoothInterp)
    {
        Alpha = FMath::SmoothStep(0.0f, 1.0f, Alpha);
    }

    FVector Pos = FMath::Lerp(Waypoints[IdxA].Position, Waypoints[IdxB].Position, Alpha);
    FQuat RotA = Waypoints[IdxA].Rotation.Quaternion();
    FQuat RotB = Waypoints[IdxB].Rotation.Quaternion();
    FQuat Rot = FQuat::Slerp(RotA, RotB, Alpha);

    CineCamera->SetWorldLocation(Pos);
    CineCamera->SetWorldRotation(Rot.Rotator());
}

void AMjKeyframeCameraActor::Play()
{
    if (Waypoints.Num() < 2)
    {
        UE_LOG(LogURLab, Warning, TEXT("MjKeyframeCamera: Need at least 2 waypoints"));
        return;
    }
    bIsPlaying = true;
    UE_LOG(LogURLab, Log, TEXT("MjKeyframeCamera: Playing (%d waypoints, %.1fs)"),
        Waypoints.Num(), Waypoints.Last().Time);
}

void AMjKeyframeCameraActor::Pause()
{
    bIsPlaying = false;
}

void AMjKeyframeCameraActor::TogglePlayPause()
{
    if (bIsPlaying) Pause();
    else Play();
}

void AMjKeyframeCameraActor::Reset()
{
    PlaybackTime = 0.0f;
    bIsPlaying = false;
    if (Waypoints.Num() > 0)
    {
        CineCamera->SetWorldLocation(Waypoints[0].Position);
        CineCamera->SetWorldRotation(Waypoints[0].Rotation);
    }
}

void AMjKeyframeCameraActor::CaptureCurrentView()
{
#if WITH_EDITOR
    if (GEditor)
    {
        // Get the editor viewport camera transform
        FEditorViewportClient* Client = static_cast<FEditorViewportClient*>(GEditor->GetActiveViewport()->GetClient());
        if (Client)
        {
            FMjCameraWaypoint WP;
            WP.Position = Client->GetViewLocation();
            WP.Rotation = Client->GetViewRotation();
            // Auto-assign time: last waypoint time + 3 seconds, or 0 if first
            WP.Time = Waypoints.Num() > 0 ? Waypoints.Last().Time + 3.0f : 0.0f;

            Waypoints.Add(WP);
            RebuildSplinePreview();

            UE_LOG(LogURLab, Log, TEXT("MjKeyframeCamera: Captured waypoint %d at pos=%s, time=%.1fs"),
                Waypoints.Num() - 1, *WP.Position.ToString(), WP.Time);
        }
    }
#endif
}

#if WITH_EDITOR
void AMjKeyframeCameraActor::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);
    RebuildSplinePreview();
}
#endif

void AMjKeyframeCameraActor::RebuildSplinePreview()
{
    if (!PathSpline) return;

    PathSpline->ClearSplinePoints(false);

    for (int32 i = 0; i < Waypoints.Num(); ++i)
    {
        // Convert world position to local space of the actor
        FVector LocalPos = GetActorTransform().InverseTransformPosition(Waypoints[i].Position);
        PathSpline->AddSplinePoint(LocalPos, ESplineCoordinateSpace::Local, false);
    }

    PathSpline->UpdateSpline();
}
