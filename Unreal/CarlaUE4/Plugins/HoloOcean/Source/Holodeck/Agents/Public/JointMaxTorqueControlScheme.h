#pragma once

#include "Holodeck.h"

#include "HolodeckPawnController.h"
#include "Android.h"
#include "HolodeckControlScheme.h"

#include "JointMaxTorqueControlScheme.generated.h"

/**
 * UAndroidControlSchemeMaxTorque
 */
UCLASS()
class HOLODECK_API UJointMaxTorqueControlScheme : public UHolodeckControlScheme
{
public:
	GENERATED_BODY()

	UJointMaxTorqueControlScheme(const FObjectInitializer& ObjectInitializer);

	void Execute(void* const CommandArray, void* const InputCommand, float DeltaSeconds) override;

	unsigned int GetControlSchemeSizeInBytes() const override;

	/**
	 * Must call this method to set the control scheme size after creating this object
	*/
	void SetControlSchemeSizeInBytes(int size);

	/**
	 * Must call this method to give a reference to SkeletalMesh and an array of indexed joint names
	 */
	void SetSkeletalMesh(USkeletalMeshComponent* SkeletalMesh, FName Joints[]);

	/**
	 * Fingers are less powerful than joints, after this index we will use TorqueScalarFingers
	 * instead of TorqueScalarMuscles
	 */
	void SetFingerStartIndex(int size);
	void SetJointSizes(int ThreeAxis, int TwoAxis, int OneAxis);

	void SetController(AHolodeckPawnController* const NewController) { this->Controller = NewController; };

private:
	AHolodeckPawnController* Controller;
	USkeletalMeshComponent* SkeletalMesh;
	FName* JointNames;

	const float TorqueScalarMuscles = 5.0f; // Muscles can apply more torque than fingers
	const float TorqueScalarFingers = 1.2f;
	const float MinCommand = -1.0f;
	const float MaxCommand = 1.0f;

	unsigned int ControlSchemeSizeBytes = 0;
	unsigned int FingerStartIndex = 0;
	unsigned int ThreeAxisJoints, TwoAxisJoints, OneAxisJoints;

	/**
	 * calcTorqueToApply
	 *
	 * Takes the input command, the bone mass, and the specific scalar value and computes the torque to apply
	 *
	 * CommandValue is between -1.0 and 1.0. It represents the percentage of the maximum torque to apply.
	 *
	 * The maximum Torque is calculated by multiplying the bone mass and the given torque scalar.
	 * Bone mass is in kg, TorqueScalar is (Neutonmeters/kg)
	 *
	*/
	float CalculateTorque(float CommandValue, float BoneMass, float TorqueScalar = 1.0f);

};
