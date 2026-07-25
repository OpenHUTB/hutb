#include "Holodeck.h"
#include "CollisionSensor.h"

UCollisionSensor::UCollisionSensor() {
	PrimaryComponentTick.bCanEverTick = true;
	bWantsInitializeComponent = true;
	SensorName = "CollisionSensor";
}

void UCollisionSensor::InitializeComponent() {
	Super::InitializeComponent();

	Parent = this->GetOwner();
	//Set up the hit delegate, then give it to the parent. The parent will then call OnHit whenever it collides. 
	FScriptDelegate HitDelegate;
	HitDelegate.BindUFunction(this, TEXT("OnHit"));
	if (Parent) {
		Parent->OnActorHit.AddUnique(HitDelegate);
	}
	else {
		UE_LOG(LogHolodeck, Fatal, TEXT("UCollisionSensor::Parent was never initialized. Cannot add HitDelegate"));
	}
}

void UCollisionSensor::TickSensorComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) {
	//Update the buffer to current collision status, then set the colliding bool to false. 
	if (Parent != nullptr && bOn) {
		bool* BoolBuffer = static_cast<bool*>(Buffer);
		BoolBuffer[0] = bIsColliding;
	}
	bIsColliding = false;
}

void UCollisionSensor::OnHit(AActor* SelfActor, AActor* OtherActor, FVector NormalImpulse, const FHitResult& Hit) {
	bIsColliding = true;
}
