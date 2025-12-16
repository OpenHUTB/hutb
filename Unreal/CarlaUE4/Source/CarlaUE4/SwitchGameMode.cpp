// Copyright (c) 2025 OpenHUTB at the Human University of Technology and Business (HUTB). This work is licensed under the terms of the MIT license. For a copy, see <https://opensource.org/licenses/MIT>.


#include "SwitchGameMode.h"

// Sets default values
ASwitchGameMode::ASwitchGameMode()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

}

// Called when the game starts or when spawned
void ASwitchGameMode::BeginPlay()
{
	Super::BeginPlay();
	// 绑定事件
	auto controller = this->GetWorld()->GetFirstPlayerController();
	EnableInput(controller);
	UInputComponent* comp = controller->InputComponent;
	comp->BindAction("SwitchGameMode", IE_Pressed, this, &ASwitchGameMode::SwitchGameModeAction);
}

void ASwitchGameMode::SwitchGameModeAction()
{
	if (GEngine)
	{
		UGameplayStatics::OpenLevel(this, FName("/Game/Carla/Maps/Town10HD"), false, "/Game/Carla/Maps/Town10HD?GAME=AIR");
		GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Red, TEXT("Switched to air mode."));
	}
}

// Called every frame
void ASwitchGameMode::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

