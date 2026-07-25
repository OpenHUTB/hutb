// MIT License (c) 2019 BYU PCCL see LICENSE file

#include "Holodeck.h"
#include "HandAgentController.h"

AHandAgentController::AHandAgentController(const FObjectInitializer& ObjectInitializer) : AHolodeckPawnController(ObjectInitializer) {
	UE_LOG(LogHolodeck, Log, TEXT("Hand Agent Controller Initialized"));
}

void AHandAgentController::AddControlSchemes() {
	this->JointTorqueControlScheme = NewObject<UJointMaxTorqueControlScheme>();
	this->JointTorqueControlScheme->SetController(this);
	this->JointTorqueControlScheme->SetControlSchemeSizeInBytes(AHandAgent::TOTAL_JOINT_DOF);

	this->JointTorqueControlScheme->SetJointSizes(
		AHandAgent::NUM_3_AXIS_JOINTS,
		AHandAgent::NUM_2_AXIS_JOINTS,
		AHandAgent::NUM_1_AXIS_JOINTS);

	this->JointTorqueControlScheme->SetFingerStartIndex(AHandAgent::NUM_3_AXIS_JOINTS * 3 - 1);

	// Create the floating control scheme too
	this->HandAgentFloatControlScheme = NewObject<UHandAgentMaxTorqueFloat>();
	this->HandAgentFloatControlScheme->SetTorqueControlScheme(this->JointTorqueControlScheme);

	ControlSchemes.Add(this->JointTorqueControlScheme);
	ControlSchemes.Add(this->HandAgentFloatControlScheme);

}

void AHandAgentController::OnPossess(APawn* PawnParam) {
	Super::OnPossess(PawnParam);
	UE_LOG(LogHolodeck, Log, TEXT("HandAgent Controller possessing pawn"));

	TArray<USkeletalMeshComponent*> Components;
	PawnParam->GetComponents<USkeletalMeshComponent>(Components);

	if (Components.Num() < 1) {
		UE_LOG(LogHolodeck, Fatal, TEXT("Couldn't find USkeletelMeshComponent for PawnParam"));
	}

	SkeletalMeshComponent = Components[0];

	this->JointTorqueControlScheme->SetSkeletalMesh(SkeletalMeshComponent, const_cast<FName*>(AHandAgent::Joints));

	ActionBufferFloatPtr = static_cast<float*>(ActionBuffer);
}
