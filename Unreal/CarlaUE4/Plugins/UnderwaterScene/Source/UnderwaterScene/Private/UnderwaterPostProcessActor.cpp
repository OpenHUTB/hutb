// Copyright (c) OpenHUTB. All rights reserved.

#include "UnderwaterPostProcessActor.h"

#include "Components/PostProcessComponent.h"
#include "Materials/MaterialInterface.h"
#include "Materials/Material.h"

AUnderwaterPostProcessActor::AUnderwaterPostProcessActor()
{
    PrimaryActorTick.bCanEverTick = false;

    PostProcess = CreateDefaultSubobject<UPostProcessComponent>(TEXT("PostProcess"));
    SetRootComponent(PostProcess);

    // Unbound：整个场景都吃这个后处理。
    // M_Underwater_PP 内部按相机深度衰减，所以水面之上自然没有效果。
    PostProcess->bUnbound = true;
    PostProcess->bEnabled = true;
    PostProcess->Priority = 10.0f;
}

void AUnderwaterPostProcessActor::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);

    if (!bAutoAssignUnderwaterMaterial || !PostProcess)
    {
        return;
    }

    // 已经有混合项就不再重复添加（OnConstruction 会被多次调用）
    if (PostProcess->Settings.WeightedBlendables.Array.Num() > 0)
    {
        return;
    }

    UMaterialInterface* UnderwaterMat = LoadObject<UMaterialInterface>(
        nullptr,
        TEXT("/OceanPlugin/Ocean/Materials/M_Underwater_PP.M_Underwater_PP"));

    if (!UnderwaterMat)
    {
        UE_LOG(LogTemp, Warning,
            TEXT("[UnderwaterPostProcess] 找不到 M_Underwater_PP，后处理未挂载"));
        return;
    }

    PostProcess->Settings.WeightedBlendables.Array.Add(
        FWeightedBlendable(1.0f, UnderwaterMat));

    UE_LOG(LogTemp, Log,
        TEXT("[UnderwaterPostProcess] 已挂载 M_Underwater_PP（权重 1.0）"));
}
