// MIT License (c) 2019 BYU PCCL see LICENSE file

#include "Holodeck.h"
#include "SphereRobotController.h"

ASphereRobotController::ASphereRobotController(const FObjectInitializer& ObjectInitializer)
		: AHolodeckPawnController(ObjectInitializer) {
	UE_LOG(LogTemp, Warning, TEXT("SphereRobot Controller Initialized"));
}

ASphereRobotController::~ASphereRobotController() {}
