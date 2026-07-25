// MIT License (c) 2019 BYU PCCL see LICENSE file
#include "Holodeck.h"
#include "Uav.h"


AUav::AUav() {
	// Set this pawn to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	// Set the defualt controller
	AIControllerClass = LoadClass<AController>(NULL, TEXT("/Script/Holodeck.UavController"), NULL, LOAD_None, NULL);
	AutoPossessAI = EAutoPossessAI::PlacedInWorld;

	// TODO: the unreal unit conversion should be derived via global settings
	// TODO: the physics substep doesn't seem to have updated velocity/position estimates - we likely need to access the physx body directly (e.g. https://github.com/EpicGames/UnrealEngine/pull/585)
	// TODO: add changes seen in https://answers.unrealengine.com/questions/7459/question-is-120-the-engine-max-frame-rate.html
	// TODO: set deltaTick to 1/40th of a second
	SetActorEnableCollision(true);
	// OnCalculateCustomPhysics.BindUObject(this, &AUav::SubstepTick);
}

void AUav::InitializeAgent() {
	Super::InitializeAgent();
	RootMesh = Cast<UStaticMeshComponent>(RootComponent);
}

void AUav::Tick(float DeltaTime) {
	Super::Tick(DeltaTime);
	ApplyForces();
}

void AUav::ApplyForces() {
	float RollTorqueToApply = FMath::Clamp(GetRollTorqueToApply(), -UAV_MAX_ROLL, UAV_MAX_ROLL);
	float PitchTorqueToApply = FMath::Clamp(GetPitchTorqueToApply(), -UAV_MAX_PITCH, UAV_MAX_PITCH);
	float YawTorqueToApply = FMath::Clamp(GetYawTorqueToApply(), -UAV_MAX_YAW_RATE, UAV_MAX_YAW_RATE);
	float ThrustToApply = FMath::Clamp(GetThrustToApply(), -UAV_MAX_FORCE, UAV_MAX_FORCE);

	FVector LocalThrust = FVector(0, 0, ThrustToApply);
	LocalThrust = ConvertLinearVector(LocalThrust, ClientToUE);
	FVector LocalTorque = FVector(RollTorqueToApply, PitchTorqueToApply, YawTorqueToApply);
	LocalTorque = ConvertTorque(LocalTorque, ClientToUE);

	// Apply torques and forces in global coordinates
	RootMesh->AddTorqueInRadians(GetActorRotation().RotateVector(LocalTorque));
	RootMesh->AddForce(GetActorRotation().RotateVector(LocalThrust));
}
