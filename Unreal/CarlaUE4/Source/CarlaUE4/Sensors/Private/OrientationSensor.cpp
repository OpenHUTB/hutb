// MIT License (c) 2019 BYU PCCL see LICENSE file

#include "Holodeck.h"
#include "OrientationSensor.h"

UOrientationSensor::UOrientationSensor() {
	PrimaryComponentTick.bCanEverTick = true;
	SensorName = "OrientationSensor";
}

void UOrientationSensor::InitializeSensor() {
	Super::InitializeSensor();

	Controller = static_cast<AHolodeckPawnController*>(this->GetAttachmentRootActor()->GetInstigator()->Controller);
	Parent = static_cast<UPrimitiveComponent*>(this->GetAttachParent());
	RootMesh = static_cast<UStaticMeshComponent*>(this->GetAttachParent());

	World = Parent->GetWorld();
}

// Called every frame
void UOrientationSensor::TickSensorComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) {
	if (Parent != nullptr && RootMesh != nullptr) {
		FVector Forward = this->GetForwardVector();
		FVector Right = this->GetRightVector();
		FVector Up = this->GetUpVector();

		float* FloatBuffer = static_cast<float*>(Buffer);
		Forward = ConvertLinearVector(Forward, NoScale);
		FVector Left = ConvertLinearVector(-Right, NoScale);
		Up = ConvertLinearVector(Up, NoScale);

		FloatBuffer[0] = Forward.X;
		FloatBuffer[3] = Forward.Y;
		FloatBuffer[6] = Forward.Z;
		FloatBuffer[1] = Left.X;
		FloatBuffer[4] = Left.Y;
		FloatBuffer[7] = Left.Z;
		FloatBuffer[2] = Up.X;
		FloatBuffer[5] = Up.Y;
		FloatBuffer[8] = Up.Z;
	}
}
