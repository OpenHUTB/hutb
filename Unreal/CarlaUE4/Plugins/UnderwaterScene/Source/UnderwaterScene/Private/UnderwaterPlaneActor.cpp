// Copyright (c) OpenHUTB. All rights reserved.

#include "UnderwaterPlaneActor.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "UObject/ConstructorHelpers.h"

AUnderwaterPlaneActor::AUnderwaterPlaneActor()
{
    PrimaryActorTick.bCanEverTick = false;

    PlaneMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PlaneMesh"));
    SetRootComponent(PlaneMesh);

    // 引擎自带的 1m×1m 平面
    static ConstructorHelpers::FObjectFinder<UStaticMesh> PlaneAsset(
        TEXT("/Engine/BasicShapes/Plane.Plane"));
    if (PlaneAsset.Succeeded())
    {
        PlaneMesh->SetStaticMesh(PlaneAsset.Object);
    }

    // 平面本身不需要碰撞：海底/水面都是纯视觉，物理在 MuJoCo 里算。
    // 如果后续需要 ROV 撞海底，改成 QueryOnly 并加碰撞响应。
    PlaneMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    PlaneMesh->SetCastShadow(false);

    ApplySize();
}

void AUnderwaterPlaneActor::ApplySize()
{
    if (!PlaneMesh) return;

    // 引擎 Plane 是 100cm × 100cm，所以缩放系数 = 米数 × 100 / 100 = 米数
    const float ScaleXY = FMath::Max(SizeMeters, 1.0f);
    PlaneMesh->SetRelativeScale3D(FVector(ScaleXY, ScaleXY, 1.0f));
}

void AUnderwaterPlaneActor::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);
    ApplySize();
}
