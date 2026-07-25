#include "Holodeck.h"
#include "AndroidControlSchemeMaxTorque.h"




UAndroidControlSchemeMaxTorque::UAndroidControlSchemeMaxTorque(const FObjectInitializer& ObjectInitializer) :
		Super(ObjectInitializer) {}

void UAndroidControlSchemeMaxTorque::Execute(void* const CommandArray, void* const InputCommand, float DeltaSeconds) {
    float* InputCommandFloat = static_cast<float*>(InputCommand);
    float* CommandArrayFloat = static_cast<float*>(CommandArray);

    if (Android == nullptr) {
		Android = static_cast<AAndroid*>(AndroidController->GetPawn());
		if (Android == nullptr) {
			UE_LOG(LogHolodeck, Error, TEXT("UAndroidControlSchemeMaxTorque couldn't get Android reference"));
			return;
		}
	}

    USkeletalMeshComponent* SkeletalMesh =  Android->SkeletalMesh;

    int ComInd = 0;

	for (int BoneInd = 0; BoneInd < Android->NUM_JOINTS; BoneInd++) {

        // Joint names directly correspond to bone names
		FName BoneName = Android->Joints[BoneInd];

        // Get the mass of the bone connected to this joint
        float BoneMass = SkeletalMesh->GetBoneMass(BoneName, true);

        // Set the Scalar depending on whether its a finger joint or large muscle joint
        float scalar = TorqueScalarFingers;
        if (BoneInd < Android->NUM_3_AXIS_JOINTS){
            scalar = TorqueScalarMuscles;
        }

        CommandArrayFloat[ComInd] = calcTorqueToApply(InputCommandFloat[ComInd], BoneMass, scalar);
		ComInd++;

		// Apply Swing 2 if Torque non is 2 or 3 axis joint
		if (BoneInd < (Android->NUM_2_PLUS_3_AXIS_JOINTS)) {
			CommandArrayFloat[ComInd] = calcTorqueToApply(InputCommandFloat[ComInd], BoneMass, scalar);
			ComInd++;

			// Apply Twist if Torque is 3 axis joint
			if (BoneInd < Android->NUM_3_AXIS_JOINTS) {
                CommandArrayFloat[ComInd] = calcTorqueToApply(InputCommandFloat[ComInd], BoneMass, scalar);
				ComInd++;
			}
		}
	}
}

float UAndroidControlSchemeMaxTorque::calcTorqueToApply(float CommandValue, float BoneMass, float TorqueScalar){
    CommandValue = FMath::Clamp(CommandValue, MinCommand, MaxCommand);
    return CommandValue * ( BoneMass * TorqueScalar ); // See function declaration for explaination of equation
}
