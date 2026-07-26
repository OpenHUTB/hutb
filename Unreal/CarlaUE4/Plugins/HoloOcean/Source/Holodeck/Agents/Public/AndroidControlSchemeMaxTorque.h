#pragma once

#include "Holodeck.h"

#include "HolodeckPawnController.h"
#include "Android.h"
#include "HolodeckControlScheme.h"

#include "AndroidControlSchemeMaxTorque.generated.h"

/**
 * UAndroidControlSchemeMaxTorque
 */
UCLASS()
class HOLODECK_API UAndroidControlSchemeMaxTorque : public UHolodeckControlScheme
{
public:
	GENERATED_BODY()
	
	UAndroidControlSchemeMaxTorque(const FObjectInitializer& ObjectInitializer);

	void Execute(void* const CommandArray, void* const InputCommand, float DeltaSeconds) override;

	unsigned int GetControlSchemeSizeInBytes() const override {
		return 94 * sizeof(float);
	}

	void SetController(AHolodeckPawnController* const Controller) { AndroidController = Controller; };

private:
	AHolodeckPawnController* AndroidController;
	AAndroid* Android;

	const float TorqueScalarMuscles = 2.0f; // Muscles can apply more torque than fingers
	const float TorqueScalarFingers = 0.9f;
	const float MinCommand = -1.0f;
	const float MaxCommand =  1.0f;

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
	float calcTorqueToApply(float CommandValue, float BoneMass, float TorqueScalar=1.0f);

};
