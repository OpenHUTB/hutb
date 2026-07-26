// MIT License (c) 2021 BYU FRoStLab see LICENSE file

#include "Holodeck.h"
#include "Benchmarker.h"
#include "HolodeckBuoyantAgent.h"
#include "ImagingSonar.h"
// #pragma warning (disable : 4101)

UImagingSonar::UImagingSonar() {
	SensorName = "ImagingSonar";
}

void UImagingSonar::BeginDestroy() {
	Super::BeginDestroy();

	delete[] count;
	delete[] hasPerfectNormal;
}

// Allows sensor parameters to be set programmatically from client.
void UImagingSonar::ParseSensorParms(FString ParmsJson) {
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
		if (JsonParsed->HasTypedField<EJson::Number>("RangeSigma")) {
			rNoise.initBounds(JsonParsed->GetNumberField("RangeSigma")*100);
		}
		if (JsonParsed->HasTypedField<EJson::Boolean>("ScaleNoise")) {
			ScaleNoise = JsonParsed->GetBoolField("ScaleNoise");
		}
		if (JsonParsed->HasTypedField<EJson::Number>("AzimuthStreaks")) {
			AzimuthStreaks = JsonParsed->GetIntegerField("AzimuthStreaks");
		}

		// Multipath Settings
		if (JsonParsed->HasTypedField<EJson::Boolean>("MultiPath")) {
			MultiPath = JsonParsed->GetBoolField("MultiPath");
		}
		if (JsonParsed->HasTypedField<EJson::Number>("ClusterSize")) {
			ClusterSize = JsonParsed->GetIntegerField("ClusterSize");
		}

		// Size of our binning
		if (JsonParsed->HasTypedField<EJson::Number>("RangeBins")) {
			RangeBins = JsonParsed->GetIntegerField("RangeBins");
		}
		if (JsonParsed->HasTypedField<EJson::Number>("RangeRes")) {
			RangeRes = JsonParsed->GetNumberField("RangeRes")*100;
		}
		if (JsonParsed->HasTypedField<EJson::Number>("AzimuthBins")) {
			AzimuthBins = JsonParsed->GetIntegerField("AzimuthBins");
		}
		if (JsonParsed->HasTypedField<EJson::Number>("AzimuthRes")) {
			AzimuthRes = JsonParsed->GetNumberField("AzimuthRes");
		}
		if (JsonParsed->HasTypedField<EJson::Number>("ElevationBins")) {
			ElevationBins = JsonParsed->GetIntegerField("ElevationBins");
		}
		if (JsonParsed->HasTypedField<EJson::Number>("ElevationRes")) {
			ElevationRes = JsonParsed->GetNumberField("ElevationRes");
		}
	}
	else {
		UE_LOG(LogHolodeck, Fatal, TEXT("UImagingSonar::ParseSensorParms:: Unable to parse json."));
	}

	// Parse through the Range parameters given to us
	if(RangeBins != 0){
		RangeRes = (RangeMax - RangeMin) / RangeBins;
	} 
	else if(RangeRes != 0){
		RangeBins = (RangeMax - RangeMin) / RangeRes;
	}
	else{
		RangeBins = 512;
		RangeRes = (RangeMax - RangeMin) / RangeBins;
	}

	// Parse through the Azimuth parameters given to us
	if(AzimuthBins != 0){
		AzimuthRes = Azimuth / AzimuthBins;
	} 
	else if(AzimuthRes != 0){
		AzimuthBins = Azimuth / AzimuthRes;
	}
	else{
		AzimuthBins = 512;
		AzimuthRes = Azimuth / AzimuthBins;
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
		float dist = (RangeMax - RangeMin) / 8 + RangeMin;
		ElevationBins = (dist*Elevation*Pi/180) / Octree::OctreeMin;
		if(ElevationBins < 1) ElevationBins = 1;
		ElevationRes = Elevation / ElevationBins;
	}
}

void UImagingSonar::InitializeSensor() {
	Super::InitializeSensor();
	
	// Check if we should shadow with less Azimuth bins 
	float dist = (RangeMax - RangeMin) / 8 + RangeMin;
	AzimuthBinScale = 1;
	while(Octree::OctreeMin >= (dist*AzimuthRes*Pi/180)*AzimuthBinScale){
		AzimuthBinScale *= 2;
	}
	if(AzimuthBinScale > AzimuthBins) AzimuthBinScale = AzimuthBins;

	// setup count of each bin
	count = new int32[RangeBins*AzimuthBins]();
	hasPerfectNormal = new int32[AzimuthBins*RangeBins]();

	// Define a perfect reflection
	perfectCos = UKismetMathLibrary::DegCos(8);
	for(int i=0;i<ElevationBins*AzimuthBins/AzimuthBinScale;i++){
		sortedLeaves.Add(TArray<Octree*>());
		sortedLeaves[i].Reserve(10000);
	}

	mapLeaves.Reserve(100000);
}


void UImagingSonar::TickSensorComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) {
	Super::TickSensorComponent(DeltaTime, TickType, ThisTickFunction);

	if(TickCounter == 0){
		// reset things and get ready
		float* result = static_cast<float*>(Buffer);
		std::fill(result, result+RangeBins*AzimuthBins, 0);
		std::fill(count, count+RangeBins*AzimuthBins, 0);
		std::fill(hasPerfectNormal, hasPerfectNormal+AzimuthBins*RangeBins, 0);
		
		for(auto& sl: sortedLeaves){
			sl.Reset();
		}
		mapLeaves.Reset();
		mapSearch.Reset();
		cluster.Reset();


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

				idx = l->idx.Z*AzimuthBins/AzimuthBinScale + l->idx.Y/AzimuthBinScale;
				sortedLeaves[idx].Emplace(l);
			}
		}

		// HANDLE SHADOWING
		shadowLeaves();

		// ADD IN ALL CONTRIBUTIONS
		float noise, pdf;
		for(TArray<Octree*>& bin : sortedLeaves){
			for(Octree* l : bin){
				// Add noise to each of them
				noise = rNoise.sampleExponential();
				pdf = rNoise.exponentialScaledPDF(noise);
				l->idx.X = (int32)((l->locSpherical.X + noise - RangeMin) / RangeRes);
				l->val *= pdf;

				// In case our noise has pushed us out of range
				if(l->idx.X >= RangeBins) l->idx.X = RangeBins-1;

				// Add to their appropriate bin
				idx = l->idx.X*AzimuthBins + l->idx.Y;
				if(l->cos > perfectCos) hasPerfectNormal[idx] += 1;

				result[idx] += l->val;
				++count[idx];
			}
		}

		if(MultiPath){
			// PUT INTO MAP FOR CLUSTER
			for(TArray<Octree*>& binLeafs : sortedLeaves){
				if(binLeafs.Num() > 0){
					// Get first element in this azimuth, elevation bin (ie idx.Y and idx.Z are the same for all of these)
					Octree* jth = binLeafs.GetData()[0];
					mapLeaves.Add(jth->idx, jth);
					int idxR = jth->idx.X;
					// Iterate through only taking ones with different range idx (idx.X)
					// Note that the bin is sorted from shadowing above.
					for(int i=1;i<binLeafs.Num();i++){
						jth = binLeafs.GetData()[i];
						if(jth->idx.X != idxR){
							mapLeaves.Add(jth->idx, jth);
							idxR = jth->idx.X;
						}
					}
				}
			}

			// PUT THEM INTO CLUSTERS
			mapSearch = TMap<FIntVector,Octree*>(mapLeaves);
			mapSearch.Compact();
			int i_start, j_start, k_start, i_end, j_end, k_end;
			Octree** close = nullptr;
			while(mapSearch.Num() > 0){
				// Get start of cluster
				Octree* l = mapSearch.begin()->Value;
				mapSearch.Remove(l->idx);
				cluster.Add({l});

				// Get anything that may be nearby
				i_start = FGenericPlatformMath::Max(0,l->idx.X-ClusterSize);
				j_start = FGenericPlatformMath::Max(0,l->idx.Y-ClusterSize);
				k_start = FGenericPlatformMath::Max(0,l->idx.Z-ClusterSize);
				i_end = FGenericPlatformMath::Min(RangeBins,l->idx.X+ClusterSize+1);
				j_end = FGenericPlatformMath::Min(AzimuthBins,l->idx.Y+ClusterSize+1);
				k_end = FGenericPlatformMath::Min(ElevationBins,l->idx.Z+ClusterSize+1);
				for(int i=i_start; i<i_end; i++){
					for(int j=j_start; j<j_end; j++){
						for(int k=k_start; k<k_end; k++){
							close = mapSearch.Find(FIntVector(i,j,k));
							if(close != nullptr && FVector::DotProduct(l->normal, (*close)->normal) > 0.965){
								cluster.Top().Add(*close);
								mapSearch.Remove((*close)->idx);
							}
						}
					}
				}
			}


			// MULTIPATH CONTRIBUTIONS
			float step_size = Octree::OctreeMin;
			int iterations = RangeMax / Octree::OctreeMin;
			FTransform SensortoWorld = this->GetComponentTransform();
			std::function<FVector(FVector,FVector)> reflect;
			reflect = [](FVector normal, FVector impact){
				return -impact + 2*FVector::DotProduct(normal,impact)*normal;
			};
			ParallelFor(cluster.Num(), [&](int32 i){
				TArray<Octree*>& thisCluster = cluster.GetData()[i];
				Octree* l = thisCluster.GetData()[0];

				FVector reflection = reflect(l->normal, l->normalImpact);
				Octree stepper(l->loc, l->size);
				Octree** hit = nullptr; 
				FVector offset = reflection*step_size*30;

				// TODO: Replace this with real raytracing?
				for(int32 j=0;j<iterations;j++){
					// step
					offset += reflection*step_size;
					stepper.loc = l->loc + offset;

					// make sure it's still in range (& compute spherical coordinates)
					if(!inRange(&stepper)){
						thisCluster.Empty();
						return;
					}

					// Set the index values
					stepper.idx.X = (int32)((stepper.locSpherical.X - RangeMin) / RangeRes);
					stepper.idx.Y = (int32)((stepper.locSpherical.Y - minAzimuth)/ AzimuthRes);
					stepper.idx.Z = (int32)((stepper.locSpherical.Z - minElev)/ ElevationRes);

					// If there's something in that bin
					hit = mapLeaves.Find(stepper.idx);
					if(hit != nullptr){
						FVector returnImpact = reflect((*hit)->normal, -reflection);
						// make sure it's in the right direction
						if(FVector::DotProduct(returnImpact, (*hit)->normalImpact) > 0) break;
						else {
							thisCluster.Empty();
							return;
						}
					}
				} 

				if(hit == nullptr){
					thisCluster.Empty();
					return;
				}

				// If we did hit something, ray trace the rest of everything in the cluster
				float t, noise, pdf, R1, R2;
				FVector locBounce, returnRay;
				for(Octree* m : thisCluster){
					// find 2nd impact location
					reflection = reflect(m->normal, m->normalImpact);
					t = FVector::DotProduct((*hit)->loc - m->loc, (*hit)->normal) / (FVector::DotProduct(reflection, (*hit)->normal));
					locBounce = m->loc + reflection*t;

					// find return vector
					// TODO: See if any change in accuracy in just using the hit version, should be pretty close angles

					// find ray return
					returnRay = reflect((*hit)->normal, -reflection);

					// find spherical location
					Octree bounce(locBounce, m->size);
					inRange(&bounce);
					// float dist = bounce.locSpherical.X;
					bounce.locSpherical.X += m->locSpherical.X + FVector::Dist(bounce.loc, m->loc);
					bounce.locSpherical.X /= 2;

					// Convert to contribution index
					noise = rNoise.sampleExponential();
					pdf = rNoise.exponentialScaledPDF(noise);
					m->idx.X = (int32)((bounce.locSpherical.X + noise - RangeMin) / RangeRes);
					m->idx.Y = (int32)((bounce.locSpherical.Y - minAzimuth)/ AzimuthRes);
					m->cos = FVector::DotProduct(returnRay, (*hit)->normalImpact);
					R1 = (m->z - WaterImpedance) / (m->z + WaterImpedance);
					R2 = ((*hit)->z - WaterImpedance) / ((*hit)->z + WaterImpedance);
					m->val = R1*R1*R2*R2*m->cos*pdf;

					// TODO: There's a bug this is working around, find it and fix it
					if(m->idx.X < 0) m->idx.X = 0;
					if(m->idx.X > RangeBins) m->idx.X = RangeBins-1;
					if(m->idx.Y < 0) m->idx.Y = 0;
					if(m->idx.Y > AzimuthBins) m->idx.Y = AzimuthBins-1;

					// DrawDebugPoint(GetWorld(), m->loc, 3, FColor::Red, false, DeltaTime*TicksPerCapture);
					// DrawDebugPoint(GetWorld(), bounce.loc, 3, FColor::Blue, false, DeltaTime*TicksPerCapture);
				}
			}, false);

			// ADD IN MULTIPATH CONTRIBUTIONS
			for(TArray<Octree*>& bin : cluster){
				for(Octree* l : bin){
					idx = l->idx.X*AzimuthBins + l->idx.Y;

					result[idx] += l->val;
					++count[idx];
				}
			}
		}


		// NORMALIZE & PERTURB RESULTS
		float scale_range, scale_total, azimuth;
		float std = Azimuth/64;
		for (int i=0; i<RangeBins; i++) {
			// Scale along range to recreate intensity dropoff
			scale_range = i*RangeRes/RangeMax;
			scale_range = scale_range*scale_range;
			for(int j=0; j<AzimuthBins; j++){
				// Scale along azimuth to recreat lobe shape
				azimuth = j*AzimuthRes - Azimuth/2;
				scale_total = scale_range*(1 + FMath::Exp(-azimuth*azimuth/std)*0.5);

				if(!ScaleNoise) scale_total = 1;

				idx = i*AzimuthBins + j;

				// Normalize & perturb
				if(count[idx] != 0){
					result[idx] *= (0.5 + multNoise.sampleFloat())/count[idx];
					result[idx] += addNoise.sampleRayleigh()*scale_total;
				}
				else{
					result[idx] = addNoise.sampleRayleigh()*scale_total;
				}
			}
		}

		// CHECK IF ROWS HAVE STREAKING ISSUES
		if(AzimuthStreaks == -1 || AzimuthStreaks == 1){
			float percToBand = 0.08;
			float avgPerfect;
			int numPerfect, numTotal;
			for(int i=0; i<RangeBins; i++){
				// Count how many in that row have dead on normals
				numPerfect = std::accumulate(hasPerfectNormal+i*AzimuthBins, hasPerfectNormal+(i+1)*AzimuthBins, 0);
				numTotal = std::accumulate(count+i*AzimuthBins, count+(i+1)*AzimuthBins, 0);
				avgPerfect = numTotal == 0 ? 0 : (float)numPerfect / (float)numTotal;  
				// UE_LOG(LogHolodeck, Warning, TEXT("Avg Perfect %d, %d, %d, %f"), i, numPerfect, numTotal, avgPerfect);

				// If there's enough, shallow out those bounds
				if(avgPerfect >= percToBand){
					for(int j=0; j<AzimuthBins; j++){
						idx = i*AzimuthBins + j;

						// Attempts to remove streak
						if(AzimuthStreaks == -1){
							result[idx] = result[idx]*result[idx];
						}
						// Adding in streak
						else if(AzimuthStreaks == 1){
							result[idx] = 1 - (1- result[idx])*(1- result[idx]);
						}
					}
				}
			}
		}
	}
}
