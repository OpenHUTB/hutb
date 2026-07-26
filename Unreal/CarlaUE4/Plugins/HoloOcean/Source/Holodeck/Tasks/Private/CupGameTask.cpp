// MIT License (c) 2019 BYU PCCL see LICENSE file

#include "Holodeck.h"
#include "HolodeckGameMode.h"
#include "CupGameTask.h"


void UCupGameTask::InitializeSensor() {
	Super::InitializeSensor();
}

// Allows sensor parameters to be set programmatically from client.
void UCupGameTask::ParseSensorParms(FString ParmsJson) {
	Super::ParseSensorParms(ParmsJson);

	TSharedPtr<FJsonObject> JsonParsed;
	TSharedRef<TJsonReader<TCHAR>> JsonReader = TJsonReaderFactory<TCHAR>::Create(ParmsJson);

	bool HasConfiguration = false;
	int32 Speed = 2;
	int32 NumShuffles = 3;
	bool UseSeed = false;
	int32 Seed = 0;

	if (FJsonSerializer::Deserialize(JsonReader, JsonParsed)) {

		if (JsonParsed->HasTypedField<EJson::Number>("Speed")) {
			HasConfiguration = true;
			Speed = JsonParsed->GetNumberField("Speed");
		}

		if (JsonParsed->HasTypedField<EJson::Number>("NumShuffles")) {
			HasConfiguration = true;
			NumShuffles = JsonParsed->GetNumberField("NumShuffles");
		}

		if (JsonParsed->HasTypedField<EJson::Number>("Seed")) {
			HasConfiguration = true;
			UseSeed = true;
			Seed = JsonParsed->GetNumberField("Seed");
		}

		if (HasConfiguration){
			TArray<float> nums;
			nums.Add(Speed);
			nums.Add(NumShuffles);
			nums.Add(UseSeed);
			nums.Add(Seed);
			TArray<FString> strs;
			strs.Add(AgentName);
			AActor* Target = GetWorld()->GetAuthGameMode();
			AHolodeckGameMode* Game = static_cast<AHolodeckGameMode*>(Target);
			Game->ExecuteCustomCommand("CupGameConfig", nums, strs);
			Game->ExecuteCustomCommand("StartCupGame", nums, strs);
		}
	}
	else {
		UE_LOG(LogHolodeck, Fatal, TEXT("UCupGameTask::ParseSensorParms:: Unable to parse json."));
	}
}

void UCupGameTask::TickSensorComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) {
	AActor* Target = GetWorld()->GetAuthGameMode();
	AHolodeckGameMode* Game = static_cast<AHolodeckGameMode*>(Target);
	bool BallTouched = Game->GetWorldBool("BallTouched");
	bool CorrectCupTouched = Game->GetWorldBool("CorrectCupTouched");
	bool WrongCupTouched = Game->GetWorldBool("WrongCupTouched");

	if (BallTouched && !WrongCupTouched) {
		Reward = 2;
		Terminal = 1;
	}
	else if (CorrectCupTouched && !WrongCupTouched && !MinRewardGiven){
		Reward = 1;
		MinRewardGiven = true; // Only give the min reward once
	}
	else if (WrongCupTouched) {
		Reward = -1;
		Terminal = 1;
	}
	else {
		Reward = 0;
	}

	UTaskSensor::TickSensorComponent(DeltaTime, TickType, ThisTickFunction);
}
