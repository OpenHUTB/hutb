// Copyright (c) OpenHUTB. All rights reserved.

#include "UnderwaterFogActor.h"

#include "Components/ExponentialHeightFogComponent.h"

AUnderwaterFogActor::AUnderwaterFogActor()
{
    PrimaryActorTick.bCanEverTick = false;

    Fog = CreateDefaultSubobject<UExponentialHeightFogComponent>(TEXT("Fog"));
    SetRootComponent(Fog);

    // 雾组件默认是 Movable，这里显式设一次，避免蓝图实例化时被改成 Static
    Fog->SetMobility(EComponentMobility::Movable);
}

void AUnderwaterFogActor::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);

    if (!Fog)
    {
        return;
    }

    Fog->SetFogDensity(FogDensity);
    Fog->SetFogHeightFalloff(FogHeightFalloff);
    Fog->SetFogInscatteringColor(FogColor);
    Fog->SetFogMaxOpacity(FogMaxOpacity);
    Fog->SetStartDistance(StartDistance);
    Fog->SetFogCutoffDistance(FogCutoffDistance);
}
