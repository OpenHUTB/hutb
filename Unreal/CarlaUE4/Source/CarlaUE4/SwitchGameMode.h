// Copyright (c) 2025 OpenHUTB at the Human University of Technology and Business (HUTB). This work is licensed under the terms of the MIT license. For a copy, see <https://opensource.org/licenses/MIT>.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SwitchGameMode.generated.h"

UCLASS()
class CARLAUE4_API ASwitchGameMode : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	ASwitchGameMode();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

	void SwitchGameModeAction();

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;

};
