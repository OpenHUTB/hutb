#include "Holodeck.h" 

#include "TaskSensor.h"
#include "HolodeckGameMode.h"

// Sets default values
UTaskSensor::UTaskSensor() {
	PrimaryComponentTick.bCanEverTick = true;
	SensorName = "TaskSensor";
}

// Initialize and get Parent
void UTaskSensor::InitializeSensor() {
	Super::InitializeSensor();
	//You need to get the pointer to the object you are attached to. 
	Parent = this->GetAttachmentRootActor();
}

// Called every frame
// Sets Reward and Terminal in sensor buffer
// Must be called at the end of child tasks' Tick method
void UTaskSensor::TickSensorComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) {
	if (Parent != nullptr && bOn) {
		float* FloatBuffer = static_cast<float*>(Buffer);
		FloatBuffer[0] = Reward;
		FloatBuffer[1] = Terminal;
	}
}

AActor* UTaskSensor::FindActorWithTag(FString tag) {
	AHolodeckGameMode* GameTarget = (AHolodeckGameMode*)GetWorld()->GetAuthGameMode();
	return GameTarget->FindActorWithTag(tag);
}
