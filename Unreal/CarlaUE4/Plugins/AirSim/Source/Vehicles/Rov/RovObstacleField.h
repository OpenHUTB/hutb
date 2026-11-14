// Copyright (c) OpenHUTB. All rights reserved.
//
// 障碍物碰撞场景：管道、框架、礁石三类障碍的运行时摆放。

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RovObstacleField.generated.h"

class UStaticMeshComponent;

/** 障碍类型。 */
UENUM(BlueprintType)
enum class ERovObstacleType : uint8
{
    Pipe UMETA(DisplayName = "管道"),
    Frame UMETA(DisplayName = "框架"),
    Reef UMETA(DisplayName = "礁石"),
};

/** 单个障碍定义。位置相对锚点（通常是 ROV 出生点），单位 cm。 */
USTRUCT(BlueprintType)
struct FRovObstacleDef
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ROV|Obstacle")
    ERovObstacleType Type = ERovObstacleType::Pipe;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ROV|Obstacle")
    FVector Location = FVector::ZeroVector;

    /** 尺寸（cm）：圆柱为 (半径, 半径, 半高)，立方体为半尺寸，球为半径。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ROV|Obstacle")
    FVector Size = FVector(50.0f, 50.0f, 200.0f);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ROV|Obstacle")
    FRotator Rotation = FRotator::ZeroRotator;
};

/**
 * 障碍物场。挂在关卡里或由 ROV 仿真模式自动生成。
 *
 * 默认摆放：3 段管道、4 根框架立柱、3 块礁石，围绕锚点分布，
 * 与 MuJoCo 模型里的 pipeline_* / pile_* / reef_* 碰撞体一一对应。
 * 这些障碍直接复用给 4.1.3 / 7.2.x 的接近与碰撞试验。
 */
UCLASS()
class AIRSIM_API ARovObstacleField : public AActor
{
    GENERATED_BODY()

public:
    ARovObstacleField();

    virtual void OnConstruction(const FTransform& Transform) override;
    virtual void BeginPlay() override;

    /** 障碍列表。为空且 bSpawnDefaultObstacles=true 时自动生成默认三类障碍。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ROV|Obstacle")
    TArray<FRovObstacleDef> Obstacles;

    /** 未显式配置时是否生成默认障碍。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ROV|Obstacle")
    bool bSpawnDefaultObstacles = true;

    /** 摆放锚点。为空则以自身位置为锚点。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ROV|Obstacle")
    AActor* AnchorActor = nullptr;

    /** 重建全部障碍（改配置后调用）。 */
    UFUNCTION(BlueprintCallable, Category = "ROV|Obstacle")
    void Rebuild();

private:
    void BuildDefaults();

    UPROPERTY()
    TArray<UStaticMeshComponent*> SpawnedComponents;
};
