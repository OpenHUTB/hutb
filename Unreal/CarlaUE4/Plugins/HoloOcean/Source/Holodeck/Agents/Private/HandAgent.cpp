// MIT License (c) 2019 BYU PCCL see LICENSE file

#include "Holodeck.h"
#include "HandAgent.h"

AHandAgent::AHandAgent() {
	PrimaryActorTick.bCanEverTick = true;

	// Set the default controller
	AIControllerClass = LoadClass<AController>(NULL, TEXT("/Script/Holodeck.HandAgentController"), NULL, LOAD_None, NULL);
	AutoPossessAI = EAutoPossessAI::PlacedInWorld;
}

void AHandAgent::InitializeAgent() {
	Super::InitializeAgent();
	SkeletalMesh = Cast<USkeletalMeshComponent>(RootComponent);
	memset(this->CommandArray, 0, sizeof this->CommandArray);
}

void AHandAgent::Tick(float DeltaTime) {
	Super::Tick(DeltaTime);
	ApplyTorques();
	ApplyLevitation();
}

void AHandAgent::ApplyTorques() {
	UE_LOG(LogHolodeck, Verbose, TEXT("AHandAgent::ApplyTorques"));
	int ComInd = 0;

	for (int JointInd = 0; JointInd < NUM_JOINTS; JointInd++) {

		FName JointName = Joints[JointInd];

		// Get rotation of that socket
		FQuat RotQuat = SkeletalMesh->GetSocketQuaternion(JointName);
		FVector RotationVector = FVector(0.0f, 0.0f, 0.0f);

		// Apply Swing 1 Torque if non zero
		if (CommandArray[ComInd] != 0) {
			float RotForce = FMath::Clamp(CommandArray[ComInd], -MAX_TORQUE, MAX_TORQUE);
			RotationVector.Z = RotForce;
		}
		ComInd++;

		// Apply Swing 2 if Torque non zero and is 2 or 3 axis joint
		if (JointInd < (NUM_2_PLUS_3_AXIS_JOINTS)) {
			if (CommandArray[ComInd] != 0) {
				float RotForce = FMath::Clamp(CommandArray[ComInd], -MAX_TORQUE, MAX_TORQUE);
				RotationVector.Y = RotForce;
			}
			ComInd++;

			// Apply Twist if Torque non zero and is 3 axis joint
			if (JointInd < NUM_3_AXIS_JOINTS) {
				if (CommandArray[ComInd] != 0) {
					float RotForce = FMath::Clamp(CommandArray[ComInd], -MAX_TORQUE, MAX_TORQUE);
					RotationVector.X = RotForce;
				}
				ComInd++;
			}
		}
		// Convert torque from m/rhs to cm/lhs
		RotationVector = ConvertTorque(RotationVector, ClientToUE);
		SkeletalMesh->AddTorqueInRadians(RotQuat.RotateVector(RotationVector), JointName, false);
	}
}

void AHandAgent::ApplyLevitation() {
	float deltaX = std::min(CommandArray[AHandAgent::TOTAL_JOINT_DOF], MAX_MOVEMENT_METERS);
	float deltaY = std::min(CommandArray[AHandAgent::TOTAL_JOINT_DOF + 1], MAX_MOVEMENT_METERS);
	float deltaZ = std::min(CommandArray[AHandAgent::TOTAL_JOINT_DOF + 2], MAX_MOVEMENT_METERS);

	FVector DeltaLocation = FVector(deltaX, deltaY, deltaZ);

	DeltaLocation = ConvertLinearVector(DeltaLocation, ClientToUE);

	AddActorWorldOffset(DeltaLocation, true, NULL, ETeleportType::None);
}
 
const FName AHandAgent::Joints[] = {

	// Head, Spine, and Arm joints. Each has [swing1, swing2, twist]
	FName(TEXT("hand_r")),

	// First joint of each finger. Has only [swing1, swing2]
	FName(TEXT("thumb_01_r")),
	FName(TEXT("index_01_r")),
	FName(TEXT("middle_01_r")),
	FName(TEXT("ring_01_r")),
	FName(TEXT("pinky_01_r")),

	// Second joint of each finger. Has only [swing1]
	FName(TEXT("thumb_02_r")),
	FName(TEXT("index_02_r")),
	FName(TEXT("middle_02_r")),
	FName(TEXT("ring_02_r")),
	FName(TEXT("pinky_02_r")),

	// Third joint of each finger. Has only [swing1]
	FName(TEXT("thumb_03_r")),
	FName(TEXT("index_03_r")),
	FName(TEXT("middle_03_r")),
	FName(TEXT("ring_03_r")),
	FName(TEXT("pinky_03_r")),
};

// Don't forget to update AHandAgent::NumBones after changing this array!
const FName AHandAgent::BoneNames[] = {
	FName(TEXT("lowerarm_r")),
	FName(TEXT("hand_r")),
	FName(TEXT("index_01_r")),
	FName(TEXT("index_02_r")),
	FName(TEXT("index_03_r")),
	FName(TEXT("middle_01_r")),
	FName(TEXT("middle_02_r")),
	FName(TEXT("middle_03_r")),
	FName(TEXT("pinky_01_r")),
	FName(TEXT("pinky_02_r")),
	FName(TEXT("pinky_03_r")),
	FName(TEXT("ring_01_r")),
	FName(TEXT("ring_02_r")),
	FName(TEXT("ring_03_r")),
	FName(TEXT("thumb_01_r")),
	FName(TEXT("thumb_02_r")),
	FName(TEXT("thumb_03_r"))
};

// If you change this number, change the corresponding number in RelativeSkeletalPositionSensor.__init__
const int AHandAgent::NumBones = 17;
