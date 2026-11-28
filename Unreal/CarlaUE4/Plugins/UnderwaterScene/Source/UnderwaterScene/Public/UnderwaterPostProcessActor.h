// Copyright (c) OpenHUTB. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "UnderwaterPostProcessActor.generated.h"

class UPostProcessComponent;

/**
 * 水下后处理 Actor。
 *
 * 为什么不用 APostProcessVolume：
 *   AVolume/ABrush 不是 Blueprintable，Python 的 BlueprintFactory 建不出子类；
 *   而无头模式下又只能放置蓝图对象。所以这里用「普通 Actor + UPostProcessComponent」
 *   自己拼一个等效的、可蓝图化的后处理载体。
 *
 * 行为：
 *   - PostProcessComponent 设为 Unbound（全场景生效）
 *   - 构造时把 OceanPlugin 的 M_Underwater_PP 加进 Blendables（权重 1.0）
 *   - 材质内部按相机深度衰减，所以水面之上看不到效果，入水后才显现
 */
UCLASS(Blueprintable)
class UNDERWATERSCENE_API AUnderwaterPostProcessActor : public AActor
{
    GENERATED_BODY()

public:
    AUnderwaterPostProcessActor();

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Underwater|PostProcess")
    UPostProcessComponent* PostProcess;

    /** 是否在构造时自动挂上 M_Underwater_PP。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Underwater|PostProcess")
    bool bAutoAssignUnderwaterMaterial = true;

protected:
    virtual void OnConstruction(const FTransform& Transform) override;
};
