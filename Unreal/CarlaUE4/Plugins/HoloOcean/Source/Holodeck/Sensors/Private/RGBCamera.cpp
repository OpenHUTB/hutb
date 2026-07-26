#include "Holodeck.h"
#include "RGBCamera.h"
#include "Json.h"

URGBCamera::URGBCamera() {
	SensorName = "RGBCamera";
}

// Allows sensor parameters to be set programmatically from client.
void URGBCamera::ParseSensorParms(FString ParmsJson) {
	Super::ParseSensorParms(ParmsJson);

	TSharedPtr<FJsonObject> JsonParsed;
	TSharedRef<TJsonReader<TCHAR>> JsonReader = TJsonReaderFactory<TCHAR>::Create(ParmsJson);
	if (FJsonSerializer::Deserialize(JsonReader, JsonParsed)) {
		
		if (JsonParsed->HasTypedField<EJson::Number>("TicksPerCapture")) {
			TicksPerCapture = JsonParsed->GetIntegerField("TicksPerCapture");
		}
	} else {
		UE_LOG(LogHolodeck, Fatal, TEXT("URGBCamera::ParseSensorParms:: Unable to parse json."));
	}
}

void URGBCamera::InitializeSensor() {
	Super::InitializeSensor();

	//Set up everything for the scenecapturecomponent2d
	SceneCapture->CaptureSource = SCS_FinalColorLDR; //Pick what type of output you want to be sent to the texture target. 	
}

void URGBCamera::TickSensorComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) {

	TickCounter++;
	if (TickCounter == TicksPerCapture) {
		RenderRequest.RetrievePixels(Buffer, TargetTexture);
		TickCounter = 0;
	}
}
