#include "Holodeck.h"
#include "FollowTask.h"
#include "Json.h"

// Set default values
void UFollowTask::InitializeSensor() {
	Super::InitializeSensor();
}

// Allows sensor parameters to be set programmatically from client.
void UFollowTask::ParseSensorParms(FString ParmsJson) {
	Super::ParseSensorParms(ParmsJson);

	TSharedPtr<FJsonObject> JsonParsed;
	TSharedRef<TJsonReader<TCHAR>> JsonReader = TJsonReaderFactory<TCHAR>::Create(ParmsJson);
	if (FJsonSerializer::Deserialize(JsonReader, JsonParsed)) {

		if (JsonParsed->HasTypedField<EJson::String>("ToFollow")) {
			ToFollowTag = JsonParsed->GetStringField("ToFollow");
		}

		if (JsonParsed->HasTypedField<EJson::String>("FollowSocket")) {
			FollowSocket = JsonParsed->GetStringField("FollowSocket");
		}

		if (JsonParsed->HasTypedField<EJson::Boolean>("OnlyWithinSight")) {
			OnlyWithinSight = JsonParsed->GetBoolField("OnlyWithinSight");
		}

		if (JsonParsed->HasTypedField<EJson::Number>("FOVRadians")) {
			FOVRadians = JsonParsed->GetNumberField("FOVRadians");
		}

		if (JsonParsed->HasTypedField<EJson::Number>("MinDistance")) {
			MinDistance = ConvertClientDistanceToUnreal(JsonParsed->GetNumberField("MinDistance"));
		}
	}
}

// Called every frame
void UFollowTask::TickSensorComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) {
	if (ToFollow == nullptr && ToFollowTag != "")
		ToFollow = FindActorWithTag(ToFollowTag);

	if (ToFollow) {
		// Get location and distance
		FVector StartVec = GetActorSocketLocation(Parent, "CameraSocket");
		FVector EndVec = GetActorSocketLocation(ToFollow, FollowSocket);
		FVector DistanceVec = EndVec - StartVec;
		float Distance = DistanceVec.Size();

		if (OnlyWithinSight) {
			bool can_see = IsInSight(Parent, ToFollow, StartVec, EndVec, FOVRadians, DistanceVec, Distance);
			
			if (can_see && Distance < MinDistance) {
				Reward = MaxScore * (MinDistance - Distance) / MinDistance;
			} else {
				Reward = 0;
			}
		}
		else {
			if (Distance < MinDistance) {
				Reward = MaxScore * (MinDistance - Distance) / MinDistance;
			} else {
				Reward = 0;
			}
		}
	}

	// Call TaskSensor's Tick to store Reward and Terminal
	UTaskSensor::TickSensorComponent(DeltaTime, TickType, ThisTickFunction);
}
