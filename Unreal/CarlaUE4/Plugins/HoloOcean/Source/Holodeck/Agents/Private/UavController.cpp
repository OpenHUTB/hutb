// MIT License (c) 2019 BYU PCCL see LICENSE file

#include "Holodeck.h"
#include "UavController.h"

AUavController::AUavController(const FObjectInitializer& ObjectInitializer)
		: AHolodeckPawnController(ObjectInitializer) {
	UE_LOG(LogTemp, Warning, TEXT("UAV Controller Initialized"));
}

AUavController::~AUavController() {}
