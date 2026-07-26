#include "Holodeck.h"
#include "HolodeckCamera.h"
#include "Json.h"

UHolodeckCamera::UHolodeckCamera() {
	UE_LOG(LogHolodeck, Log, TEXT("UHolodeckCamera::UHolodeckCamer() initialization called."));

}

// Allows sensor parameters to be set programmatically from client.
void UHolodeckCamera::ParseSensorParms(FString ParmsJson) {
	Super::ParseSensorParms(ParmsJson);

	TSharedPtr<FJsonObject> JsonParsed;
	TSharedRef<TJsonReader<TCHAR>> JsonReader = TJsonReaderFactory<TCHAR>::Create(ParmsJson);
	if (FJsonSerializer::Deserialize(JsonReader, JsonParsed)) {

		if (JsonParsed->HasTypedField<EJson::Number>("CaptureWidth")) {
			CaptureWidth = JsonParsed->GetIntegerField("CaptureWidth");
		}

		if (JsonParsed->HasTypedField<EJson::Number>("CaptureHeight")) {
			CaptureHeight = JsonParsed->GetIntegerField("CaptureHeight");
		}
	} else {
		UE_LOG(LogHolodeck, Fatal, TEXT("UHolodeckCamera::ParseSensorParms:: Unable to parse json."));
	}

	UE_LOG(LogHolodeck, Log, TEXT("CaptureHeight is %d"), CaptureHeight);
	UE_LOG(LogHolodeck, Log, TEXT("CaptureWidth is %d"), CaptureWidth);

}

void UHolodeckCamera::InitializeSensor() {
	UE_LOG(LogHolodeck, Log, TEXT("UHolodeckCamera::InitializeSensor"));
	Super::InitializeSensor();

	SceneCapture = NewObject<USceneCaptureComponent2D>(this, "SceneCap");
	SceneCapture->RegisterComponent();
	SceneCapture->AttachToComponent(this, FAttachmentTransformRules::KeepRelativeTransform);
	
	TargetTexture = NewObject<UTextureRenderTarget2D>(this);

	RenderRequest = FRenderRequest();
	
	//set up everything for the texture that you are using for output. These won't likely change for subclasses.
	//Note: the format should probably be 512 x 512 because it must be a square shape, and a power of two. If not, then the TargetTexture will cause crashes.
	TargetTexture->SRGB = false; //No alpha
	TargetTexture->CompressionSettings = TC_VectorDisplacementmap;
	TargetTexture->RenderTargetFormat = RTF_RGBA8;
	TargetTexture->InitCustomFormat(CaptureWidth, CaptureHeight, PF_FloatRGBA, false);

	//Handle whatever setup of the SceneCapture that won't likely change across different cameras. (These will be the defaults)
	SceneCapture->ProjectionType = ECameraProjectionMode::Perspective;
	SceneCapture->CompositeMode = SCCM_Overwrite;
	SceneCapture->FOVAngle = 90; //90 degrees for field of view.
	SceneCapture->CaptureSource = SCS_SceneColorHDR;
	SceneCapture->TextureTarget = TargetTexture;
	SceneCapture->PostProcessSettings.bOverride_AutoExposureBias = 1;

	// Higher = brighter captured image. Lower = darker
	// This is a magic number that has been fine tuned to the default worlds. Do not edit without thourough testing.
	SceneCapture->PostProcessSettings.AutoExposureBias = 4;

	//The buffer has got to be an FColor pointer so you can export the pixel data to it. 
	this->Buffer = static_cast<FColor*>(Super::Buffer);
	this->ViewportClient = Cast<UHolodeckViewportClient>(GEngine->GameViewport);
}

void UHolodeckCamera::TickSensorComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) {
	RenderRequest.RetrievePixels(Buffer, TargetTexture);
}