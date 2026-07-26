// MIT License (c) 2021 BYU FRoStLab see LICENSE file

#include "Holodeck.h"
#include "DepthSensor.h"

UDepthSensor::UDepthSensor() {
	PrimaryComponentTick.bCanEverTick = true;
	SensorName = "DepthSensor";
}

void UDepthSensor::ParseSensorParms(FString ParmsJson) {
	Super::ParseSensorParms(ParmsJson);

	TSharedPtr<FJsonObject> JsonParsed;
	TSharedRef<TJsonReader<TCHAR>> JsonReader = TJsonReaderFactory<TCHAR>::Create(ParmsJson);
	if (FJsonSerializer::Deserialize(JsonReader, JsonParsed)) {

		if (JsonParsed->HasTypedField<EJson::Number>("Sigma")) {
			mvn.initSigma(JsonParsed->GetNumberField("Sigma"));
		}
		if (JsonParsed->HasTypedField<EJson::Number>("Cov")) {
			mvn.initCov(JsonParsed->GetNumberField("Cov"));
		}
		
	}
	else {
		UE_LOG(LogHolodeck, Fatal, TEXT("UDepthSensor::ParseSensorParms:: Unable to parse json."));
	}
}

void UDepthSensor::InitializeSensor() {
	Super::InitializeSensor();

	//You need to get the pointer to the object you are attached to. 
	Parent = this->GetAttachParent();
}

void UDepthSensor::TickSensorComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) {
	//check if your parent pointer is valid, and if the sensor is on. Then get the location and buffer, then send the location to the buffer. 
	if (Parent != nullptr && bOn) {
		float* FloatBuffer = static_cast<float*>(Buffer);
		
		FVector Location = this->GetComponentLocation();
		Location = ConvertLinearVector(Location, UEToClient);
		
		Location.Z += mvn.sampleFloat();
		FloatBuffer[0] = Location.Z;
	}
}
