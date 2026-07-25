// MIT License (c) 2019 BYU PCCL see LICENSE file

#include "Holodeck.h"
#include "HolodeckGameMode.h"
#include "CleanUpTask.h"

void UCleanUpTask::InitializeSensor() {
	Super::InitializeSensor();
}

void UCleanUpTask::ParseSensorParms(FString ParmsJson) {
	Super::ParseSensorParms(ParmsJson);

	TSharedPtr<FJsonObject> JsonParsed;
	TSharedRef<TJsonReader<TCHAR>> JsonReader = TJsonReaderFactory<TCHAR>::Create(ParmsJson);

	bool HasConfiguration = false;
	int NumTrash = 5;
	bool UseTable = false;

	if (FJsonSerializer::Deserialize(JsonReader, JsonParsed)) {

		if (JsonParsed->HasTypedField<EJson::Number>("NumTrash")) {
			HasConfiguration = true;
			NumTrash = JsonParsed->GetNumberField("NumTrash");
		}

		if (JsonParsed->HasTypedField<EJson::Boolean>("UseTable")) {
			HasConfiguration = true;
			UseTable = JsonParsed->GetBoolField("UseTable");
		}

		// If it does not have a configuration block, it must be set via world command
		if (HasConfiguration) {
			TArray<float> nums;
			nums.Add(NumTrash);
			nums.Add(UseTable);
			TArray<FString> strs;
			AActor* Target = GetWorld()->GetAuthGameMode();
			AHolodeckGameMode* Game = static_cast<AHolodeckGameMode*>(Target);
			Game->ExecuteCustomCommand("CleanUpConfig", nums, strs);
		}
	}
	else {
		UE_LOG(LogHolodeck, Fatal, TEXT("UCleanUpTask::ParseSensorParms:: Unable to parse json."));
	}
}

void UCleanUpTask::TickSensorComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) {
	AActor* Target = GetWorld()->GetAuthGameMode();
	AHolodeckGameMode* Game = static_cast<AHolodeckGameMode*>(Target);
	if (TotalTrash == 0) {
		// By default the total trash is zero, so we must keep updating it until it is not zero
		TotalTrash = (int)Game->GetWorldNum("TotalTrash");
	}
	
	int TrashInCan = (int)Game->GetWorldNum("TrashInCan");
	// If trash is added, the reward will be positive and vice versa
	Reward = TrashInCan - PrevTick_TrashInCan;
	PrevTick_TrashInCan = TrashInCan;

	if (TrashInCan == TotalTrash && TotalTrash != 0) {
		Terminal = 1;
	}
	else {
		Terminal = 0;
	}


	UTaskSensor::TickSensorComponent(DeltaTime, TickType, ThisTickFunction);
}