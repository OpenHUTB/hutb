#include "Holodeck.h"
#include "Conversion.h"
#include "RotationSensor.h"

URotationSensor::URotationSensor() {
	PrimaryComponentTick.bCanEverTick = true;
	SensorName = "RotationSensor";
}

void URotationSensor::InitializeSensor() {
	Super::InitializeSensor();

	//You need to get the pointer to the object the sensor is attached to. 
	Parent = this->GetAttachmentRootActor();
}

void URotationSensor::TickSensorComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) {
	if (Parent != nullptr && bOn) {
		FRotator Rotation = this->GetComponentRotation();
		FVector EulerAngles = RotatorToRPY(Rotation);
		float* FloatBuffer = static_cast<float*>(Buffer);
		FloatBuffer[0] = EulerAngles.X;
		FloatBuffer[1] = EulerAngles.Y;
		FloatBuffer[2] = EulerAngles.Z;
	}
}
