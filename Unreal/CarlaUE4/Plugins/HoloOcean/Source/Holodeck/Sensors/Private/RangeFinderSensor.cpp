// MIT License (c) 2019 BYU PCCL see LICENSE file

#include "Holodeck.h"
#include "RangeFinderSensor.h"
#include "Json.h"

URangeFinderSensor::URangeFinderSensor() {
	SensorName = "RangeFinderSensor";
}

// Allows sensor parameters to be set programmatically from client.
void URangeFinderSensor::ParseSensorParms(FString ParmsJson) {
	Super::ParseSensorParms(ParmsJson);

	TSharedPtr<FJsonObject> JsonParsed;
	TSharedRef<TJsonReader<TCHAR>> JsonReader = TJsonReaderFactory<TCHAR>::Create(ParmsJson);
	if (FJsonSerializer::Deserialize(JsonReader, JsonParsed)) {

		if (JsonParsed->HasTypedField<EJson::Number>("LaserCount")) {
			LaserCount = JsonParsed->GetIntegerField("LaserCount");
		}

		if (JsonParsed->HasTypedField<EJson::Number>("LaserAngle")) {
			LaserAngle = -JsonParsed->GetIntegerField("LaserAngle");  // in client positive angles point up
		}

		if (JsonParsed->HasTypedField<EJson::Number>("LaserMaxDistance")) {
			LaserMaxDistance = JsonParsed->GetIntegerField("LaserMaxDistance") * 100;  // meters to centimeters
		}

		if (JsonParsed->HasTypedField<EJson::Boolean>("LaserDebug")) {
			LaserDebug = JsonParsed->GetBoolField("LaserDebug");
		}
	}
	else {
		UE_LOG(LogHolodeck, Fatal, TEXT("URangeFinderSensor::ParseSensorParms:: Unable to parse json."));
	}
}

void URangeFinderSensor::InitializeSensor() {
	Super::InitializeSensor();
	//You need to get the pointer to the object you are attached to. 
	Parent = this->GetAttachmentRootActor();
}

void URangeFinderSensor::TickSensorComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) {

	float* FloatBuffer = static_cast<float*>(Buffer);

	for (int i = 0; i < LaserCount; i++) {

		FVector start = GetComponentLocation();

		FVector end = GetForwardVector();
		FVector right = GetRightVector();
		end = end.RotateAngleAxis(-360 * i / LaserCount, GetUpVector());
		right = right.RotateAngleAxis(-360 * i / LaserCount, GetUpVector());
		end = end.RotateAngleAxis(LaserAngle, right);
		end = end * LaserMaxDistance;
		end = start + end;

		FCollisionQueryParams QueryParams = FCollisionQueryParams();
		QueryParams.AddIgnoredActor(Parent);

		FHitResult Hit = FHitResult();

		bool TraceResult = GetWorld()->LineTraceSingleByChannel(Hit, start, end, ECollisionChannel::ECC_Visibility, QueryParams);
		
		FloatBuffer[i] = (TraceResult ? Hit.Distance : LaserMaxDistance) / 100;  // centimeter to meters

		if (LaserDebug) {
			DrawDebugLine(GetWorld(), start, end, FColor::Green, false, .01, ECC_WorldStatic, 1.f);
		}
	}
}
