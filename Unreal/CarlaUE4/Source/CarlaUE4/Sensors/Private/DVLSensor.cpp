// MIT License (c) 2021 BYU FRoStLab see LICENSE file

#include "Holodeck.h"
#include "DVLSensor.h"

UDVLSensor::UDVLSensor() {
	PrimaryComponentTick.bCanEverTick = true;
	SensorName = "DVLSensor";
}

void UDVLSensor::ParseSensorParms(FString ParmsJson) {
	Super::ParseSensorParms(ParmsJson);

	TSharedPtr<FJsonObject> JsonParsed;
	TSharedRef<TJsonReader<TCHAR>> JsonReader = TJsonReaderFactory<TCHAR>::Create(ParmsJson);
	if (FJsonSerializer::Deserialize(JsonReader, JsonParsed)) {

		if (JsonParsed->HasTypedField<EJson::Number>("Elevation")) {
			elevation = JsonParsed->GetNumberField("Elevation");
		}
		if (JsonParsed->HasTypedField<EJson::Boolean>("ReturnRange")) {
			ReturnRange = JsonParsed->GetBoolField("ReturnRange");
		}
		if (JsonParsed->HasTypedField<EJson::Number>("MaxRange")) {
			MaxRange = JsonParsed->GetNumberField("MaxRange")*100;
		}
		if (JsonParsed->HasTypedField<EJson::Boolean>("DebugLines")) {
			DebugLines = JsonParsed->GetBoolField("DebugLines");
		}

		// For handling noise
		if (JsonParsed->HasTypedField<EJson::Number>("VelSigma")) {
			mvnVel.initSigma(JsonParsed->GetNumberField("VelSigma"));
		}
		if (JsonParsed->HasTypedField<EJson::Array>("VelSigma")) {
			mvnVel.initSigma(JsonParsed->GetArrayField("VelSigma"));
		}
		if (JsonParsed->HasTypedField<EJson::Number>("VelCov")) {
			mvnVel.initCov(JsonParsed->GetNumberField("VelCov"));
		}
		if (JsonParsed->HasTypedField<EJson::Array>("VelCov")) {
			mvnVel.initCov(JsonParsed->GetArrayField("VelCov"));
		}

		if (JsonParsed->HasTypedField<EJson::Number>("RangeSigma")) {
			mvnRange.initSigma(JsonParsed->GetNumberField("RangeSigma"));
		}
		if (JsonParsed->HasTypedField<EJson::Array>("RangeSigma")) {
			mvnRange.initSigma(JsonParsed->GetArrayField("RangeSigma"));
		}
		if (JsonParsed->HasTypedField<EJson::Number>("RangeCov")) {
			mvnRange.initCov(JsonParsed->GetNumberField("RangeCov"));
		}
		if (JsonParsed->HasTypedField<EJson::Array>("RangeCov")) {
			mvnRange.initCov(JsonParsed->GetArrayField("RangeCov"));
		}

	}
	else {
		UE_LOG(LogHolodeck, Fatal, TEXT("UDVLSensor::ParseSensorParms:: Unable to parse json."));
	}
}

void UDVLSensor::InitializeSensor() {
	Super::InitializeSensor();

	//You need to get the pointer to the object the sensor is attached to. 
	Parent = Cast<UPrimitiveComponent>(this->GetAttachParent());

	sinElev = UKismetMathLibrary::DegSin(elevation);
	cosElev = UKismetMathLibrary::DegCos(elevation);

	// make transformation matrix
	// See https://etda.libraries.psu.edu/files/final_submissions/17327
	transform = {   {1/(2*sinElev),             0, -1/(2*sinElev),             0},
					{            0, 1/(2*sinElev),              0, -1/(2*sinElev)},
					{1/(4*cosElev), 1/(4*cosElev),  1/(4*cosElev),  1/(4*cosElev)} };

	// make direction lines
	directions = {FVector(sinElev, 0, -cosElev),
				FVector(0, -sinElev, -cosElev),
				FVector(-sinElev, 0, -cosElev),
				FVector(0, sinElev, -cosElev)};
	// scale up to point MaxRange distance
	for(FVector& d : directions) d *= MaxRange;
}

void UDVLSensor::TickSensorComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) {
	//check if your parent pointer is valid, and if the sensor is on. Then get the velocity and buffer, then send the data to it. 
	if (Parent != nullptr && bOn) {
		float* FloatBuffer = static_cast<float*>(Buffer);
		FTransform SensortoWorld = this->GetComponentTransform();
		FVector Location = this->GetComponentLocation();

		//UE4 gives us world velocity, rotate it to get local velocity
		FVector Velocity = Parent->GetPhysicsLinearVelocityAtPoint(this->GetComponentLocation());
		Velocity = SensortoWorld.GetRotation().UnrotateVector(Velocity);
		Velocity = ConvertLinearVector(Velocity, UEToClient);

		// Add noise if it's been enabled
		if(mvnVel.isUncertain()){
			TArray<float> sample = mvnVel.sampleTArray();
			for(int i=0;i<4;i++){
				Velocity.X += transform[0][i]*sample[i];
				Velocity.Y += transform[1][i]*sample[i];
				Velocity.Z += transform[2][i]*sample[i];

			}
		}

		// Send to buffer
		FloatBuffer[0] = Velocity.X;
		FloatBuffer[1] = Velocity.Y;
		FloatBuffer[2] = Velocity.Z;



		// Get range if it was requested
		if(ReturnRange){
			// Get parameters we'll need
			FCollisionQueryParams QueryParams = FCollisionQueryParams();
			QueryParams.AddIgnoredActor(this->GetAttachmentRootActor());

			// iterate through and do all raytracing
			for(int i=0;i<4;i++){
				FVector end = SensortoWorld.TransformPositionNoScale(directions[i]);
				FHitResult Hit = FHitResult();
				bool TraceResult = GetWorld()->LineTraceSingleByChannel(Hit, Location, end, ECollisionChannel::ECC_Visibility, QueryParams);
				FloatBuffer[i+3] = (TraceResult ? Hit.Distance : MaxRange) / 100;  // centimeter to meters
			}

			// Add noise if it's been enabled
			if(mvnRange.isUncertain()){
				TArray<float> sample = mvnRange.sampleTArray();
				for(int i=0;i<4;i++){
					FloatBuffer[i+3] += sample[i];
				}
			} 
		}



		// display debug lines if we want
		if(DebugLines){
			for(int i=0;i<4;i++){
				FVector end = SensortoWorld.TransformPositionNoScale(directions[i]);
				DrawDebugLine(GetWorld(), Location, end, FColor::Green, false, .01, ECC_WorldStatic, 1.f);
			}
		}
	}
}
