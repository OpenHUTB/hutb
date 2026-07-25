// MIT License (c) 2021 BYU FRoStLab see LICENSE file

#include "Holodeck.h"
#include "ProfilingSonar.h"
// #pragma warning (disable : 4101)

UProfilingSonar::UProfilingSonar() {
	SensorName = "ProfilingSonar";
}

// Allows sensor parameters to be set programmatically from client.
void UProfilingSonar::ParseSensorParms(FString ParmsJson) {
	Elevation = 1;
	RangeMin = 0.5 * 100;
	RangeMax = 75 * 100;

	TSharedPtr<FJsonObject> JsonParsed;
	TSharedRef<TJsonReader<TCHAR>> JsonReader = TJsonReaderFactory<TCHAR>::Create(ParmsJson);
	if (FJsonSerializer::Deserialize(JsonReader, JsonParsed)) {
		// If they haven't set RangeRes or RangeBins, input our default RangeBins
		if (!JsonParsed->HasTypedField<EJson::Number>("RangeRes") && !JsonParsed->HasTypedField<EJson::Number>("RangeBins")) {
			RangeBins = 750;
		}

		// If they haven't set AzimuthRes or AzimuthBins, input our default AzimuthBins
		if (!JsonParsed->HasTypedField<EJson::Number>("AzimuthRes") && !JsonParsed->HasTypedField<EJson::Number>("AzimuthBins")) {
			AzimuthBins = 480;
		}
	}

	Super::ParseSensorParms(ParmsJson);
}
