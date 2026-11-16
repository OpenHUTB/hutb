// Copyright (c) OpenHUTB. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "UnderwaterPlaneActor.generated.h"

class UStaticMeshComponent;

/**
 * 可配置的平面 Actor —— 用来铺海底和水面。
 *
 * 为什么需要它：
 *   UE4.26 的 Python 在 -run=pythonscript 无头模式下，spawn_actor_from_class 和
 *   spawn_actor_from_object(C++ 类) 都会挂起（等编辑器视口），只有传「蓝图对象」
 *   才能成功放置。而 Landscape / StaticMeshActor 这类 C++ 类没法直接变成蓝图
 *   （Landscape 不是 Blueprintable）。
 *
 *   所以这里提供一个 Blueprintable 的 C++ Actor，在构造函数里建好 StaticMeshComponent，
 *   再由 Python 创建一个蓝图子类（可以改网格/缩放/材质），最后用
 *   spawn_actor_from_object 放置。
 *
 * 用法：
 *   1. Python 里用 BlueprintFactory(parent_class=UnderwaterPlaneActor) 建蓝图
 *   2. 改蓝图的 Mesh / Scale / Material 属性
 *   3. spawn_actor_from_object 放到场景里
 */
UCLASS(Blueprintable)
class UNDERWATERSCENE_API AUnderwaterPlaneActor : public AActor
{
    GENERATED_BODY()

public:
    AUnderwaterPlaneActor();

    /** 平面网格组件。默认用引擎的 1m×1m Plane，靠 Scale 放大到需要的尺寸。 */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Underwater|Plane")
    UStaticMeshComponent* PlaneMesh;

    /**
     * 平面边长（米）。
     * 2000 表示 2000m × 2000m，足以覆盖 Town10HD（约 900m × 1500m）。
     * 在蓝图里改这个值即可，构造函数会把网格缩放算好。
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Underwater|Plane")
    float SizeMeters = 2000.0f;

    /** 在蓝图里改完 SizeMeters 后调用，重新计算网格缩放。 */
    UFUNCTION(BlueprintCallable, CallInEditor, Category = "Underwater|Plane")
    void ApplySize();

protected:
    virtual void OnConstruction(const FTransform& Transform) override;
};
