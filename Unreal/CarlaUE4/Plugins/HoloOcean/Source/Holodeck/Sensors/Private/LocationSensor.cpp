#include "Holodeck.h"
#include "LocationSensor.h"

ULocationSensor::ULocationSensor() {
	PrimaryComponentTick.bCanEverTick = true;
	SensorName = "LocationSensor";
}

void ULocationSensor::ParseSensorParms(FString ParmsJson) {
	Super::ParseSensorParms(ParmsJson);

	TSharedPtr<FJsonObject> JsonParsed;
	TSharedRef<TJsonReader<TCHAR>> JsonReader = TJsonReaderFactory<TCHAR>::Create(ParmsJson);
	if (FJsonSerializer::Deserialize(JsonReader, JsonParsed)) {

		if (JsonParsed->HasTypedField<EJson::Number>("Sigma")) {
			mvn.initSigma(JsonParsed->GetNumberField("Sigma"));
		}
		if (JsonParsed->HasTypedField<EJson::Array>("Sigma")) {
			mvn.initSigma(JsonParsed->GetArrayField("Sigma"));
		}

		if (JsonParsed->HasTypedField<EJson::Number>("Cov")) {
			mvn.initCov(JsonParsed->GetNumberField("Cov"));
		}
		if (JsonParsed->HasTypedField<EJson::Array>("Cov")) {
			mvn.initCov(JsonParsed->GetArrayField("Cov"));
		}

	}
	else {
		UE_LOG(LogHolodeck, Fatal, TEXT("ULocationSensor::ParseSensorParms:: Unable to parse json."));
	}
}

void ULocationSensor::InitializeSensor() {
	Super::InitializeSensor();

	//You need to get the pointer to the object you are attached to. 
	Parent = this->GetAttachParent();
}

void ULocationSensor::TickSensorComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) {
	//check if your parent pointer is valid, and if the sensor is on. Then get the location and buffer, then send the location to the buffer. 
	if (Parent != nullptr && bOn) {
		FVector Location = this->GetComponentLocation();
		float* FloatBuffer = static_cast<float*>(Buffer);
		Location = ConvertLinearVector(Location, UEToClient);
		Location += mvn.sampleFVector();
		FloatBuffer[0] = Location.X;
		FloatBuffer[1] = Location.Y;
		FloatBuffer[2] = Location.Z;
	}
}
