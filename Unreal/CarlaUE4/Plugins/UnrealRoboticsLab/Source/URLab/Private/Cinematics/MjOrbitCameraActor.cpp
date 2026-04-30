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
#include "Cinematics/MjOrbitCameraActor.h"
#include "mujoco/mujoco.h"
#include "MuJoCo/Core/MjArticulation.h"
#include "MuJoCo/Components/Bodies/MjBody.h"
#include "Replay/MjReplayManager.h"
#include "CineCameraComponent.h"
#include "Components/BoxComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Utils/URLabLogging.h"

AMjOrbitCameraActor::AMjOrbitCameraActor()
{
    PrimaryActorTick.bCanEverTick = true;

    USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    SetRootComponent(Root);

    DetectionBox = CreateDefaultSubobject<UBoxComponent>(TEXT("DetectionBox"));
    DetectionBox->SetupAttachment(Root);
    DetectionBox->SetBoxExtent(FVector(500.0f, 500.0f, 200.0f));
    DetectionBox->SetCollisionProfileName(TEXT("OverlapAllDynamic"));
    DetectionBox->SetGenerateOverlapEvents(true);
    DetectionBox->SetHiddenInGame(false);
    DetectionBox->ShapeColor = FColor(100, 200, 255, 128);

    CineCamera = CreateDefaultSubobject<UCineCameraComponent>(TEXT("CineCamera"));
    CineCamera->SetupAttachment(Root);
}

void AMjOrbitCameraActor::BeginPlay()
{
    Super::BeginPlay();

    DetectionBox->OnComponentBeginOverlap.AddDynamic(this, &AMjOrbitCameraActor::OnBoxBeginOverlap);

    if (CineCamera)
    {
        CineCamera->CurrentFocalLength = FocalLength;
        CineCamera->CurrentAperture = Aperture;
        CineCamera->FocusSettings.FocusMethod = ECameraFocusMethod::Disable;
        // Prevent aspect ratio letterboxing which offsets debug draws from the rendered image
        CineCamera->SetConstraintAspectRatio(false);
    }

    CachedPC = GetWorld()->GetFirstPlayerController();

    // Find replay manager
    ReplayMgr = Cast<AMjReplayManager>(
        UGameplayStatics::GetActorOfClass(GetWorld(), AMjReplayManager::StaticClass()));

    if (bAutoActivate)
    {
        ActivateCamera();
    }

    // Use manual target if set, otherwise auto-detect from overlap box
    if (ManualTarget)
    {
        SetTarget(ManualTarget);
    }
    else
    {
        TArray<AActor*> OverlappingActors;
        DetectionBox->GetOverlappingActors(OverlappingActors, AMjArticulation::StaticClass());
        for (AActor* Actor : OverlappingActors)
        {
            if (AMjArticulation* Art = Cast<AMjArticulation>(Actor))
            {
                SetTarget(Art);
                break;
            }
        }
    }
}

void AMjOrbitCameraActor::OnBoxBeginOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
    UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
    if (!CurrentTarget)
    {
        if (AMjArticulation* Art = Cast<AMjArticulation>(OtherActor))
        {
            SetTarget(Art);
        }
    }
}

void AMjOrbitCameraActor::SetTarget(AMjArticulation* NewTarget)
{
    TrackedBody = nullptr;
    bIsOrbiting = false;

    if (NewTarget)
    {
        TArray<UMjBody*> Bodies;
        NewTarget->GetComponents<UMjBody>(Bodies);

        // Prefer the root body (attached directly to the articulation root, not to another MjBody)
        UMjBody* FirstNonDefault = nullptr;
        for (UMjBody* B : Bodies)
        {
            if (B->bIsDefault) continue;
            if (!FirstNonDefault) FirstNonDefault = B;

            // Root body = its parent component is NOT an MjBody (it's the scene root or articulation root)
            USceneComponent* Parent = B->GetAttachParent();
            if (Parent && !Cast<UMjBody>(Parent))
            {
                TrackedBody = B;
                UE_LOG(LogURLab, Log, TEXT("MjOrbitCamera: Selected root body '%s'"), *B->GetName());
                break;
            }
        }
        if (!TrackedBody) TrackedBody = FirstNonDefault;

        if (!TrackedBody)
        {
            // Bodies not ready yet — don't commit target so overlap can retry later
            UE_LOG(LogURLab, Log, TEXT("MjOrbitCamera: '%s' has no tracked body yet, will retry"),
                *NewTarget->GetName());
            CurrentTarget = nullptr;
            return;
        }

        CurrentTarget = NewTarget;
        bIsOrbiting = true;
        UE_LOG(LogURLab, Log, TEXT("MjOrbitCamera: Locked onto '%s' (body: '%s')"),
            *CurrentTarget->GetName(),
            *TrackedBody->GetName());
    }
    else
    {
        CurrentTarget = nullptr;
    }
}

void AMjOrbitCameraActor::ActivateCamera()
{
    if (CachedPC && CineCamera)
    {
        CachedPC->SetViewTargetWithBlend(this, 0.5f);
        UE_LOG(LogURLab, Log, TEXT("MjOrbitCamera: Activated"));
    }
}

void AMjOrbitCameraActor::DeactivateCamera()
{
    if (CachedPC)
    {
        if (APawn* Pawn = CachedPC->GetPawn())
        {
            CachedPC->SetViewTargetWithBlend(Pawn, 0.5f);
        }
        UE_LOG(LogURLab, Log, TEXT("MjOrbitCamera: Deactivated"));
    }
}

FVector AMjOrbitCameraActor::GetCurrentCameraPosition() const
{
    return CineCamera ? CineCamera->GetComponentLocation() : GetActorLocation();
}

FRotator AMjOrbitCameraActor::GetCurrentCameraRotation() const
{
    return CineCamera ? CineCamera->GetComponentRotation() : GetActorRotation();
}

float AMjOrbitCameraActor::ComputeAutoFrameRadius() const
{
    if (!CurrentTarget) return OrbitRadius;

    // Get the articulation's bounding box extent
    FVector Origin, Extent;
    CurrentTarget->GetActorBounds(false, Origin, Extent);

    // Use the largest horizontal extent to determine distance
    float RobotSize = FMath::Max(Extent.X, Extent.Y) * 2.0f; // Full width
    float RobotHeight = Extent.Z * 2.0f;
    float MaxExtent = FMath::Max(RobotSize, RobotHeight);

    // Compute distance needed based on FOV
    float HalfFOVRad = FMath::DegreesToRadians(CineCamera ? CineCamera->FieldOfView * 0.5f : 45.0f);
    float RequiredDistance = (MaxExtent * FramingPadding) / FMath::Tan(HalfFOVRad);

    return FMath::Max(RequiredDistance, MinOrbitRadius);
}

void AMjOrbitCameraActor::WriteCameraToReplayFrame()
{
    if (!bRecordCameraPath) return;

    // Lazy lookup — ReplayManager may be auto-created after our BeginPlay
    if (!ReplayMgr)
    {
        ReplayMgr = Cast<AMjReplayManager>(
            UGameplayStatics::GetActorOfClass(GetWorld(), AMjReplayManager::StaticClass()));
    }
    if (!ReplayMgr || !ReplayMgr->bIsRecording) return;

    TArray<FMjReplayFrame>& LiveFrames = ReplayMgr->Sessions.FindOrAdd(AMjReplayManager::LiveSessionName).Frames;
    if (LiveFrames.Num() == 0) { LastCameraWriteFrameIdx = 0; return; }

    // Reset if recording restarted (frame count dropped)
    if (LiveFrames.Num() < LastCameraWriteFrameIdx) LastCameraWriteFrameIdx = 0;

    FVector CamPos = GetCurrentCameraPosition();
    FRotator CamRot = GetCurrentCameraRotation();

    // Fill all frames since the last write (physics runs faster than game tick)
    int32 StartIdx = FMath::Max(LastCameraWriteFrameIdx, 0);
    for (int32 i = StartIdx; i < LiveFrames.Num(); ++i)
    {
        LiveFrames[i].CameraPosition = CamPos;
        LiveFrames[i].CameraRotation = CamRot;
        LiveFrames[i].bHasCameraData = true;
    }
    LastCameraWriteFrameIdx = LiveFrames.Num();
}

void AMjOrbitCameraActor::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    if (!CineCamera) return;

    // --- Playback mode: read camera transforms from replay ---
    if (bPlaybackCameraPath && ReplayMgr && ReplayMgr->bIsReplaying)
    {
        TArray<FMjReplayFrame>& Frames = ReplayMgr->Sessions.FindOrAdd(ReplayMgr->ActiveSessionName).Frames;
        if (Frames.Num() > 0)
        {
            // Find frame at current playback time (same logic as OnReplayStep)
            double StartTime = Frames[0].Timestamp;
            double TargetTime = StartTime + (double)ReplayMgr->PlaybackTime;

            int32 FoundIdx = 0;
            for (int32 i = 1; i < Frames.Num(); ++i)
            {
                if (Frames[i].Timestamp >= TargetTime) break;
                FoundIdx = i;
            }

            const FMjReplayFrame& Frame = Frames[FoundIdx];
            if (Frame.bHasCameraData)
            {
                FVector TargetCamPos = Frame.CameraPosition;

                // Apply RelPos offset if the target articulation has one
                // This shifts the camera the same way the robot position is shifted
                if (CurrentTarget)
                {
                    TArray<FReplayArticulationBinding>& Bindings = ReplayMgr->GetArticulationBindings();
                    for (const FReplayArticulationBinding& B : Bindings)
                    {
                        if (B.Articulation == CurrentTarget && B.bRelativePosition && B.bInitialsCaptured)
                        {
                            FVector Offset(
                                B.InitialMjPosition.X - B.CsvStartPosition.X,
                                B.InitialMjPosition.Y - B.CsvStartPosition.Y,
                                B.InitialMjPosition.Z - B.CsvStartPosition.Z);
                            // Convert MuJoCo meters offset to Unreal cm (x100)
                            TargetCamPos += Offset * 100.0;
                            break;
                        }
                    }
                }

                FVector CurrentPos = CineCamera->GetComponentLocation();
                FRotator CurrentRot = CineCamera->GetComponentRotation();

                FVector NewPos = FMath::VInterpTo(CurrentPos, TargetCamPos, DeltaTime, SmoothingSpeed);
                FRotator NewRot = FMath::RInterpTo(CurrentRot, Frame.CameraRotation, DeltaTime, SmoothingSpeed);

                CineCamera->SetWorldLocationAndRotation(NewPos, NewRot);

                CineCamera->CurrentFocalLength = FocalLength;
                CineCamera->CurrentAperture = Aperture;
                return; // Skip live orbit computation
            }
        }
    }

    // --- Retry target acquisition if we don't have one yet ---
    if (!CurrentTarget)
    {
        TArray<AActor*> OverlappingActors;
        DetectionBox->GetOverlappingActors(OverlappingActors, AMjArticulation::StaticClass());
        for (AActor* Actor : OverlappingActors)
        {
            if (AMjArticulation* Art = Cast<AMjArticulation>(Actor))
            {
                SetTarget(Art);
                if (CurrentTarget) break;
            }
        }
    }

    // --- Live orbit mode ---
    if (!bIsOrbiting || !TrackedBody)
    {
        // Still write camera data even if not orbiting (records whatever view is active)
        WriteCameraToReplayFrame();
        return;
    }

    CurrentAngle += OrbitSpeed * DeltaTime;
    if (CurrentAngle > 360.0f) CurrentAngle -= 360.0f;

    float AngleRad = FMath::DegreesToRadians(CurrentAngle);

    // Compute center from actual MjBody component positions (actor origin doesn't move)
    FVector TargetPos = TrackedBody->GetComponentLocation();
    {
        TArray<UMjBody*> Bodies;
        CurrentTarget->GetComponents<UMjBody>(Bodies);
        if (Bodies.Num() > 1)
        {
            FVector Min(FLT_MAX), Max(-FLT_MAX);
            for (UMjBody* B : Bodies)
            {
                if (!B->bIsDefault)
                {
                    FVector Loc = B->GetComponentLocation();
                    Min = Min.ComponentMin(Loc);
                    Max = Max.ComponentMax(Loc);
                }
            }
            TargetPos = (Min + Max) * 0.5f;
        }
    }

    // Auto-frame: compute radius to keep robot in frame
    float EffectiveRadius = bAutoFrameRobot ? ComputeAutoFrameRadius() : OrbitRadius;
    EffectiveRadius = FMath::FInterpTo(
        CineCamera->GetComponentLocation().Size2D() > 0 ? (CineCamera->GetComponentLocation() - TargetPos).Size2D() : EffectiveRadius,
        EffectiveRadius, DeltaTime, SmoothingSpeed * 0.5f);

    // Compute desired camera position on the orbit circle
    float X = TargetPos.X + EffectiveRadius * FMath::Cos(AngleRad);
    float Y = TargetPos.Y + EffectiveRadius * FMath::Sin(AngleRad);

    // Height with oscillation
    float OscillationPhase = AngleRad * HeightOscillationFrequency;
    float Z = TargetPos.Z + HeightOffset + HeightOscillationAmplitude * FMath::Sin(OscillationPhase);

    FVector DesiredPos(X, Y, Z);

    // Smooth interpolation
    FVector CurrentPos = CineCamera->GetComponentLocation();
    FVector NewPos = FMath::VInterpTo(CurrentPos, DesiredPos, DeltaTime, SmoothingSpeed);

    // Look at target body
    FVector LookAtPoint = TargetPos + FVector(0, 0, LookAtHeightOffset);
    FRotator LookAtRot = (LookAtPoint - NewPos).Rotation();

    FRotator CurrentRot = CineCamera->GetComponentRotation();
    FRotator NewRot = FMath::RInterpTo(CurrentRot, LookAtRot, DeltaTime, SmoothingSpeed);

    CineCamera->SetWorldLocationAndRotation(NewPos, NewRot);

    CineCamera->CurrentFocalLength = FocalLength;
    CineCamera->CurrentAperture = Aperture;

    // Write camera path AFTER position is computed
    WriteCameraToReplayFrame();
}
