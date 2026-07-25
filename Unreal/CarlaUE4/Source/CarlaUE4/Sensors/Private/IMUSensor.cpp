// MIT License (c) 2019 BYU PCCL see LICENSE file

#include "Holodeck.h"
#include "IMUSensor.h"

UIMUSensor::UIMUSensor() {
	PrimaryComponentTick.bCanEverTick = true;
	SensorName = "IMUSensor";
}

void UIMUSensor::ParseSensorParms(FString ParmsJson) {
	Super::ParseSensorParms(ParmsJson);

	TSharedPtr<FJsonObject> JsonParsed;
	TSharedRef<TJsonReader<TCHAR>> JsonReader = TJsonReaderFactory<TCHAR>::Create(ParmsJson);
	if (FJsonSerializer::Deserialize(JsonReader, JsonParsed)) {

		if (JsonParsed->HasTypedField<EJson::Boolean>("ReturnBias")) {
			ReturnBias = JsonParsed->GetBoolField("ReturnBias");
		}

		// Acceleration noise
		if (JsonParsed->HasTypedField<EJson::Number>("AccelSigma")) {
			mvnAccel.initSigma(JsonParsed->GetNumberField("AccelSigma"));
		}
		if (JsonParsed->HasTypedField<EJson::Array>("AccelSigma")) {
			mvnAccel.initSigma(JsonParsed->GetArrayField("AccelSigma"));
		}
		if (JsonParsed->HasTypedField<EJson::Number>("AccelCov")) {
			mvnAccel.initCov(JsonParsed->GetNumberField("AccelCov"));
		}
		if (JsonParsed->HasTypedField<EJson::Array>("AccelCov")) {
			mvnAccel.initCov(JsonParsed->GetArrayField("AccelCov"));
		}

		// Angular Velocity noise
		if (JsonParsed->HasTypedField<EJson::Number>("AngVelSigma")) {
			mvnOmega.initSigma(JsonParsed->GetNumberField("AngVelSigma"));
		}
		if (JsonParsed->HasTypedField<EJson::Array>("AngVelSigma")) {
			mvnOmega.initSigma(JsonParsed->GetArrayField("AngVelSigma"));
		}
		if (JsonParsed->HasTypedField<EJson::Number>("AngVelCov")) {
			mvnOmega.initCov(JsonParsed->GetNumberField("AngVelCov"));
		}
		if (JsonParsed->HasTypedField<EJson::Array>("AngVelCov")) {
			mvnOmega.initCov(JsonParsed->GetArrayField("AngVelCov"));
		}

		// Acceleration Bias noise
		if (JsonParsed->HasTypedField<EJson::Number>("AccelBiasSigma")) {
			mvnBiasAccel.initSigma(JsonParsed->GetNumberField("AccelBiasSigma"));
		}
		if (JsonParsed->HasTypedField<EJson::Array>("AccelBiasSigma")) {
			mvnBiasAccel.initSigma(JsonParsed->GetArrayField("AccelBiasSigma"));
		}
		if (JsonParsed->HasTypedField<EJson::Number>("AccelBiasCov")) {
			mvnBiasAccel.initCov(JsonParsed->GetNumberField("AccelBiasCov"));
		}
		if (JsonParsed->HasTypedField<EJson::Array>("AccelBiasCov")) {
			mvnBiasAccel.initCov(JsonParsed->GetArrayField("AccelBiasCov"));
		}

		// Angular Velocity noise
		if (JsonParsed->HasTypedField<EJson::Number>("AngVelBiasSigma")) {
			mvnBiasOmega.initSigma(JsonParsed->GetNumberField("AngVelBiasSigma"));
		}
		if (JsonParsed->HasTypedField<EJson::Array>("AngVelBiasSigma")) {
			mvnBiasOmega.initSigma(JsonParsed->GetArrayField("AngVelBiasSigma"));
		}
		if (JsonParsed->HasTypedField<EJson::Number>("AngVelBiasCov")) {
			mvnBiasOmega.initCov(JsonParsed->GetNumberField("AngVelBiasCov"));
		}
		if (JsonParsed->HasTypedField<EJson::Array>("AngVelBiasCov")) {
			mvnBiasOmega.initCov(JsonParsed->GetArrayField("AngVelBiasCov"));
		}

	}
	else {
		UE_LOG(LogHolodeck, Fatal, TEXT("UIMUSensor::ParseSensorParms:: Unable to parse json."));
	}
}

void UIMUSensor::InitializeSensor() {
	Super::InitializeSensor();

	// Cache important variables
	Parent = Cast<UPrimitiveComponent>(this->GetAttachParent());

	World = Parent->GetWorld();
	WorldSettings = World->GetWorldSettings(false, false);
	WorldGravity = WorldSettings->GetGravityZ();

	VelocityThen = FVector();
	VelocityNow = FVector();
	LinearAccelerationVector = FVector();
	AngularVelocityVector = FVector();
}


void UIMUSensor::TickSensorComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction * ThisTickFunction) {
	if (Parent != nullptr && bOn) {
		CalculateAccelerationVector(DeltaTime);
		CalculateAngularVelocityVector();

		float* FloatBuffer = static_cast<float*>(Buffer);

		// Convert before sending to user side.
		LinearAccelerationVector = ConvertLinearVector(LinearAccelerationVector, UEToClient);
		AngularVelocityVector = ConvertAngularVector(AngularVelocityVector, NoScale);

		// Introduce noise
		BiasAccel += mvnBiasAccel.sampleFVector();
		BiasOmega += mvnBiasOmega.sampleFVector();
		LinearAccelerationVector += BiasAccel + mvnAccel.sampleFVector();
		AngularVelocityVector    += BiasOmega + mvnOmega.sampleFVector();

		FloatBuffer[0] = LinearAccelerationVector.X;
		FloatBuffer[1] = LinearAccelerationVector.Y;
		FloatBuffer[2] = LinearAccelerationVector.Z;
		FloatBuffer[3] = AngularVelocityVector.X;
		FloatBuffer[4] = AngularVelocityVector.Y;
		FloatBuffer[5] = AngularVelocityVector.Z;

		if(ReturnBias){
			FloatBuffer[6] = BiasAccel.X;
			FloatBuffer[7] = BiasAccel.Y;
			FloatBuffer[8] = BiasAccel.Z;
			FloatBuffer[9] = BiasOmega.X;
			FloatBuffer[10] = BiasOmega.Y;
			FloatBuffer[11] = BiasOmega.Z;
		}
	}
}

void UIMUSensor::CalculateAccelerationVector(float DeltaTime) {
	VelocityThen = VelocityNow;
	VelocityNow  = Parent->GetPhysicsLinearVelocityAtPoint(this->GetComponentLocation());

	RotationNow = this->GetComponentRotation();

	LinearAccelerationVector = VelocityNow - VelocityThen;
	LinearAccelerationVector /= DeltaTime;

	LinearAccelerationVector += FVector(0.0, 0.0, -WorldGravity);

	LinearAccelerationVector = RotationNow.UnrotateVector(LinearAccelerationVector); //changes world axis to local axis
}

void UIMUSensor::CalculateAngularVelocityVector() {
	AngularVelocityVector = Parent->GetPhysicsAngularVelocityInRadians();

	AngularVelocityVector.X = AngularVelocityVector.X;
	AngularVelocityVector.Y = AngularVelocityVector.Y;
	AngularVelocityVector.Z = AngularVelocityVector.Z;

	AngularVelocityVector = RotationNow.UnrotateVector(AngularVelocityVector); //Rotate from world angles to local angles.

}

FVector UIMUSensor::GetAccelerationVector() {
	return LinearAccelerationVector;
}

FVector UIMUSensor::GetAngularVelocityVector() {
	return AngularVelocityVector;
}
