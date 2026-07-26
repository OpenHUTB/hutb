// MIT License (c) 2019 BYU PCCL see LICENSE file

#include "Holodeck.h"
#include "ViewportCapture.h"
#include "Json.h"

// Sets default values for this component's properties
UViewportCapture::UViewportCapture(){
	PrimaryComponentTick.bCanEverTick = true;
	SensorName = "ViewportCapture";
}

// Allows sensor parameters to be set programmatically from client.
void UViewportCapture::ParseSensorParms(FString ParmsJson) {
	Super::ParseSensorParms(ParmsJson);

	TSharedPtr<FJsonObject> JsonParsed;
	TSharedRef<TJsonReader<TCHAR>> JsonReader = TJsonReaderFactory<TCHAR>::Create(ParmsJson);
	if (FJsonSerializer::Deserialize(JsonReader, JsonParsed)) {

		if (JsonParsed->HasTypedField<EJson::Boolean>("bGrayScale")) {
			bGrayScale = JsonParsed->GetBoolField("bGrayScale");
		}

		if (JsonParsed->HasTypedField<EJson::Number>("Width")) {
			Width = JsonParsed->GetIntegerField("Width");
		}

		if (JsonParsed->HasTypedField<EJson::Number>("Height")) {
			Height = JsonParsed->GetIntegerField("Height");
		}
	} else {
		UE_LOG(LogHolodeck, Fatal, TEXT("URGBCamera::ParseSensorParms:: Unable to parse json."));
	}
}

void UViewportCapture::InitializeSensor() {
	UE_LOG(LogHolodeck, Log, TEXT("UViewportCapture::InitializeSensor"));

	// This must come first, since the HolodeckSensor parent class will
	// call GetNumItems, which needs the ViewportClient.
	ViewportClient = Cast<UHolodeckViewportClient>(GEngine->GameViewport);
	if (ViewportClient && bOn) {
		const FVector2D ViewportSize = FVector2D(ViewportClient->Viewport->GetSizeXY());
		Width = ViewportSize.X;
		Height = ViewportSize.Y;
	}

	// Buffers must be set after width and height are retrieved.
	Super::InitializeSensor();
	ViewportClient->SetBuffer(Buffer);
}

void UViewportCapture::TickSensorComponent(float DeltaTime,
														   ELevelTick TickType,
														   FActorComponentTickFunction* ThisTickFunction) {
	// The pixel data is captured on the rendering thread.
	// All this class needs to do is set the buffer at begin play.
	if (!ViewportClient->BufferIsSet())
		ViewportClient->SetBuffer(Buffer);
}
