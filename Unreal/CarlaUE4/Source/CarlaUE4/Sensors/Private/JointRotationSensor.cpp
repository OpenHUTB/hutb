#include "Holodeck.h"
#include "JointRotationSensor.h"

UJointRotationSensor::UJointRotationSensor() {
	PrimaryComponentTick.bCanEverTick = true;
	SensorName = "JointRotationSensor";
}

void UJointRotationSensor::InitializeSensor() {

	this->Parent = this->GetOwner();

	if (this->Parent->IsA(AAndroid::StaticClass())) {
		this->Joints.Append(AAndroid::Joints, AAndroid::NUM_JOINTS);

		this->TotalDof = AAndroid::TOTAL_DOF;
		this->Num3AxisJoints = AAndroid::NUM_3_AXIS_JOINTS;
		this->Num2AxisJoints = AAndroid::NUM_2_AXIS_JOINTS;
	}
	else if (this->Parent->IsA(AHandAgent::StaticClass())) {
		this->Joints.Append(AHandAgent::Joints, AHandAgent::NUM_JOINTS);

		this->TotalDof = AHandAgent::TOTAL_JOINT_DOF;
		this->Num3AxisJoints = AHandAgent::NUM_3_AXIS_JOINTS;
		this->Num2AxisJoints = AHandAgent::NUM_2_AXIS_JOINTS;

	}
	else {
		UE_LOG(LogHolodeck, Fatal, TEXT("Error: Tried to use UJointRotationSensor with unknown agent type."));
	}

	TArray<USkeletalMeshComponent*> Components;
	this->Parent->GetComponents<USkeletalMeshComponent>(Components);
	this->SkeletalMeshComponent = Components[0];

	Super::InitializeSensor();
}

int UJointRotationSensor::GetNumItems() {
	return this->TotalDof;
}

void UJointRotationSensor::TickSensorComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) {
	float* FloatBuffer = static_cast<float*>(Buffer);

	for (int JointInd = 0; JointInd < this->Joints.Num(); JointInd++) {

		FString JointName = Joints[JointInd].ToString();

		FloatBuffer = AddJointRotationToBuffer(JointName, true, 
														  JointInd < this->Num2AxisJoints + this->Num3AxisJoints, 
														  JointInd < this->Num3AxisJoints, 
			                                              FloatBuffer);
	}
}

float* UJointRotationSensor::AddJointRotationToBuffer(FString JointName, bool Swing1, bool Swing2, bool Twist, float* Data) {
	FConstraintInstance* Constraint = SkeletalMeshComponent->FindConstraintInstance(FName(*JointName));

	if (Swing1) {
		*Data = Constraint->GetCurrentSwing1();
		Data += 1; // pointer arithmetic, fun
	}
	if (Swing2) {
		*Data = Constraint->GetCurrentSwing2();
		Data += 1;
	}
	if (Twist) {
		*Data = Constraint->GetCurrentTwist();
		Data += 1;
	}

	return Data;
}
