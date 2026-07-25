// MIT License (c) 2021 BYU FRoStLab see LICENSE file

#include "Holodeck.h"
#include "Benchmarker.h"
#include "HolodeckBuoyantAgent.h"
#include "SidescanSonar.h"
// #pragma warning (disable : 4101)

USidescanSonar::USidescanSonar() {
	SensorName = "SidescanSonar";
}

void USidescanSonar::BeginDestroy() {
	Super::BeginDestroy();

	delete[] count;
}

// Allows sensor parameters to be set programmatically from client.
void USidescanSonar::ParseSensorParms(FString ParmsJson) {

	// Override the Parent Class defaults for some key parameters
	Azimuth = 170; 			// degrees
	Elevation = 0.25;		// degrees
	RangeMin = 0.5 * 100; 	// 0.5 m (in cm)
	RangeMax = 35 * 100; 	// 35 m (in cm)

	// Parse any parent class parameters that were provided in the configuration
	Super::ParseSensorParms(ParmsJson);

	TSharedPtr<FJsonObject> JsonParsed;
	TSharedRef<TJsonReader<TCHAR>> JsonReader = TJsonReaderFactory<TCHAR>::Create(ParmsJson);
	if (FJsonSerializer::Deserialize(JsonReader, JsonParsed)) {

		// For handling noise
		if (JsonParsed->HasTypedField<EJson::Number>("AddSigma")) {
			addNoise.initSigma(JsonParsed->GetNumberField("AddSigma"));
		}
		if (JsonParsed->HasTypedField<EJson::Number>("AddCov")) {
			addNoise.initCov(JsonParsed->GetNumberField("AddCov"));
		}
		if (JsonParsed->HasTypedField<EJson::Number>("MultSigma")) {
			multNoise.initSigma(JsonParsed->GetNumberField("MultSigma"));
		}
		if (JsonParsed->HasTypedField<EJson::Number>("MultCov")) {
			multNoise.initCov(JsonParsed->GetNumberField("MultCov"));
		}

		if (JsonParsed->HasTypedField<EJson::Number>("RangeRes")) {
			RangeRes = JsonParsed->GetNumberField("RangeRes") * 100; // m to cm
		}

		if (JsonParsed->HasTypedField<EJson::Number>("ElevationRes")) {
			ElevationRes = JsonParsed->GetNumberField("ElevationRes"); // degrees
		}

		if (JsonParsed->HasTypedField<EJson::Number>("AzimuthRes")) {
			AzimuthRes = JsonParsed->GetNumberField("AzimuthRes"); // degrees
		}

		if (JsonParsed->HasTypedField<EJson::Number>("RangeBins")) {
			RangeBins = JsonParsed->GetIntegerField("RangeBins");
		}

		if (JsonParsed->HasTypedField<EJson::Number>("AzimuthBins")) {
			AzimuthBins = JsonParsed->GetIntegerField("AzimuthBins");
		}

		if (JsonParsed->HasTypedField<EJson::Number>("ElevationBins")) {
			ElevationBins = JsonParsed->GetIntegerField("ElevationBins");
		}
	}
	else {
		UE_LOG(LogHolodeck, Fatal, TEXT("USidescanSonar::ParseSensorParms:: Unable to parse json."));
	}

	// Parse through the Range parameters given to us
	if(RangeBins != 0){
		RangeRes = (RangeMax - RangeMin) / RangeBins;
	} 
	else if(RangeRes != 0){
		RangeBins = (RangeMax - RangeMin) / RangeRes;
	}
	else{
		RangeRes = 5; // cm
		RangeBins = (RangeMax - RangeMin) / RangeRes;
	}

	// Parse through the Azimuth parameters given to us
	if(AzimuthBins != 0){
		AzimuthRes = Azimuth / AzimuthBins;
	} 
	else if(AzimuthRes != 0){
		AzimuthBins = Azimuth / AzimuthRes;
	}
	else{
		// Calculate how large the azimuth bins should be
		AzimuthRes = (180 * Octree::OctreeMin) / (Pi * (RangeMin + 0.1 * (RangeMax - RangeMin)));
		AzimuthBins = Azimuth / AzimuthRes;
	}

	// Parse through the Elevation parameters given to us
	if(ElevationBins != 0){
		ElevationRes = Elevation / ElevationBins;
	} 
	else if(ElevationRes != 0){
		ElevationBins = Elevation / ElevationRes;
	}
	else{
		// Calculate how large our shadowing bins should be
		ElevationBins = (RangeMin*Elevation*Pi/180) / Octree::OctreeMin;
		if(ElevationBins < 1) ElevationBins = 1;
		ElevationRes = Elevation / ElevationBins;
	}
}

void USidescanSonar::InitializeSensor() {
	Super::InitializeSensor();
	
	// setup count of each bin
	count = new int32[RangeBins](); // Sidescan Sonar (1d array)

	for(int i=0;i<AzimuthBins*ElevationBins;i++){
		sortedLeaves.Add(TArray<Octree*>());
		sortedLeaves[i].Reserve(10000);
	}
}

// Conversion from Spherical coordinates to Euclidian
FVector spherToEucSS(float r, float theta, float phi, FTransform SensortoWorld){
	float x = r*UKismetMathLibrary::DegSin(phi)*UKismetMathLibrary::DegCos(theta);
	float y = r*UKismetMathLibrary::DegSin(phi)*UKismetMathLibrary::DegSin(theta);
	float z = r*UKismetMathLibrary::DegCos(phi);
	return UKismetMathLibrary::TransformLocation(SensortoWorld, FVector(x, y, z));
}


void USidescanSonar::TickSensorComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) {
	Super::TickSensorComponent(DeltaTime, TickType, ThisTickFunction);

	if(TickCounter == 0){
		// reset things and get ready
		float* result = static_cast<float*>(Buffer);
		std::fill(result, result+RangeBins, 0);
		std::fill(count, count+RangeBins, 0);
		
		for(auto& sl: sortedLeaves){
			sl.Reset();
		}

		// Finds leaves in range and puts them in foundLeaves
		findLeaves();		


		// SORT THEM INTO AZIMUTH/ELEVATION BINS
		int32 idx;
		for(TArray<Octree*>& bin : foundLeaves){
			for(Octree* l : bin){
				// Compute bins while we're parallelized
				l->idx.Y = (int32)((l->locSpherical.Y - minAzimuth)/ AzimuthRes);
				l->idx.Z = (int32)((l->locSpherical.Z - minElev)/ ElevationRes);
				// Sometimes we get float->int rounding errors
				if(l->idx.Y == AzimuthBins) --l->idx.Y;

				// UE_LOG(LogTemp, Warning, TEXT("Index Y: %d"), l->idx.Y);

				idx = l->idx.Z*AzimuthBins + l->idx.Y;
				sortedLeaves[idx].Emplace(l);
			}
		}


		// HANDLE SHADOWING
		shadowLeaves();


		// ADD IN ALL CONTRIBUTIONS
		// Reuse idx variable from above
		for(TArray<Octree*>& bin : sortedLeaves){
			for(Octree* l : bin){
				// Calculate range bin
				l->idx.X = (int32)((l->locSpherical.X - RangeMin) / RangeRes);

				// Add to their appropriate bin
				if (l->idx.Y > (AzimuthBins / 2)){
					idx = RangeBins / 2 - l->idx.X / 2 - 1;
				}
				else{
					idx = RangeBins / 2 + l->idx.X / 2;
				}

				result[idx] += l->val;
				++count[idx];
			}
		}


		// NORMALIZE THE BUFFER
		for (int i = 0; i < RangeBins; i++) {
			if(count[i] != 0){
				result[i] *= (1 + multNoise.sampleFloat()) / count[i];
				result[i] += addNoise.sampleRayleigh();
			}
			else{
				result[i] = addNoise.sampleRayleigh();
			}
		}
	}

	if (runtickCounter == 20 && (RangeMin*Elevation*Pi/180) / Octree::OctreeMin < 1)
	{
		float recommendedElevation = Octree::OctreeMin * 180 / (RangeMin * Pi);
		float recommendedOctreeMin = RangeMin * Elevation * Pi / 180 / 100;
		GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Red, FString::Printf(TEXT("WARNING: Elevation angle potentially too small with current OctreeMin configuration\n Recommended changes (pick one):\n Elevation = %f\n OctreeMin = %f\n"), recommendedElevation, recommendedOctreeMin));
	}

	runtickCounter++;
}
