#include "Holodeck.h"
#include "NavAgent.h"


// Sets default values
ANavAgent::ANavAgent() {
	PrimaryActorTick.bCanEverTick = true;

	// Set the defualt controller
	AIControllerClass = LoadClass<AController>(NULL, TEXT("/Script/Holodeck.NavAgentController"), NULL, LOAD_None, NULL);
	AutoPossessAI = EAutoPossessAI::PlacedInWorld;
}

void ANavAgent::InitializeAgent() {
	Super::InitializeAgent();
	Target = this->GetActorLocation();
}

// Called every frame
void ANavAgent::Tick(float DeltaSeconds) {
	Super::Tick(DeltaSeconds);
	Target = FVector(CommandArray[0], CommandArray[1], CommandArray[2]);
	Target = ConvertLinearVector(Target, ClientToUE);
}

FVector ANavAgent::GetTarget() {
	return Target;
}

void ANavAgent::SetTarget(float x, float y, float z) {
	Target = FVector(x, y, z);
}
