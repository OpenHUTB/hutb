// MIT License (c) 2019 BYU PCCL see LICENSE file

#include "Holodeck.h"
#include "RelativeSkeletalPositionSensor.h"

URelativeSkeletalPositionSensor::URelativeSkeletalPositionSensor() {
	PrimaryComponentTick.bCanEverTick = true;
	SensorName = "RelativeSkeletalPositionSensor";
}

void URelativeSkeletalPositionSensor::InitializeSensor() {

	AActor* Parent = this->GetOwner();

	if (Parent->IsA(AAndroid::StaticClass())) {
		this->Bones.Append(AAndroid::BoneNames, AAndroid::NumBones);
	}
	else if (Parent->IsA(AHandAgent::StaticClass())) {
		// is AHandAgent
		this->Bones.Append(AHandAgent::BoneNames, AHandAgent::NumBones);
	}
	else {
		UE_LOG(LogHolodeck, Fatal, TEXT("Error: Tried to use URelativeSkeletalPositionSensor with unknown agent type."));
	}

	TArray<USkeletalMeshComponent*> Components;
	Parent->GetComponents<USkeletalMeshComponent>(Components);
	this->SkeletalMeshComponent = Components[0];

	// Save arrays of bones and parent bones
	this->ParentBones.Reserve(this->Bones.Num());

	for (int i = 0; i < this->Bones.Num(); i++) {
		this->ParentBones.Insert(this->SkeletalMeshComponent->GetParentBone(this->Bones[i]), i);
	}

	// Defer this call to the end because we don't know the size of Bones() at the beginning of the method
	Super::InitializeSensor();
}

void URelativeSkeletalPositionSensor::TickSensorComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) {
	float* FloatBuffer = static_cast<float*>(Buffer);
	for (int i = 0; i < Bones.Num(); i++) {
		FQuat BoneQ = SkeletalMeshComponent->GetBoneQuaternion(Bones[i], EBoneSpaces::WorldSpace);
		FQuat ParentQ = SkeletalMeshComponent->GetBoneQuaternion(ParentBones[i], EBoneSpaces::WorldSpace);
		FQuat Quat = ParentQ.Inverse() * BoneQ;
		FloatBuffer[4 * i] = Quat.X;
		FloatBuffer[4 * i + 1] = Quat.Y;
		FloatBuffer[4 * i + 2] = Quat.Z;
		FloatBuffer[4 * i + 3] = Quat.W;
	}
}

int URelativeSkeletalPositionSensor::GetNumItems() {
	return this->Bones.Num();
}