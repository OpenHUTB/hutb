// MIT License (c) 2019 BYU PCCL see LICENSE file

#include "Holodeck.h"
#include "AndroidController.h"

AAndroidController::AAndroidController(const FObjectInitializer& ObjectInitializer) : AHolodeckPawnController(ObjectInitializer) {
	UE_LOG(LogHolodeck, Log, TEXT("Android Controller Initialized"));
}

AAndroidController::~AAndroidController() {}

void AAndroidController::OnPossess(APawn* PawnParam) {
	Super::OnPossess(PawnParam);
	UE_LOG(LogHolodeck, Log, TEXT("Android Controller possessing pawn"));

	TArray<USkeletalMeshComponent*> Components;
	PawnParam->GetComponents<USkeletalMeshComponent>(Components);

	if (Components.Num() < 1) {
		UE_LOG(LogHolodeck, Error, TEXT("Couldn't find USkeletelMeshComponent for PawnParam"));
	} else {
		SkeletalMeshComponent = Components[0];
	}

	this->ControlScheme->SetSkeletalMesh(this->SkeletalMeshComponent, const_cast<FName*>(AAndroid::Joints));

	ActionBufferFloatPtr = static_cast<float*>(ActionBuffer);
}

void AAndroidController::AddControlSchemes() {

	ControlScheme = NewObject<UJointMaxTorqueControlScheme>();
	ControlScheme->SetController(this);
	ControlScheme->SetControlSchemeSizeInBytes(AAndroid::TOTAL_DOF);

	ControlScheme->SetJointSizes(
		AAndroid::NUM_3_AXIS_JOINTS,
		AAndroid::NUM_2_AXIS_JOINTS,
		AAndroid::NUM_1_AXIS_JOINTS
	);

	ControlScheme->SetFingerStartIndex(AAndroid::NUM_3_AXIS_JOINTS * 3 - 1);

	ControlSchemes.Add(ControlScheme);

}