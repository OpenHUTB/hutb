// Copyright (c) OpenHUTB. All rights reserved.

#include "Vehicles/Rov/RovObstacleField.h"

#include "Components/StaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

ARovObstacleField::ARovObstacleField()
{
    PrimaryActorTick.bCanEverTick = false;
    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

void ARovObstacleField::BuildDefaults()
{
    Obstacles.Reset();

    // 位置与 MJCF 里的障碍几何体一一对应（相对 ROV 出生点，单位 cm）。
    // 换算：UE = 100 * (x, -y, z - z_rov)，z_rov 取出生高度 -1.0 m。
    // 圆柱默认轴为 Z：轴向沿 X 用 Pitch=90，沿 Y 用 Pitch=90 + Yaw=90。

    // 1. 海底主干管道与分支管道
    Obstacles.Add({ ERovObstacleType::Pipe, FVector(0.0f, -320.0f, -160.0f), FVector(25.0f, 25.0f, 450.0f), FRotator(90.0f, 0.0f, 0.0f) });
    Obstacles.Add({ ERovObstacleType::Pipe, FVector(300.0f, -100.0f, -160.0f), FVector(20.0f, 20.0f, 300.0f), FRotator(90.0f, 90.0f, 0.0f) });

    // 2. 井口采油树（基座 + 立管）
    Obstacles.Add({ ERovObstacleType::Frame, FVector(180.0f, 50.0f, -200.0f), FVector(60.0f, 60.0f, 50.0f), FRotator::ZeroRotator });
    Obstacles.Add({ ERovObstacleType::Pipe, FVector(180.0f, 50.0f, -90.0f), FVector(22.0f, 22.0f, 60.0f), FRotator::ZeroRotator });

    // 3. 导管架支撑桩（两根立柱 + 横梁）
    Obstacles.Add({ ERovObstacleType::Pipe, FVector(-220.0f, -180.0f, -80.0f), FVector(22.0f, 22.0f, 180.0f), FRotator::ZeroRotator });
    Obstacles.Add({ ERovObstacleType::Pipe, FVector(-220.0f, 180.0f, -80.0f), FVector(22.0f, 22.0f, 180.0f), FRotator::ZeroRotator });
    Obstacles.Add({ ERovObstacleType::Pipe, FVector(-220.0f, 0.0f, -200.0f), FVector(12.0f, 12.0f, 180.0f), FRotator(90.0f, 90.0f, 0.0f) });

    // 4. 检修结构体与礁石
    Obstacles.Add({ ERovObstacleType::Frame, FVector(0.0f, 320.0f, -140.0f), FVector(50.0f, 80.0f, 60.0f), FRotator::ZeroRotator });
    Obstacles.Add({ ERovObstacleType::Reef, FVector(-50.0f, -150.0f, -210.0f), FVector(40.0f, 40.0f, 40.0f), FRotator::ZeroRotator });
}
void ARovObstacleField::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);
    if (Obstacles.Num() == 0 && bSpawnDefaultObstacles) {
        BuildDefaults();
    }
}

void ARovObstacleField::BeginPlay()
{
    Super::BeginPlay();
    if (Obstacles.Num() == 0 && bSpawnDefaultObstacles) {
        BuildDefaults();
    }
    Rebuild();
}

void ARovObstacleField::Rebuild()
{
    for (UStaticMeshComponent* Comp : SpawnedComponents) {
        if (Comp) {
            Comp->DestroyComponent();
        }
    }
    SpawnedComponents.Reset();

    const FVector Base = AnchorActor ? AnchorActor->GetActorLocation() : GetActorLocation();

    UStaticMesh* MeshPipe = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
    UStaticMesh* MeshBox = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
    UStaticMesh* MeshSphere = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere"));

    // 引擎基础网格的原始尺寸：圆柱 r=50 h=100，立方体 100³，球 r=50（单位 cm）
    for (const FRovObstacleDef& Def : Obstacles) {
        UStaticMesh* Mesh = nullptr;
        FVector Scale = FVector::OneVector;
        FLinearColor Color(0.6f, 0.6f, 0.6f);

        switch (Def.Type) {
        case ERovObstacleType::Pipe:
            Mesh = MeshPipe;
            Scale = FVector(Def.Size.X / 50.0f, Def.Size.Y / 50.0f, Def.Size.Z / 50.0f);
            Color = FLinearColor(0.55f, 0.42f, 0.20f);
            break;
        case ERovObstacleType::Frame:
            Mesh = MeshBox;
            Scale = Def.Size / 50.0f;
            Color = FLinearColor(0.35f, 0.42f, 0.50f);
            break;
        case ERovObstacleType::Reef:
            Mesh = MeshSphere;
            Scale = Def.Size / 50.0f;
            Color = FLinearColor(0.45f, 0.38f, 0.30f);
            break;
        default:
            break;
        }

        if (!Mesh) {
            continue;
        }

        UStaticMeshComponent* Comp = NewObject<UStaticMeshComponent>(this);
        Comp->SetStaticMesh(Mesh);
        Comp->SetupAttachment(RootComponent);
        Comp->RegisterComponent();
        Comp->SetWorldLocation(Base + Def.Location);
        Comp->SetWorldRotation(Def.Rotation);
        Comp->SetWorldScale3D(Scale);
        Comp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Comp->SetCastShadow(false);

        if (UMaterialInstanceDynamic* MID = Comp->CreateAndSetMaterialInstanceDynamic(0)) {
            MID->SetVectorParameterValue(TEXT("Color"), Color);
        }
        SpawnedComponents.Add(Comp);
    }

    UE_LOG(LogTemp, Log, TEXT("[RovScene] 障碍物 %d 个（管道/框架/礁石），锚点 %s"), SpawnedComponents.Num(), *Base.ToString());
}
