#include "Holodeck.h"
#include "AvoidTask.h"
#include "Json.h"

// Set default values
void UAvoidTask::InitializeSensor() {
	Super::InitializeSensor();
}

// Allows sensor parameters to be set programmatically from client.
void UAvoidTask::ParseSensorParms(FString ParmsJson) {
	Super::ParseSensorParms(ParmsJson);

	TSharedPtr<FJsonObject> JsonParsed;
	TSharedRef<TJsonReader<TCHAR>> JsonReader = TJsonReaderFactory<TCHAR>::Create(ParmsJson);
	if (FJsonSerializer::Deserialize(JsonReader, JsonParsed)) {

		if (JsonParsed->HasTypedField<EJson::String>("ToAvoid")) {
			ToAvoidTag = JsonParsed->GetStringField("ToAvoid");
		}

		if (JsonParsed->HasTypedField<EJson::String>("StartSocket")) {
			StartSocket = JsonParsed->GetStringField("StartSocket");
		}

		if (JsonParsed->HasTypedField<EJson::String>("EndSocket")) {
			EndSocket = JsonParsed->GetStringField("EndSocket");
		}

		if (JsonParsed->HasTypedField<EJson::Boolean>("OnlyWithinSight")) {
			OnlyWithinSight = JsonParsed->GetBoolField("OnlyWithinSight");
		}

		if (JsonParsed->HasTypedField<EJson::Number>("FOVRadians")) {
			FOVRadians = JsonParsed->GetNumberField("FOVRadians");
		}

		if (JsonParsed->HasTypedField<EJson::Number>("MinDistance")) {
			MinDistance = JsonParsed->GetNumberField("MinDistance");
		}
	} else {
		UE_LOG(LogHolodeck, Warning, TEXT("UAvoidTask::ParseSensorParms:: Unable to parse json."));
	}
}

// Called every frame
void UAvoidTask::TickSensorComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) {
	if (ToAvoid == nullptr && ToAvoidTag != "")
		ToAvoid = FindActorWithTag(ToAvoidTag);

	if (ToAvoid) {
		// Get location and distance
		FVector StartVec = GetActorSocketLocation(ToAvoid, StartSocket); // Parent->GetActorLocation();
		FVector EndVec = GetActorSocketLocation(Parent, EndSocket);
		FVector DistanceVec = EndVec - StartVec;
		float Distance = DistanceVec.Size();
		
		if (OnlyWithinSight) {
			bool can_see = IsInSight(ToAvoid, Parent, StartVec, EndVec, FOVRadians, DistanceVec, Distance);
			
			if (can_see && Distance < MinDistance) {
				Reward = MaxScore * Distance / MinDistance;
			}
			else {
				Reward = 100;
			}
		}
		else {
			if (Distance < MinDistance) {
				Reward = MaxScore * Distance / MinDistance;
			}
			else {
				Reward = 100;
			}
		}
	}

	// Call TaskSensor's Tick to store Reward and Terminal
	UTaskSensor::TickSensorComponent(DeltaTime, TickType, ThisTickFunction);
}
