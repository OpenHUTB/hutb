// MIT License (c) 2021 BYU FRoStLab see LICENSE file

#include "CarlaUE4.h"
#include "HoveringAUVController.h"

AHoveringAUVController::AHoveringAUVController(const FObjectInitializer& ObjectInitializer)
	: AHolodeckPawnController(ObjectInitializer) {
	UE_LOG(LogTemp, Warning, TEXT("HoveringAUV Controller Initialized"));
}

AHoveringAUVController::~AHoveringAUVController() {}
