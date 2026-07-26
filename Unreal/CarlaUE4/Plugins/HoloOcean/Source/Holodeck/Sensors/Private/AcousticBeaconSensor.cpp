// MIT License (c) 2021 BYU FRoStLab see LICENSE file

#include "Holodeck.h"
#include "AcousticBeaconSensor.h"

UAcousticBeaconSensor::UAcousticBeaconSensor() {
	PrimaryComponentTick.bCanEverTick = true;
	SensorName = "AcousticBeaconSensor";
}

void UAcousticBeaconSensor::InitializeSensor() {
	Super::InitializeSensor();

	//You need to get the pointer to the object the sensor is attached to. 
	Parent = Cast<UPrimitiveComponent>(this->GetAttachParent());
}

void UAcousticBeaconSensor::TickSensorComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) {
	//check if your parent pointer is valid, and if the sensor is on. Then get the velocity and buffer, then send the data to it. 
	if (Parent != nullptr && bOn) {
		// if someone starting transmitting
		if (fromSensor){
			// get coordinates of other sensor in local frame
			FTransform SensortoWorld = this->GetComponentTransform();
			FVector fromLocation = fromSensor->GetComponentLocation();
			FVector fromLocationLocal = UKismetMathLibrary::InverseTransformLocation(SensortoWorld, fromLocation);
			fromLocationLocal = ConvertLinearVector(fromLocationLocal, UEToClient);

			// calculate angles
			WaitBuffer[0] = UKismetMathLibrary::Atan2(fromLocationLocal.Y, fromLocationLocal.X);
			float dist = UKismetMathLibrary::Sqrt( FMath::Square(fromLocationLocal.X) + FMath::Square(fromLocationLocal.Y) );
			WaitBuffer[1] = UKismetMathLibrary::Atan2(fromLocationLocal.Z, dist);

			// distance away (already converted)
			WaitBuffer[2] = fromLocationLocal.Size();

			// Z Value
			FVector toLocation = this->GetComponentLocation();
			WaitBuffer[3] = ConvertUnrealDistanceToClient( toLocation.Z );

			// empty the sensor we got it from
			fromSensor = NULL;

			// figure out how long to receive that message
			WaitTicks = roundl(WaitBuffer[2] / (DeltaTime * SpeedOfSound));
		}
		// if we're waiting for message
		if(WaitTicks > 0){
			WaitTicks--;
		}
		// if the message was received
		else if(WaitTicks == 0){
			float* FloatBuffer = static_cast<float*>(Buffer);
			for(int i=0;i<4;i++)
				FloatBuffer[i] = WaitBuffer[i];

			WaitTicks = -1;
		}
		// remove message after it was received
		else if(WaitTicks == -1){
			float* FloatBuffer = static_cast<float*>(Buffer);
			for(int i=0;i<4;i++)
				FloatBuffer[i] = std::numeric_limits<double>::quiet_NaN();

			WaitTicks = -2;
		}
	}
}
