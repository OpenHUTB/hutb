// MIT License (c) 2019 BYU PCCL see LICENSE file

#include "Holodeck.h"
#include "SphereRobot.h"


// Sets default values
ASphereRobot::ASphereRobot() {
	PrimaryActorTick.bCanEverTick = true;

	// Set the defualt controller
	AIControllerClass = LoadClass<AController>(NULL, TEXT("/Script/Holodeck.SphereRobotController"), NULL, LOAD_None, NULL);
	AutoPossessAI = EAutoPossessAI::PlacedInWorld;
}

void ASphereRobot::InitializeAgent() {
	Super::InitializeAgent();
}

// Called every frame
void ASphereRobot::Tick(float DeltaSeconds) {
	Super::Tick(DeltaSeconds);
	ForwardSpeed = FMath::Clamp(CommandArray[0], -MAX_FORWARD_SPEED, MAX_FORWARD_SPEED);
	RotSpeed = FMath::Clamp(CommandArray[1], -MAX_ROTATION_SPEED, MAX_ROTATION_SPEED);

	FVector DeltaLocation = GetActorForwardVector() * ForwardSpeed;
	DeltaLocation = ConvertLinearVector(DeltaLocation, ClientToUE);

	AddActorWorldOffset(DeltaLocation, true);
	FRotator DeltaRotation(0, RotSpeed, 0);

	DeltaRotation = ConvertAngularVector(DeltaRotation, ClientToUE);

	AddActorWorldRotation(DeltaRotation);
}
