// Copyright (c) OpenHUTB. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "UnderwaterFogActor.generated.h"

class UExponentialHeightFogComponent;

/**
 * 水下雾。
 *
 * 为什么需要它
 * ------------
 * 之前的水下氛围由 OceanPlugin 的 M_Underwater_PP 提供，但那个材质内部用
 * UnderwaterLightDepth / UnderwaterNoLightDepth 两个硬深度阈值判断「是否在水下」，
 * 相机高度落在阈值附近时判断会在 0/1 之间反复翻转，整屏以约 3.3 秒周期方波闪烁。
 * 参数表里没有可关闭的开关，只能弃用。
 *
 * 本 Actor 用引擎自带的 UExponentialHeightFogComponent 提供**真正的距离雾**：
 *   - 纯静态参数，不随时间变化，不存在闪烁可能
 *   - 按距离指数衰减，远处比近处更「雾」，这正是水下该有的观感
 *   - 不依赖任何第三方材质
 *
 * 为什么不用 AExponentialHeightFog
 * --------------------------------
 * 和 APostProcessVolume 一样，它不是 Blueprintable，无头模式下建不出蓝图子类，
 * 而 spawn_actor_from_class 在无头模式会挂起。所以这里自建一个可蓝图化的载体。
 */
UCLASS(Blueprintable)
class UNDERWATERSCENE_API AUnderwaterFogActor : public AActor
{
    GENERATED_BODY()

public:
    AUnderwaterFogActor();

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Underwater|Fog")
    UExponentialHeightFogComponent* Fog;

    /** 雾密度。越大越浑，0.02 约等于能见度数十米。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Underwater|Fog")
    float FogDensity = 0.02f;

    /** 高度衰减。水下雾基本是均匀的，取小值即可。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Underwater|Fog")
    float FogHeightFalloff = 0.2f;

    /** 雾色（线性空间）。偏青蓝，模拟水体对红光的吸收。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Underwater|Fog")
    FLinearColor FogColor = FLinearColor(0.02f, 0.12f, 0.16f, 1.0f);

    /** 最大不透明度，1.0 表示远处完全被雾吃掉。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Underwater|Fog")
    float FogMaxOpacity = 1.0f;

    /** 起始距离（cm）。留一点余量避免近处糊成一片。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Underwater|Fog")
    float StartDistance = 0.0f;

    /** 截止距离（cm）。0 表示不限制。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Underwater|Fog")
    float FogCutoffDistance = 0.0f;

protected:
    virtual void OnConstruction(const FTransform& Transform) override;
};
