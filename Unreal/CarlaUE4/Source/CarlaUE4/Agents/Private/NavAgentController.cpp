// MIT License (c) 2019 BYU PCCL see LICENSE file

#include "Holodeck.h"
#include "NavAgentController.h"

ANavAgentController::ANavAgentController(const FObjectInitializer& ObjectInitializer)
	: AHolodeckPawnController(ObjectInitializer) {
	UE_LOG(LogHolodeck, Log, TEXT("NavAgent Controller Initialized"));
}

ANavAgentController::~ANavAgentController() {}
