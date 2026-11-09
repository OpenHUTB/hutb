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

    // 管道：三段水平圆柱
    const float PipeZ = -120.0f;
    const FRotator PipeRot(0.0f, 0.0f, 90.0f);
    Obstacles.Add({ ERovObstacleType::Pipe, FVector(400.0f, 0.0f, PipeZ), FVector(30.0f, 30.0f, 450.0f), PipeRot });
    Obstacles.Add({ ERovObstacleType::Pipe, FVector(400.0f, 300.0f, PipeZ), FVector(25.0f, 25.0f, 300.0f), FRotator(0.0f, 45.0f, 90.0f) });
    Obstacles.Add({ ERovObstacleType::Pipe, FVector(400.0f, -300.0f, PipeZ), FVector(25.0f, 25.0f, 300.0f), FRotator(0.0f, -45.0f, 90.0f) });

    // 框架：四根立柱 + 两根横梁
    for (int32 i = 0; i < 4; ++i)
    {
        const float SignX = (i & 1) ? 1.0f : -1.0f;
        const float SignY = (i & 2) ? 1.0f : -1.0f;
        Obstacles.Add({ ERovObstacleType::Frame, FVector(-300.0f + SignX * 120.0f, SignY * 120.0f, -100.0f),
                        FVector(12.0f, 12.0f, 220.0f), FRotator::ZeroRotator });
    }
    Obstacles.Add({ ERovObstacleType::Frame, FVector(-300.0f, 0.0f, 100.0f), FVector(12.0f, 12.0f, 130.0f), FRotator(90.0f, 0.0f, 0.0f) });
    Obstacles.Add({ ERovObstacleType::Frame, FVector(-300.0f, 0.0f, -320.0f), FVector(12.0f, 12.0f, 130.0f), FRotator(90.0f, 0.0f, 0.0f) });

    // 礁石：三块球体
    Obstacles.Add({ ERovObstacleType::Reef, FVector(-500.0f, 500.0f, -260.0f), FVector(90.0f, 90.0f, 90.0f), FRotator::ZeroRotator });
    Obstacles.Add({ ERovObstacleType::Reef, FVector(-650.0f, 380.0f, -300.0f), FVector(60.0f, 60.0f, 60.0f), FRotator::ZeroRotator });
    Obstacles.Add({ ERovObstacleType::Reef, FVector(-420.0f, 620.0f, -310.0f), FVector(45.0f, 45.0f, 45.0f), FRotator::ZeroRotator });
}

void ARovObstacleField::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);
    if (Obstacles.Num() == 0 && bSpawnDefaultObstacles)
    {
        BuildDefaults();
    }
}

void ARovObstacleField::BeginPlay()
{
    Super::BeginPlay();
    if (Obstacles.Num() == 0 && bSpawnDefaultObstacles)
    {
        BuildDefaults();
    }
    Rebuild();
}

void ARovObstacleField::Rebuild()
{
    for (UStaticMeshComponent* Comp : SpawnedComponents)
    {
        if (Comp)
        {
            Comp->DestroyComponent();
        }
    }
    SpawnedComponents.Reset();

    const FVector Base = AnchorActor ? AnchorActor->GetActorLocation() : GetActorLocation();

    UStaticMesh* MeshPipe = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
    UStaticMesh* MeshBox = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
    UStaticMesh* MeshSphere = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere"));

    // 引擎基础网格的原始尺寸：圆柱 r=50 h=100，立方体 100³，球 r=50（单位 cm）
    for (const FRovObstacleDef& Def : Obstacles)    {
        UStaticMesh* Mesh = nullptr;
        FVector Scale = FVector::OneVector;
        FLinearColor Color(0.6f, 0.6f, 0.6f);

        switch (Def.Type)
        {
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

        if (!Mesh)
        {
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

        if (UMaterialInstanceDynamic* MID = Comp->CreateAndSetMaterialInstanceDynamic(0))
        {
            MID->SetVectorParameterValue(TEXT("Color"), Color);
        }
        SpawnedComponents.Add(Comp);
    }

    UE_LOG(LogTemp, Log, TEXT("[RovScene] 障碍物 %d 个（管道/框架/礁石），锚点 %s"),
        SpawnedComponents.Num(), *Base.ToString());
}
