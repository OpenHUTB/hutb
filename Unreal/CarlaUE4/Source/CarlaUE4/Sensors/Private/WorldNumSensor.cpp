// MIT License (c) 2019 BYU PCCL see LICENSE file

#include "Holodeck.h"
#include "WorldNumSensor.h"
#include "HolodeckGameMode.h"

UWorldNumSensor::UWorldNumSensor() {
	PrimaryComponentTick.bCanEverTick = true;
	SensorName = "WorldNumSensor";
}

void UWorldNumSensor::InitializeSensor() {
	Super::InitializeSensor();
}

void UWorldNumSensor::ParseSensorParms(FString ParmsJson) {
	Super::ParseSensorParms(ParmsJson);

	TSharedPtr<FJsonObject> JsonParsed;
	TSharedRef<TJsonReader<TCHAR>> JsonReader = TJsonReaderFactory<TCHAR>::Create(ParmsJson);
	if (FJsonSerializer::Deserialize(JsonReader, JsonParsed)) {

		if (JsonParsed->HasTypedField<EJson::String>("Key")) {
			Key = JsonParsed->GetStringField("Key");
		}
	}
	else {
		UE_LOG(LogHolodeck, Warning, TEXT("%s Unable to parse json."), *FString(__func__));
	}
}

void UWorldNumSensor::TickSensorComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) {
	//Check if the sensor is on and if it is retrieve value from the world state
	if (bOn && Key != "") {
		AActor* Target = GetWorld()->GetAuthGameMode();
		AHolodeckGameMode* Game = static_cast<AHolodeckGameMode*>(Target);
		int* IntBuffer = static_cast<int*>(Buffer);
		IntBuffer[0] = (int32)Game->GetWorldNum(Key);
	}
}
