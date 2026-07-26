// Written by joshgreaves.

#include "Holodeck.h"
#include "HolodeckSensor.h"

UHolodeckSensor::UHolodeckSensor() {
	PrimaryComponentTick.bCanEverTick = true;
	//Sensors should tick after physics is processed, so that the data
	//they collect is current. 
	PrimaryComponentTick.TickGroup = TG_PostPhysics;
	bOn = true;
}

void UHolodeckSensor::SetAgentAndController(AHolodeckPawnControllerInterface* ControllerParam, FString AgentNameParam) {
	Controller = ControllerParam;
	AgentName = AgentNameParam;
}

void UHolodeckSensor::InitializeSensor() {

	if (bOn && Controller != nullptr) {
		UE_LOG(LogTemp, Warning, TEXT("Getting buffer of size %d"), GetNumItems() * GetItemSize());
		Buffer = Controller->GetServer()->Malloc(UHolodeckServer::MakeKey(AgentName, SensorName + SensorDataKey), GetNumItems() * GetItemSize());
	} else {
		UE_LOG(LogTemp, Warning, TEXT("Getting Controller Failed. Sensor not "));
	}
}

void UHolodeckSensor::BeginPlay() {
	Super::BeginPlay();
}

void UHolodeckSensor::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) {
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (bOn && Buffer != nullptr)
		TickSensorComponent(DeltaTime, TickType, ThisTickFunction);
}
