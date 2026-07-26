// MIT License (c) 2019 BYU PCCL see LICENSE file

#include "Holodeck.h"
#include "AbuseSensor.h"

UAbuseSensor::UAbuseSensor() {
	PrimaryComponentTick.bCanEverTick = true;
	SensorName = "AbuseSensor";

	Agent = (AHolodeckAgent*)this->GetOwner();
}

void UAbuseSensor::InitializeSensor() {
	Super::InitializeSensor();
}

void UAbuseSensor::ParseSensorParms(FString ParmsJson) {
	Super::ParseSensorParms(ParmsJson);

	TSharedPtr<FJsonObject> JsonParsed;
	TSharedRef<TJsonReader<TCHAR>> JsonReader = TJsonReaderFactory<TCHAR>::Create(ParmsJson);

	this->AccelerationLimit = Agent->GetAccelerationLimit();

	if (FJsonSerializer::Deserialize(JsonReader, JsonParsed)) {

		if (JsonParsed->HasTypedField<EJson::Number>("AccelerationLimit")) {
			this->AccelerationLimit = JsonParsed->GetNumberField("AccelerationLimit");
		}
	}
	else {
		UE_LOG(LogHolodeck, Warning, TEXT("%s Unable to parse json."), *FString(__func__));
	}
}

void UAbuseSensor::TickSensorComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) {
	if (Agent != nullptr && bOn) {
		int32 Abused = (int32)Agent->IsAbused;

		if (!Abused && AccelerationLimit != -1) {
			FVector CurrentSpeed = Agent->GetVelocity();
			
			// a = (delta v / delta t)
			float Acceleration = (CurrentSpeed - PrevSpeed).Size() / DeltaTime;
			Acceleration /= 100; // Convert cm/s2 to m/s2

			if (Acceleration > AccelerationLimit) {
				Abused = 1;
			}

			PrevSpeed = CurrentSpeed;
		}

		int* IntBuffer = static_cast<int*>(Buffer);
		IntBuffer[0] = Abused;

		// IsAbused must be reset in case the agent leaves its abusive state
		Agent->IsAbused = false;
	}
}