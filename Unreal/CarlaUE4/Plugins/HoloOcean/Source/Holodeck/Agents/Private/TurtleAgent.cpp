// MIT License (c) 2019 BYU PCCL see LICENSE file

#include "Holodeck.h"
#include "TurtleAgent.h"


// Sets default values
ATurtleAgent::ATurtleAgent() {
	PrimaryActorTick.bCanEverTick = true;

	// Set the defualt controller
	AIControllerClass = LoadClass<AController>(NULL, TEXT("/Script/Holodeck.TurtleAgentController"), NULL, LOAD_None, NULL);
	AutoPossessAI = EAutoPossessAI::PlacedInWorld;
}

void ATurtleAgent::InitializeAgent() {
	Super::InitializeAgent();
	RootMesh = Cast<UStaticMeshComponent>(RootComponent);
}

// Called every frame
void ATurtleAgent::Tick(float DeltaSeconds) {
	Super::Tick(DeltaSeconds);
	float ForwardForce = CommandArray[0];
	float RotForce = CommandArray[1];

	float ThrustToApply = FMath::Clamp(ForwardForce, -MAX_THRUST, MAX_THRUST);
	float YawTorqueToApply = FMath::Clamp(RotForce, -MAX_YAW, MAX_YAW);

	FVector LocalThrust = FVector(ThrustToApply, 0, 0);
	LocalThrust = ConvertLinearVector(LocalThrust, ClientToUE);

	FVector LocalTorque = FVector(0, 0, YawTorqueToApply);
	LocalTorque = ConvertTorque(LocalTorque, ClientToUE);

	RootMesh->AddTorqueInRadians(GetActorRotation().RotateVector(LocalTorque));
	RootMesh->AddForce(GetActorRotation().RotateVector(LocalThrust));

	// If the turtle is upside down it is abused
	if (this->GetActorUpVector().Z < -0.5) {
		this->IsAbused = true;
	}
}



