// Copyright (c) OpenHUTB. All rights reserved.
//
// ROV 实时状态面板与力/速度向量可视化。

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "RovStatusHUD.generated.h"

class URovMuJoCoBridge;

/**
 * 屏幕左上角的状态面板 + 世界中的向量箭头。
 *
 * 面板内容：姿态角、深度、推力、接触力、洋流速度。
 * 向量：夹爪接触力箭头、机体速度箭头、洋流速度场。
 * 可用 R 键切换面板、V 键切换向量显示。
 */
UCLASS()
class AIRSIM_API ARovStatusHUD : public AHUD
{
    GENERATED_BODY()

public:
    ARovStatusHUD();

    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;
    virtual void DrawHUD() override;

    /** 是否显示状态面板。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ROV|HUD")
    bool bShowStatusPanel = true;

    /** 是否显示力/速度向量。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ROV|HUD")
    bool bShowVectors = true;

    /** 是否显示洋流速度场（机体周围 3×3 采样箭头）。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ROV|HUD")
    bool bShowFlowField = true;

    /** 面板左上角位置（屏幕像素）。bAnchorRight 为 true 时忽略 X，贴右边缘。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ROV|HUD")
    FVector2D PanelOrigin = FVector2D(24.0f, 24.0f);

    /** 面板贴右上角，避开 AirSim 左上角的消息文字。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ROV|HUD")
    bool bAnchorRight = true;

    /** 力箭头比例（cm / N）。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ROV|HUD")
    float ForceArrowScale = 0.5f;

    /** 速度箭头比例（cm / (m/s)）。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ROV|HUD")
    float VelocityArrowScale = 250.0f;

    /** 箭头最长长度（cm），避免超出视野。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ROV|HUD")
    float MaxArrowLength = 300.0f;

private:
    void DrawStatusPanel();
    void DrawVectors();

    UPROPERTY()
    TWeakObjectPtr<URovMuJoCoBridge> Bridge;

    UPROPERTY()
    TWeakObjectPtr<AActor> RovActor;

    /** 上一帧的机体位置与时间，用于求 UE 空间速度方向。 */
    FVector LastRovLocation = FVector::ZeroVector;
    float LastRovTime = 0.0f;
    bool bHasLastRov = false;

    /** 平滑后的 UE 空间速度（cm/s）。 */
    FVector SmoothedVelocityUE = FVector::ZeroVector;

    void ResolveBridge();
};
