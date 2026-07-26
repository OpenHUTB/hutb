// MIT License (c) 2019 BYU PCCL see LICENSE file

#include "Android.h"
#include "Holodeck.h"

AAndroid::AAndroid() {
	PrimaryActorTick.bCanEverTick = true;
	bCollisionsAreVisible = false;

	// Set the defualt controller
	AIControllerClass = LoadClass<AController>(NULL, TEXT("/Script/Holodeck.AndroidController"), NULL, LOAD_None, NULL);
	AutoPossessAI = EAutoPossessAI::PlacedInWorld;
}

void AAndroid::InitializeAgent() {
	Super::InitializeAgent();
	SkeletalMesh = Cast<USkeletalMeshComponent>(RootComponent);
}

void AAndroid::Tick(float DeltaTime) {
	Super::Tick(DeltaTime);
	ApplyTorques();
}

void AAndroid::SetCollisionsVisible(bool Visible) {
	bCollisionsAreVisible = Visible;
}

bool AAndroid::GetCollisionsVisible() {
	return bCollisionsAreVisible;
}

void AAndroid::ApplyTorques() {
	UE_LOG(LogHolodeck, Verbose, TEXT("AAndroid::ApplyTorques"));
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

const FName AAndroid::Joints[] = {

	// Head, Spine, and Arm joints. Each has [swing1, swing2, twist]
	FName(TEXT("head")),
	FName(TEXT("neck_01")),
	FName(TEXT("spine_02")),
	FName(TEXT("spine_01")),
	FName(TEXT("upperarm_l")),
	FName(TEXT("lowerarm_l")),
	FName(TEXT("hand_l")),
	FName(TEXT("upperarm_r")),
	FName(TEXT("lowerarm_r")),
	FName(TEXT("hand_r")),

	// Leg Joints. Each has [swing1, swing2, twist]
	FName(TEXT("thigh_l")),
	FName(TEXT("calf_l")),
	FName(TEXT("foot_l")),
	FName(TEXT("ball_l")),
	FName(TEXT("thigh_r")),
	FName(TEXT("calf_r")),
	FName(TEXT("foot_r")),
	FName(TEXT("ball_r")),

	// First joint of each finger. Has only [swing1, swing2]
	FName(TEXT("thumb_01_l")),
	FName(TEXT("index_01_l")),
	FName(TEXT("middle_01_l")),
	FName(TEXT("ring_01_l")),
	FName(TEXT("pinky_01_l")),
	FName(TEXT("thumb_01_r")),
	FName(TEXT("index_01_r")),
	FName(TEXT("middle_01_r")),
	FName(TEXT("ring_01_r")),
	FName(TEXT("pinky_01_r")),

	// Second joint of each finger. Has only [swing1]
	FName(TEXT("thumb_02_l")),
	FName(TEXT("index_02_l")),
	FName(TEXT("middle_02_l")),
	FName(TEXT("ring_02_l")),
	FName(TEXT("pinky_02_l")),
	FName(TEXT("thumb_02_r")),
	FName(TEXT("index_02_r")),
	FName(TEXT("middle_02_r")),
	FName(TEXT("ring_02_r")),
	FName(TEXT("pinky_02_r")),

	// Third joint of each finger. Has only [swing1]
	FName(TEXT("thumb_03_l")),
	FName(TEXT("index_03_l")),
	FName(TEXT("middle_03_l")),
	FName(TEXT("ring_03_l")),
	FName(TEXT("pinky_03_l")),
	FName(TEXT("thumb_03_r")),
	FName(TEXT("index_03_r")),
	FName(TEXT("middle_03_r")),
	FName(TEXT("ring_03_r")),
	FName(TEXT("pinky_03_r")),
};

// Don't forget to update AAndroid::NumBones after changing this array!
const FName AAndroid::BoneNames[] = {
	FName(TEXT("pelvis")),
	FName(TEXT("spine_01")),
	FName(TEXT("spine_02")),
	FName(TEXT("spine_03")),
	FName(TEXT("clavicle_l")),
	FName(TEXT("upperarm_l")),
	FName(TEXT("lowerarm_l")),
	FName(TEXT("hand_l")),
	FName(TEXT("index_01_l")),
	FName(TEXT("index_02_l")),
	FName(TEXT("index_03_l")),
	FName(TEXT("middle_01_l")),
	FName(TEXT("middle_02_l")),
	FName(TEXT("middle_03_l")),
	FName(TEXT("pinky_01_l")),
	FName(TEXT("pinky_02_l")),
	FName(TEXT("pinky_03_l")),
	FName(TEXT("ring_01_l")),
	FName(TEXT("ring_02_l")),
	FName(TEXT("ring_03_l")),
	FName(TEXT("thumb_01_l")),
	FName(TEXT("thumb_02_l")),
	FName(TEXT("thumb_03_l")),
	FName(TEXT("lowerarm_twist_01_l")),
	FName(TEXT("upperarm_twist_01_l")),
	FName(TEXT("clavicle_r")),
	FName(TEXT("upperarm_r")),
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
	FName(TEXT("thumb_03_r")),
	FName(TEXT("lowerarm_twist_01_r")),
	FName(TEXT("upperarm_twist_01_r")),
	FName(TEXT("neck_01")),
	FName(TEXT("head")),
	FName(TEXT("thigh_l")),
	FName(TEXT("calf_l")),
	FName(TEXT("calf_twist_01_l")),
	FName(TEXT("foot_l")),
	FName(TEXT("ball_l")),
	FName(TEXT("thigh_twist_01_l")),
	FName(TEXT("thigh_r")),
	FName(TEXT("calf_r")),
	FName(TEXT("calf_twist_01_r")),
	FName(TEXT("foot_r")),
	FName(TEXT("ball_r")),
	FName(TEXT("thigh_twist_01_r"))
};

// If you change this number, change the corresponding number in RelativeSkeletalPositionSensor.__init__
const int AAndroid::NumBones = 60;