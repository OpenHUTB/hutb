#include "Holodeck.h"
#include "JointMaxTorqueControlScheme.h"


UJointMaxTorqueControlScheme::UJointMaxTorqueControlScheme(const FObjectInitializer& ObjectInitializer) :
	Super(ObjectInitializer) {}

unsigned int UJointMaxTorqueControlScheme::GetControlSchemeSizeInBytes() const {
	return this->ControlSchemeSizeBytes;
}

void UJointMaxTorqueControlScheme::SetControlSchemeSizeInBytes(int size) {
	this->ControlSchemeSizeBytes = size;
}

void UJointMaxTorqueControlScheme::SetSkeletalMesh(USkeletalMeshComponent* Skeletal, FName Joints[]) {
	this->SkeletalMesh = Skeletal;
	this->JointNames = Joints;
}

void UJointMaxTorqueControlScheme::SetJointSizes(int ThreeAxis, int TwoAxis, int OneAxis) {
	this->ThreeAxisJoints = ThreeAxis;
	this->TwoAxisJoints = TwoAxis;
	this->OneAxisJoints = OneAxis;
}

void UJointMaxTorqueControlScheme::SetFingerStartIndex(int size) {
	this->FingerStartIndex = size;
}

void UJointMaxTorqueControlScheme::Execute(void* const CommandArray, void* const InputCommand, float DeltaSeconds) {
	float* InputCommandFloat = static_cast<float*>(InputCommand);
	float* CommandArrayFloat = static_cast<float*>(CommandArray);

	int ComInd = 0;

	unsigned TotalJoints = this->ThreeAxisJoints + this->TwoAxisJoints + this->OneAxisJoints;

	for (unsigned BoneInd = 0; BoneInd < TotalJoints; BoneInd++) {

		// Joint names directly correspond to bone names
		FName BoneName = this->JointNames[BoneInd];

		// Get the mass of the bone connected to this joint
		float BoneMass = SkeletalMesh->GetBoneMass(BoneName, true);

		// Set the Scalar depending on whether its a finger joint or large muscle joint
		float scalar = TorqueScalarFingers;

		if (BoneInd < this->FingerStartIndex) {
			scalar = TorqueScalarMuscles;
		}

		CommandArrayFloat[ComInd] = this->CalculateTorque(InputCommandFloat[ComInd], BoneMass, scalar);
		ComInd++;

		// Apply Swing 2 if Torque non is 2 or 3 axis joint
		if (BoneInd < (this->ThreeAxisJoints + this->TwoAxisJoints)) {
			CommandArrayFloat[ComInd] = this->CalculateTorque(InputCommandFloat[ComInd], BoneMass, scalar);
			ComInd++;

			// Apply Twist if Torque is 3 axis joint
			if (BoneInd < this->ThreeAxisJoints) {
				CommandArrayFloat[ComInd] = this->CalculateTorque(InputCommandFloat[ComInd], BoneMass, scalar);
				ComInd++;
			}
		}
	}
}

float UJointMaxTorqueControlScheme::CalculateTorque(float CommandValue, float BoneMass, float TorqueScalar) {
	CommandValue = FMath::Clamp(CommandValue, MinCommand, MaxCommand);
	return CommandValue * (BoneMass * TorqueScalar); // See function declaration for explanation of equation
}
