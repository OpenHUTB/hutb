// Copyright (c) OpenHUTB. All rights reserved.

#include "Vehicles/Rov/RovStatusHUD.h"

#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Vehicles/Rov/RovMuJoCoBridge.h"

ARovStatusHUD::ARovStatusHUD()
{
    PrimaryActorTick.bCanEverTick = true;
}

void ARovStatusHUD::BeginPlay()
{
    Super::BeginPlay();
    ResolveBridge();
}

void ARovStatusHUD::ResolveBridge()
{
    if (Bridge.IsValid())
    {
        return;
    }
    UWorld* World = GetWorld();
    if (!World)
    {
        return;
    }
    for (TActorIterator<APawn> It(World); It; ++It)
    {
        APawn* Pawn = *It;
        if (!Pawn)
        {
            continue;
        }
        URovMuJoCoBridge* Found = Pawn->FindComponentByClass<URovMuJoCoBridge>();
        if (Found)
        {
            Bridge = Found;
            RovActor = Pawn;
            return;
        }
    }
}

void ARovStatusHUD::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    ResolveBridge();

    if (APlayerController* PC = GetOwningPlayerController())
    {
        if (PC->WasInputKeyJustPressed(EKeys::R))
        {
            bShowStatusPanel = !bShowStatusPanel;
        }
        if (PC->WasInputKeyJustPressed(EKeys::V))
        {
            bShowVectors = !bShowVectors;
        }
    }
}

void ARovStatusHUD::DrawHUD()
{
    Super::DrawHUD();
    if (!Canvas)
    {
        return;
    }
    if (bShowStatusPanel)
    {
        DrawStatusPanel();
    }
    if (bShowVectors)
    {
        DrawVectors();
    }
}

void ARovStatusHUD::DrawStatusPanel()
{
    FRovStatusSnapshot S;
    if (Bridge.IsValid())
    {
        Bridge->GetStatusSnapshot(S);
    }
    if (!S.bValid)
    {
        DrawText(TEXT("ROV 状态：等待 MuJoCo 桥接…"), FLinearColor(1.0f, 0.8f, 0.2f), PanelOrigin.X, PanelOrigin.Y, nullptr, 1.1f);
        return;
    }

    const float X = bAnchorRight ? (Canvas->SizeX - 316.0f) : PanelOrigin.X;
    float Y = PanelOrigin.Y;
    const float LH = 20.0f;
    const FLinearColor Title(0.4f, 0.9f, 1.0f);
    const FLinearColor Body(0.92f, 0.95f, 1.0f);
    const FLinearColor Warn(1.0f, 0.55f, 0.25f);

    DrawRect(FLinearColor(0.02f, 0.06f, 0.10f, 0.55f), X - 8.0f, Y - 8.0f, 300.0f, 8.0f * LH + 16.0f);

    DrawText(TEXT("ROV 实时状态"), Title, X, Y, nullptr, 1.2f); Y += LH * 1.3f;
    DrawText(FString::Printf(TEXT("姿态  横滚 %+6.1f°  俯仰 %+6.1f°"), S.RollDeg, S.PitchDeg), Body, X, Y, nullptr, 1.0f); Y += LH;
    DrawText(FString::Printf(TEXT("      航向 %+6.1f°"), S.YawDeg), Body, X, Y, nullptr, 1.0f); Y += LH;
    DrawText(FString::Printf(TEXT("深度  %.2f m"), S.DepthM), Body, X, Y, nullptr, 1.0f); Y += LH;
    DrawText(FString::Printf(TEXT("推力  %.1f N"), S.ThrustN), Body, X, Y, nullptr, 1.0f); Y += LH;
    DrawText(FString::Printf(TEXT("接触力 %.1f N"), S.ContactForceN),
             S.ContactForceN > 1.0f ? Warn : Body, X, Y, nullptr, 1.0f); Y += LH;
    DrawText(FString::Printf(TEXT("洋流  %.3f m/s"), S.FlowVelocityMps.Size()), Body, X, Y, nullptr, 1.0f); Y += LH;
    DrawText(FString::Printf(TEXT("速度  %.3f m/s"), S.VelocityMps.Size()), Body, X, Y, nullptr, 1.0f); Y += LH;
    DrawText(TEXT("R 面板   V 向量"), FLinearColor(0.6f, 0.65f, 0.7f), X, Y, nullptr, 0.9f);
}

void ARovStatusHUD::DrawVectors()
{
    if (!RovActor.IsValid() || !Bridge.IsValid() || !Canvas)
    {
        return;
    }
    FRovStatusSnapshot S;
    Bridge->GetStatusSnapshot(S);
    if (!S.bValid)
    {
        return;
    }

    const FVector Origin = RovActor->GetActorLocation();

    // UE 空间速度：用机体位置差分求方向，保证与屏幕上的运动方向一致；
    // 幅值仍取 MuJoCo 的 qvel（更准）。两套坐标系不再混用。
    const float NowTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
    FVector VelUE = FVector::ZeroVector;
    if (bHasLastRov)
    {
        const float Dt = FMath::Max(NowTime - LastRovTime, 1e-3f);
        VelUE = (Origin - LastRovLocation) / Dt;
    }
    LastRovLocation = Origin;
    LastRovTime = NowTime;
    bHasLastRov = true;
    SmoothedVelocityUE = FMath::Lerp(SmoothedVelocityUE, VelUE, 0.35f);

    // 起终点都投影到屏幕，箭头方向与场景中的真实方向一致
    auto DrawWorldArrow = [this](const FVector& Start, const FVector& Dir, float LengthCm,
                                 const FLinearColor& Color, float Thickness) -> bool
    {
        if (Dir.IsNearlyZero() || LengthCm <= 1.0f)
        {
            return false;
        }
        const FVector End = Start + Dir.GetSafeNormal() * LengthCm;
        const FVector P0 = Project(Start);
        const FVector P1 = Project(End);
        if (P0.Z <= 0.0f || P1.Z <= 0.0f)
        {
            return false;
        }
        DrawLine(P0.X, P0.Y, P1.X, P1.Y, Color, Thickness);

        const FVector2D D(P1.X - P0.X, P1.Y - P0.Y);
        const float L = D.Size();
        if (L < 6.0f)
        {
            return true;
        }
        const FVector2D U = D / L;
        const FVector2D N(-U.Y, U.X);
        const float Head = FMath::Min(14.0f, L * 0.3f);
        const FVector2D Base(P1.X - U.X * Head, P1.Y - U.Y * Head);
        DrawLine(P1.X, P1.Y, Base.X + N.X * Head * 0.6f, Base.Y + N.Y * Head * 0.6f, Color, Thickness);
        DrawLine(P1.X, P1.Y, Base.X - N.X * Head * 0.6f, Base.Y - N.Y * Head * 0.6f, Color, Thickness);
        return true;
    };

    // 接触力：从接触点出发，沿实测法向力方向
    if (S.ContactForceN > 0.5f && !S.ContactForceDir.IsNearlyZero())
    {
        const float Len = FMath::Clamp(S.ContactForceN * ForceArrowScale, 30.0f, MaxArrowLength);
        // 接触点存的是「相对机体、UE 轴、厘米」的偏移
        const FVector ContactUE = Origin + S.ContactPoint;
        if (DrawWorldArrow(ContactUE, S.ContactForceDir, Len, FLinearColor(1.0f, 0.55f, 0.1f), 3.0f))
        {
            const FVector Tip = Project(ContactUE + S.ContactForceDir.GetSafeNormal() * Len);
            if (Tip.Z > 0.0f)
            {
                DrawText(FString::Printf(TEXT("F %.0f N"), S.ContactForceN),
                         FLinearColor(1.0f, 0.65f, 0.2f), Tip.X + 8.0f, Tip.Y - 10.0f, nullptr, 1.0f);
            }
        }
    }

    // 机体速度：方向取 UE 位置差分，幅值取 MuJoCo 速度
    const float Speed = static_cast<float>(S.VelocityMps.Size());
    if (Speed > 0.01f)
    {
        const float Len = FMath::Clamp(Speed * VelocityArrowScale, 40.0f, MaxArrowLength);
        FVector VelDir = SmoothedVelocityUE;
        if (VelDir.Size() < 5.0f)
        {
            // 低速时差分噪声大，退回 MuJoCo 速度（Y 镜像到 UE 轴）
            VelDir = FVector(S.VelocityMps.X, -S.VelocityMps.Y, S.VelocityMps.Z);
        }
        if (DrawWorldArrow(Origin, VelDir, Len, FLinearColor(0.3f, 1.0f, 0.45f), 2.5f))
        {
            const FVector Tip = Project(Origin + VelDir.GetSafeNormal() * Len);
            if (Tip.Z > 0.0f)
            {
                DrawText(FString::Printf(TEXT("v %.2f m/s"), Speed),
                         FLinearColor(0.35f, 1.0f, 0.5f), Tip.X + 8.0f, Tip.Y + 2.0f, nullptr, 1.0f);
            }
        }
    }

    // 洋流速度场：机体周围 3×3 采样，方向与幅值均取自实测洋流
    if (bShowFlowField)
    {
        const FVector Flow = S.FlowVelocityMps;
        const float FlowSpeed = static_cast<float>(Flow.Size());
        if (FlowSpeed > 1e-3f)
        {
            const FLinearColor FlowColor(0.35f, 0.7f, 1.0f, 0.85f);
            const float Len = FMath::Clamp(FlowSpeed * 250.0f, 25.0f, 150.0f);
            for (int32 iy = -1; iy <= 1; ++iy)
            {
                for (int32 iz = -1; iz <= 1; ++iz)
                {
                    const FVector Sample = Origin + FVector(0.0f, iy * 150.0f, iz * 100.0f);
                    DrawWorldArrow(Sample, Flow, Len, FlowColor, 1.5f);
                }
            }
        }
    }
}
