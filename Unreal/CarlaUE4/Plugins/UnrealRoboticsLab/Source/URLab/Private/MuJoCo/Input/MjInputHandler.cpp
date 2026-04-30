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

#include "MuJoCo/Input/MjInputHandler.h"
#include "MuJoCo/Core/AMjManager.h"
#include "MuJoCo/Core/MjDebugVisualizer.h"
#include "MuJoCo/Core/MjPhysicsEngine.h"
#include "MuJoCo/Input/MjPerturbation.h"
#include "Framework/Application/IInputProcessor.h"
#include "Framework/Application/SlateApplication.h"
#include "InputCoreTypes.h"
#include "Kismet/GameplayStatics.h"

// Slate pre-processor that accumulates raw mouse deltas. Pre-processors run
// ahead of Slate's capture path, so they still see motion while LMB/RMB
// capture has frozen GetMousePosition / GetInputMouseDelta in PIE.
class FMjMouseDeltaProcessor : public IInputProcessor
{
public:
    virtual void Tick(const float /*DeltaTime*/, FSlateApplication& /*SlateApp*/, TSharedRef<ICursor> /*Cursor*/) override {}

    virtual bool HandleMouseMoveEvent(FSlateApplication& /*SlateApp*/, const FPointerEvent& MouseEvent) override
    {
        Accumulated += MouseEvent.GetCursorDelta();
        return false;
    }

    FVector2D Consume()
    {
        const FVector2D D = Accumulated;
        Accumulated = FVector2D::ZeroVector;
        return D;
    }

private:
    FVector2D Accumulated = FVector2D::ZeroVector;
};
#include "Cinematics/MjOrbitCameraActor.h"
#include "Cinematics/MjKeyframeCameraActor.h"
#include "MuJoCo/Components/Forces/MjImpulseLauncher.h"
#include "Utils/URLabLogging.h"

UMjInputHandler::UMjInputHandler()
{
    PrimaryComponentTick.bCanEverTick = true;
}

void UMjInputHandler::BeginPlay()
{
    Super::BeginPlay();

    if (FSlateApplication::IsInitialized())
    {
        MouseDeltaProcessor = MakeShared<FMjMouseDeltaProcessor>();
        FSlateApplication::Get().RegisterInputPreProcessor(MouseDeltaProcessor);
    }
}

void UMjInputHandler::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (MouseDeltaProcessor.IsValid() && FSlateApplication::IsInitialized())
    {
        FSlateApplication::Get().UnregisterInputPreProcessor(MouseDeltaProcessor);
    }
    MouseDeltaProcessor.Reset();

    if (UWorld* World = GetWorld())
    {
        if (APlayerController* PC = World->GetFirstPlayerController())
        {
            UnlockCamera(PC);
        }
    }

    Super::EndPlay(EndPlayReason);
}

void UMjInputHandler::LockCamera(APlayerController* PC)
{
    if (!PC || bPerturbationCameraLocked) return;
    PC->SetIgnoreLookInput(true);
    PC->SetIgnoreMoveInput(true);
    bPerturbationCameraLocked = true;
}

void UMjInputHandler::UnlockCamera(APlayerController* PC)
{
    if (!PC || !bPerturbationCameraLocked) return;
    PC->SetIgnoreLookInput(false);
    PC->SetIgnoreMoveInput(false);
    bPerturbationCameraLocked = false;
}

void UMjInputHandler::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    UWorld* World = GetWorld();
    if (!World) return;

    APlayerController* PC = World->GetFirstPlayerController();
    if (PC)
    {
        ProcessHotkeys(PC);
        ProcessPerturbation(PC, DeltaTime);
    }
}

void UMjInputHandler::ProcessHotkeys(APlayerController* PC)
{
    if (!PC) return;

    AAMjManager* Manager = Cast<AAMjManager>(GetOwner());
    if (!Manager) return;

    // Keys 1-5: Debug toggles (dispatched to DebugVisualizer)
    if (PC->WasInputKeyJustPressed(EKeys::One))
    {
        if (Manager->DebugVisualizer) Manager->DebugVisualizer->ToggleDebugContacts();
    }
    if (PC->WasInputKeyJustPressed(EKeys::Two))
    {
        if (Manager->DebugVisualizer) Manager->DebugVisualizer->ToggleVisuals();
    }
    if (PC->WasInputKeyJustPressed(EKeys::Three))
    {
        if (Manager->DebugVisualizer) Manager->DebugVisualizer->ToggleArticulationCollisions();
    }
    if (PC->WasInputKeyJustPressed(EKeys::Four))
    {
        if (Manager->DebugVisualizer) Manager->DebugVisualizer->ToggleDebugJoints();
    }
    if (PC->WasInputKeyJustPressed(EKeys::Five))
    {
        if (Manager->DebugVisualizer) Manager->DebugVisualizer->ToggleQuickConvertCollisions();
    }
    if (PC->WasInputKeyJustPressed(EKeys::Six))
    {
        if (Manager->DebugVisualizer) Manager->DebugVisualizer->CycleDebugShaderMode();
    }
    if (PC->WasInputKeyJustPressed(EKeys::Seven))
    {
        if (Manager->DebugVisualizer) Manager->DebugVisualizer->ToggleTendons();
    }

    // P: Pause/Resume
    if (PC->WasInputKeyJustPressed(EKeys::P))
    {
        if (Manager->PhysicsEngine)
        {
            Manager->PhysicsEngine->SetPaused(!Manager->PhysicsEngine->bIsPaused);
            UE_LOG(LogURLab, Log, TEXT("Simulation: %s"), Manager->PhysicsEngine->bIsPaused ? TEXT("PAUSED") : TEXT("PLAYING"));
        }
    }

    // R: Reset
    if (PC->WasInputKeyJustPressed(EKeys::R))
    {
        Manager->ResetSimulation();
        UE_LOG(LogURLab, Log, TEXT("Simulation: RESET"));
    }

    // O: Toggle orbit/keyframe cameras
    if (PC->WasInputKeyJustPressed(EKeys::O))
    {
        UWorld* World = GetWorld();

        TArray<AActor*> OrbitCams;
        UGameplayStatics::GetAllActorsOfClass(World, AMjOrbitCameraActor::StaticClass(), OrbitCams);
        for (AActor* A : OrbitCams)
        {
            AMjOrbitCameraActor* Orbit = Cast<AMjOrbitCameraActor>(A);
            if (Orbit)
            {
                Orbit->ToggleOrbit();
                UE_LOG(LogURLab, Log, TEXT("Orbit camera toggled"));
            }
        }

        TArray<AActor*> KfCams;
        UGameplayStatics::GetAllActorsOfClass(World, AMjKeyframeCameraActor::StaticClass(), KfCams);
        for (AActor* A : KfCams)
        {
            AMjKeyframeCameraActor* KfCam = Cast<AMjKeyframeCameraActor>(A);
            if (KfCam)
            {
                KfCam->TogglePlayPause();
                UE_LOG(LogURLab, Log, TEXT("Keyframe camera: %s"), KfCam->IsPlaying() ? TEXT("PLAYING") : TEXT("PAUSED"));
            }
        }
    }

    // F: Fire impulse launchers
    if (PC->WasInputKeyJustPressed(EKeys::F))
    {
        UWorld* World = GetWorld();

        TArray<AActor*> Launchers;
        UGameplayStatics::GetAllActorsOfClass(World, AMjImpulseLauncher::StaticClass(), Launchers);
        for (AActor* A : Launchers)
        {
            AMjImpulseLauncher* Launcher = Cast<AMjImpulseLauncher>(A);
            if (Launcher)
            {
                Launcher->ResetAndFire();
                UE_LOG(LogURLab, Log, TEXT("Fired impulse launcher: %s"), *Launcher->GetName());
            }
        }
    }
}

void UMjInputHandler::ProcessPerturbation(APlayerController* PC, float DeltaTime)
{
    if (!PC) return;

    AAMjManager* Manager = Cast<AAMjManager>(GetOwner());
    if (!Manager || !Manager->Perturbation) return;

    UMjPerturbation* Pert = Manager->Perturbation;

    FVector CursorOrigin, CursorDirection;
    if (!PC->DeprojectMousePositionToWorld(CursorOrigin, CursorDirection))
    {
        return;
    }

    FVector CamLoc;
    FRotator CamRot;
    PC->GetPlayerViewPoint(CamLoc, CamRot);
    const FRotationMatrix CamBasis(CamRot);
    Pert->ClickCamForward = CamBasis.GetScaledAxis(EAxis::X);
    Pert->ClickCamRight   = CamBasis.GetScaledAxis(EAxis::Y);
    Pert->ClickCamUp      = CamBasis.GetScaledAxis(EAxis::Z);

    const bool bCtrl = PC->IsInputKeyDown(EKeys::LeftControl) || PC->IsInputKeyDown(EKeys::RightControl);
    const bool bLmbDown = PC->IsInputKeyDown(EKeys::LeftMouseButton);
    const bool bRmbDown = PC->IsInputKeyDown(EKeys::RightMouseButton);

    // Double-click LMB: select body.
    if (!bCtrl && PC->WasInputKeyJustPressed(EKeys::LeftMouseButton))
    {
        const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
        constexpr float kDoubleClickWindowS = 0.35f;
        if (Now - LastLMBPressTime < kDoubleClickWindowS)
        {
            Pert->HandleSelect(CursorOrigin, CursorDirection);
            LastLMBPressTime = -1.0f;
        }
        else
        {
            LastLMBPressTime = Now;
        }
    }

    // Ctrl+LMB press edge: start rotate.
    const bool bCtrlLmbHeld = bCtrl && bLmbDown;
    if (bCtrlLmbHeld && !bPrevCtrlLmbHeld && Pert->HasSelection())
    {
        Pert->StartRotate();
        LockCamera(PC);
    }

    // Ctrl+RMB press edge: start translate.
    const bool bCtrlRmbHeld = bCtrl && bRmbDown;
    if (bCtrlRmbHeld && !bPrevCtrlRmbHeld && Pert->HasSelection())
    {
        Pert->StartTranslate(CursorOrigin, CursorDirection);
        LockCamera(PC);

        float MX = 0.f, MY = 0.f;
        PC->GetMousePosition(MX, MY);
        TranslateClickScreen = FVector2D(MX, MY);
        TranslateAccumPixels = FVector2D::ZeroVector;
    }

    if (Pert->IsDragging() && (bCtrlLmbHeld || bCtrlRmbHeld))
    {
        FVector2D RawDelta = FVector2D::ZeroVector;
        if (MouseDeltaProcessor.IsValid())
        {
            RawDelta = MouseDeltaProcessor->Consume();
        }

        int32 Vw = 0, Vh = 0;
        PC->GetViewportSize(Vw, Vh);
        const float InvH = Vh > 0 ? 1.0f / static_cast<float>(Vh) : 1.0f;

        // Translate: UE captures the cursor while RMB is held, so rebuild a
        // virtual ray from click screen pos + accumulated pixel deltas.
        // Rotate doesn't need a cursor ray — only the camera basis.
        FVector RayOrigin    = CursorOrigin;
        FVector RayDirection = CursorDirection;
        if (bCtrlRmbHeld)
        {
            TranslateAccumPixels += RawDelta;
            const FVector2D VirtPos = TranslateClickScreen + TranslateAccumPixels;
            FVector VOrigin, VDir;
            if (PC->DeprojectScreenPositionToWorld(VirtPos.X, VirtPos.Y, VOrigin, VDir))
            {
                RayOrigin    = VOrigin;
                RayDirection = VDir;
            }
        }

        // simulate: reldx = dx/h, reldy = -dy/h (window Y-down → up-positive).
        Pert->UpdateDrag(RayOrigin, RayDirection,
                          RawDelta.X * InvH,
                         -RawDelta.Y * InvH);
    }
    else if (MouseDeltaProcessor.IsValid())
    {
        // Drop any accumulated motion so the next drag starts with a zero delta.
        MouseDeltaProcessor->Consume();
    }

    // Release edges: one LockCamera push on press-edge is balanced by exactly
    // one UnlockCamera here (UnlockCamera is a no-op if not locked).
    if (!bCtrlLmbHeld && bPrevCtrlLmbHeld)
    {
        Pert->StopDrag();
        UnlockCamera(PC);
    }
    if (!bCtrlRmbHeld && bPrevCtrlRmbHeld)
    {
        Pert->StopDrag();
        UnlockCamera(PC);
    }

    bPrevCtrlLmbHeld = bCtrlLmbHeld;
    bPrevCtrlRmbHeld = bCtrlRmbHeld;
}
