#include "Holodeck.h"
#include "LocationTask.h"
#include "Json.h"

// Set default values
void ULocationTask::InitializeSensor() {
	Super::InitializeSensor();
}

// Allows sensor parameters to be set programmatically from client.
void ULocationTask::ParseSensorParms(FString ParmsJson) {
	Super::ParseSensorParms(ParmsJson);

	TSharedPtr<FJsonObject> JsonParsed;
	TSharedRef<TJsonReader<TCHAR>> JsonReader = TJsonReaderFactory<TCHAR>::Create(ParmsJson);
	if (FJsonSerializer::Deserialize(JsonReader, JsonParsed)) {

		if (JsonParsed->HasTypedField<EJson::String>("LocationActor")) {
			LocationActorTag = JsonParsed->GetStringField("LocationActor");
		}

		if (JsonParsed->HasTypedField<EJson::String>("GoalActor")) {
			GoalActorTag = JsonParsed->GetStringField("GoalActor");
		}

		if (JsonParsed->HasTypedField<EJson::Array>("GoalLocation")) {
			TArray<TSharedPtr<FJsonValue>> LocationArray = JsonParsed->GetArrayField("GoalLocation");
			if (LocationArray.Num() == 3) {
				double X, Y, Z;
				if (LocationArray[0]->TryGetNumber(X) && LocationArray[1]->TryGetNumber(Y) && LocationArray[2]->TryGetNumber(Z))
					GoalLocation = ConvertLinearVector(FVector(X, Y, Z), ClientToUE);
			}
		}

		if (JsonParsed->HasTypedField<EJson::Number>("GoalDistance")) {
			GoalDistance = ConvertClientDistanceToUnreal(JsonParsed->GetNumberField("GoalDistance"));
		}

		if (JsonParsed->HasTypedField<EJson::Boolean>("NegativeReward")) {
			NegativeReward = JsonParsed->GetBoolField("NegativeReward");
		}

		if (JsonParsed->HasTypedField<EJson::Boolean>("HasTerminal")) {
			HasTerminal = JsonParsed->GetBoolField("HasTerminal");
		}
	} else {
		UE_LOG(LogHolodeck, Warning, TEXT("ULocationTask::ParseSensorParms:: Unable to parse json."));
	}
}

// Called every frame
void ULocationTask::TickSensorComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) {
	if (LocationActor == nullptr && LocationActorTag != "")
		LocationActor = FindActorWithTag(LocationActorTag);
	if (GoalActor == nullptr && GoalActorTag != "")
		GoalActor = FindActorWithTag(GoalActorTag);

	if ((LocationActor || LocationActorTag == "") && (GoalActor || GoalActorTag == "")) {
		float Distance = CalcDistance();

		if (Distance < GoalDistance) {
			if (!NegativeReward) {
				Reward = 1;
			}
			else {
				Reward = -1;
			}

			if (HasTerminal)
				Terminal = true;

		}
		else {
			Reward = 0;
			Terminal = false;
		}
	}

	// Call TaskSensor's Tick to store Reward and Terminal in sensor buffer
	UTaskSensor::TickSensorComponent(DeltaTime, TickType, ThisTickFunction);
}

// Get Distance
float ULocationTask::CalcDistance() {
	FVector FromLocation = this->GetComponentLocation();
	if (LocationActor) {
		FromLocation = LocationActor->GetActorLocation();
	}

	FVector ToLocation = GoalLocation;
	if (GoalActor) {
		ToLocation = GoalActor->GetActorLocation();
	}

	return (ToLocation - FromLocation).Size();
}
