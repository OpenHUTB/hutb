// MIT License (c) 2021 BYU FRoStLab see LICENSE file

#include "Holodeck.h"
#include "Benchmarker.h"
#include "HolodeckBuoyantAgent.h"
#include "HolodeckSonar.h"

float UHolodeckSonar::ATan2Approx(float y, float x){
    //http://pubs.opengroup.org/onlinepubs/009695399/functions/atan2.html
    //Volkan SALMA

    const float ONEQTR_PI = Pi / 4.0;
	const float THRQTR_PI = 3.0 * Pi / 4.0;
	float r, angle;
	float abs_y = fabs(y) + 1e-10f;      // kludge to prevent 0/0 condition
	if ( x < 0.0f )
	{
		r = (x + abs_y) / (abs_y - x);
		angle = THRQTR_PI;
	}
	else
	{
		r = (x - abs_y) / (x + abs_y);
		angle = ONEQTR_PI;
	}
	angle += (0.1963f * r * r - 0.9817f) * r;
	angle *= 180/Pi;
	if ( y < 0.0f )
		return( -angle );     // negate if in quad III or IV
	else
		return( angle );
}


// Allows sensor parameters to be set programmatically from client.
void UHolodeckSonar::ParseSensorParms(FString ParmsJson) {
	Super::ParseSensorParms(ParmsJson);

	TSharedPtr<FJsonObject> JsonParsed;
	TSharedRef<TJsonReader<TCHAR>> JsonReader = TJsonReaderFactory<TCHAR>::Create(ParmsJson);
	if (FJsonSerializer::Deserialize(JsonReader, JsonParsed)) {
		// Geometry Parameters
		if (JsonParsed->HasTypedField<EJson::Number>("RangeMax")) {
			RangeMax = JsonParsed->GetNumberField("RangeMax")*100;
		}
		if (JsonParsed->HasTypedField<EJson::Number>("RangeMin")) {
			RangeMin = JsonParsed->GetNumberField("RangeMin")*100;
		}
		if (JsonParsed->HasTypedField<EJson::Number>("Azimuth")) {
			Azimuth = JsonParsed->GetNumberField("Azimuth");
		}
		if (JsonParsed->HasTypedField<EJson::Number>("Elevation")) {
			Elevation = JsonParsed->GetNumberField("Elevation");
		}

		// Misc Parameters
		if (JsonParsed->HasTypedField<EJson::Number>("InitOctreeRange")) {
			InitOctreeRange = JsonParsed->GetNumberField("InitOctreeRange")*100;
		}
		if (JsonParsed->HasTypedField<EJson::Number>("TicksPerCapture")) {
			TicksPerCapture = JsonParsed->GetIntegerField("TicksPerCapture");
		}

		// Visualization Parameters
		if (JsonParsed->HasTypedField<EJson::Boolean>("ViewRegion")) {
			ViewRegion = JsonParsed->GetBoolField("ViewRegion");
		}
		if (JsonParsed->HasTypedField<EJson::Number>("ViewOctree")) {
			ViewOctree = JsonParsed->GetIntegerField("ViewOctree");
		}
		if (JsonParsed->HasTypedField<EJson::Boolean>("ShowWarning")) {
			ShowWarning = JsonParsed->GetBoolField("ShowWarning");
		}

		// Performance Parameters
		if (JsonParsed->HasTypedField<EJson::Number>("ShadowEpsilon")) {
			ShadowEpsilon = JsonParsed->GetNumberField("ShadowEpsilon")*100;
		}
		if (JsonParsed->HasTypedField<EJson::Number>("WaterDensity")) {
			WaterDensity = JsonParsed->GetNumberField("WaterDensity");
		}
		if (JsonParsed->HasTypedField<EJson::Number>("WaterSpeedSound")) {
			WaterSpeedSound = JsonParsed->GetNumberField("WaterSpeedSound");
		}


	}
	else {
		UE_LOG(LogHolodeck, Fatal, TEXT("UHolodeckSonar::ParseSensorParms:: Unable to parse json."));
	}

	if(InitOctreeRange == 0){
		InitOctreeRange = RangeMax;
	}

	if(ShadowEpsilon == 0){
		ShadowEpsilon = 4*Octree::OctreeMin;
	}

	minAzimuth = -Azimuth/2;
	maxAzimuth = Azimuth/2;
	minElev = 90 - Elevation/2;
	maxElev = 90 + Elevation/2;

	WaterImpedance = WaterDensity * WaterSpeedSound;

	sqrt3_2 = UKismetMathLibrary::Sqrt(3) / 2;
	sinOffset = UKismetMathLibrary::DegSin(FGenericPlatformMath::Min(Azimuth, Elevation)/2);
}

void UHolodeckSonar::BeginDestroy() {
	Super::BeginDestroy();

	delete octree;
}

void UHolodeckSonar::initOctree(){
	// We delay making trees till the message has been printed to the screen
	if(toMake.Num() != 0 && TickCounter >= 8){
		UE_LOG(LogHolodeck, Log, TEXT("Sonar::Initial building num: %d"), toMake.Num());
		ParallelFor(toMake.Num(), [&](int32 i){
			toMake.GetData()[i]->load();
			toMake.GetData()[i]->unload();
		});
		toMake.Empty();
		GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Red, TEXT("Finished."));
	}

	// If we haven't made it yet
	if(octree == nullptr && TickCounter >= 5){
		// initialize small octree for each agent
		for(auto& agent : Controller->GetServer()->AgentMap){
			// skip ourselves
			if(agent.Value == this->GetAttachmentRootActor()) continue;
			AHolodeckBuoyantAgent* bouyantActor = static_cast<AHolodeckBuoyantAgent*>(agent.Value);
			Octree* l = bouyantActor->makeOctree();
			if(l) agents.Add(l);
		}
		
		// Ignore necessary agents to make world one
		for(auto& agent : Controller->GetServer()->AgentMap){
			AActor* actor = static_cast<AActor*>(agent.Value);
			Octree::ignoreActor(actor);
		}
		// make/load octree
		octree = Octree::makeEnvOctreeRoot();

		// Premake octrees within range
		FVector loc = this->GetComponentLocation();
		// Offset by size of OctreeMax radius to get everything in range
		float offset = InitOctreeRange + Octree::OctreeMax*FMath::Sqrt(3)/2;
		// recursively search for close leaves
		std::function<void(Octree*, TArray<Octree*>&)> findCloseLeaves;
		findCloseLeaves = [&offset, &loc, &findCloseLeaves](Octree* tree, TArray<Octree*>& list){
			if(tree->size == Octree::OctreeMax){
				if((loc - tree->loc).Size() <= offset && !FPaths::FileExists(tree->file)){
					list.Add(tree);
				}
			}
			else{
				for(Octree* l : tree->leaves){
					findCloseLeaves(l, list);
				}
			}
		};
		findCloseLeaves(octree, toMake);
		if(toMake.Num() != 0)
			GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Red, FString::Printf(TEXT("Premaking %d Octrees, will take some time..."), toMake.Num()));

		// get all our Leaves ready
		bigLeaves.Reserve(1000);
		// We need enough foundLeaves to cover all possible threads
		// 1000 should be plenty
		foundLeaves.Reserve(1000);
		for(int i=0;i<1000;i++){
			foundLeaves.Add(TArray<Octree*>());
			foundLeaves[i].Reserve(10000);
		}
	}
}

void UHolodeckSonar::viewLeaves(Octree* tree){
	if(tree->leaves.Num() == 0){
		DrawDebugPoint(GetWorld(), tree->loc, 5, FColor::Red, false, .03*TicksPerCapture);
	}
	else{
		for( Octree* l : tree->leaves){
			viewLeaves(l);
		}
	}
}

bool UHolodeckSonar::inRange(Octree* tree){
	FTransform SensortoWorld = this->GetComponentTransform();
	// if it's not a leaf, we use a bigger search area
	float offset = 0;
	float radius = 0;
	if(tree->size != Octree::OctreeMin){
		radius = tree->size*sqrt3_2;
		offset = radius/sinOffset;
		SensortoWorld.AddToTranslation( -this->GetForwardVector()*offset );
	}

	// transform location to sensor frame
	// FVector locLocal = SensortoWorld.InverseTransformPositionNoScale(tree->loc);
	FVector locLocal = SensortoWorld.GetRotation().UnrotateVector(tree->loc-SensortoWorld.GetTranslation());

	// check if it's in range
	tree->locSpherical.X = locLocal.Size();
	if(RangeMin+offset-radius >= tree->locSpherical.X || tree->locSpherical.X >= RangeMax+offset+radius) return false; 

	// check if azimuth is in
	tree->locSpherical.Y = ATan2Approx(-locLocal.Y, locLocal.X);
	if(minAzimuth >= tree->locSpherical.Y || tree->locSpherical.Y >= maxAzimuth) return false;

	// check if elevation is in
	tree->locSpherical.Z = ATan2Approx(locLocal.Size2D(), locLocal.Z);
	if(minElev >= tree->locSpherical.Z || tree->locSpherical.Z >= maxElev) return false;
	
	// otherwise it's in!
	return true;
}	

void UHolodeckSonar::leavesInRange(Octree* tree, TArray<Octree*>& rLeaves, float stopAt){
	bool in = inRange(tree);
	if(in){
		if(tree->size == stopAt){
			if(stopAt == Octree::OctreeMin){
				// Compute contribution while we're parallelized
				// If no contribution, we don't have to add it in
				tree->normalImpact = GetComponentLocation() - tree->loc; 
				tree->normalImpact.Normalize();

				// compute contribution
				float cos = FVector::DotProduct(tree->normal, tree->normalImpact);
				if(cos > 0){
					tree->cos = cos;
					rLeaves.Add(tree);
				} 
			}
			else{
				rLeaves.Add(tree);
			}
			return;
		}

		for(Octree* l : tree->leaves){
			leavesInRange(l, rLeaves, stopAt);
		}
	}
	else if(tree->size >= Octree::OctreeMax){
		tree->unload();
	}
}

void UHolodeckSonar::findLeaves(){
	// Empty everything out
	bigLeaves.Reset();
	for(auto& fl: foundLeaves){
		fl.Reset();
	}

	// FILTER TO GET THE bigLeaves WE WANT
	leavesInRange(octree, bigLeaves, Octree::OctreeMax);
	bigLeaves += agents;

	ParallelFor(bigLeaves.Num(), [&](int32 i){
		Octree* leaf = bigLeaves.GetData()[i];
		leaf->load();
		for(Octree* l : leaf->leaves)
			leavesInRange(l, foundLeaves.GetData()[i%1000], Octree::OctreeMin);
	});
}

void UHolodeckSonar::shadowLeaves(){
	ParallelFor(sortedLeaves.Num(), [&](int32 i){
		TArray<Octree*>& binLeafs = sortedLeaves.GetData()[i]; 

		// sort from closest to farthest
		binLeafs.Sort([](const Octree& a, const Octree& b){
			return a.locSpherical.X < b.locSpherical.X;
		});

		// Get the closest cluster in the bin
		float diff, R;
		Octree* jth;
		for(int32 j=0;j<binLeafs.Num();j++){
			jth = binLeafs.GetData()[j];
			
			R = (jth->z - WaterImpedance) / (jth->z + WaterImpedance);
			jth->val = R*R*jth->cos;

			// diff = FVector::Dist(jth->loc, binLeafs.GetData()[j+1]->loc);
			if(j != binLeafs.Num()-1){
				diff = FMath::Abs(jth->locSpherical.X - binLeafs.GetData()[j+1]->locSpherical.X);
				if(diff > ShadowEpsilon){
					binLeafs.RemoveAt(j+1,binLeafs.Num()-j-1);
					break;
				}
			}
		}

	});
}

void UHolodeckSonar::showBeam(float DeltaTime){
	// draw points inside our region
	if(ViewOctree >= -1){
		for( TArray<Octree*> bins : sortedLeaves){
			for( Octree* l : bins){
				if(ViewOctree == -1 || ViewOctree == l->idx.Y){
					DrawDebugPoint(GetWorld(), l->loc, 5, FColor::Red, false, DeltaTime*TicksPerCapture);
				}
			}
		}
	}
}

void UHolodeckSonar::showRegion(float DeltaTime){
	// draw outlines of our region
	if(ViewRegion){
		FTransform tran = this->GetComponentTransform();
		float debugThickness = 3.0f;
		
		// range lines
		DrawDebugLine(GetWorld(), spherToEuc(RangeMin, minAzimuth, minElev, tran), spherToEuc(RangeMax, minAzimuth, minElev, tran), FColor::Green, false, DeltaTime*TicksPerCapture, ECC_WorldStatic, debugThickness);
		DrawDebugLine(GetWorld(), spherToEuc(RangeMin, minAzimuth, maxElev, tran), spherToEuc(RangeMax, minAzimuth, maxElev, tran), FColor::Green, false, DeltaTime*TicksPerCapture, ECC_WorldStatic, debugThickness);
		DrawDebugLine(GetWorld(), spherToEuc(RangeMin, maxAzimuth, minElev, tran), spherToEuc(RangeMax, maxAzimuth, minElev, tran), FColor::Green, false, DeltaTime*TicksPerCapture, ECC_WorldStatic, debugThickness);
		DrawDebugLine(GetWorld(), spherToEuc(RangeMin, maxAzimuth, maxElev, tran), spherToEuc(RangeMax, maxAzimuth, maxElev, tran), FColor::Green, false, DeltaTime*TicksPerCapture, ECC_WorldStatic, debugThickness);

		// azimuth lines (should be arcs, we're being lazy)
		DrawDebugLine(GetWorld(), spherToEuc(RangeMin, minAzimuth, minElev, tran), spherToEuc(RangeMin, maxAzimuth, minElev, tran), FColor::Green, false, DeltaTime*TicksPerCapture, ECC_WorldStatic, debugThickness);
		DrawDebugLine(GetWorld(), spherToEuc(RangeMin, minAzimuth, maxElev, tran), spherToEuc(RangeMin, maxAzimuth, maxElev, tran), FColor::Green, false, DeltaTime*TicksPerCapture, ECC_WorldStatic, debugThickness);
		DrawDebugLine(GetWorld(), spherToEuc(RangeMax, minAzimuth, minElev, tran), spherToEuc(RangeMax, maxAzimuth, minElev, tran), FColor::Green, false, DeltaTime*TicksPerCapture, ECC_WorldStatic, debugThickness);
		DrawDebugLine(GetWorld(), spherToEuc(RangeMax, minAzimuth, maxElev, tran), spherToEuc(RangeMax, maxAzimuth, maxElev, tran), FColor::Green, false, DeltaTime*TicksPerCapture, ECC_WorldStatic, debugThickness);

		// elevation lines (should be arcs, we're being lazy)
		DrawDebugLine(GetWorld(), spherToEuc(RangeMin, minAzimuth, minElev, tran), spherToEuc(RangeMin, minAzimuth, maxElev, tran), FColor::Green, false, DeltaTime*TicksPerCapture, ECC_WorldStatic, debugThickness);
		DrawDebugLine(GetWorld(), spherToEuc(RangeMin, maxAzimuth, minElev, tran), spherToEuc(RangeMin, maxAzimuth, maxElev, tran), FColor::Green, false, DeltaTime*TicksPerCapture, ECC_WorldStatic, debugThickness);
		DrawDebugLine(GetWorld(), spherToEuc(RangeMax, minAzimuth, minElev, tran), spherToEuc(RangeMax, minAzimuth, maxElev, tran), FColor::Green, false, DeltaTime*TicksPerCapture, ECC_WorldStatic, debugThickness);
		DrawDebugLine(GetWorld(), spherToEuc(RangeMax, maxAzimuth, minElev, tran), spherToEuc(RangeMax, maxAzimuth, maxElev, tran), FColor::Green, false, DeltaTime*TicksPerCapture, ECC_WorldStatic, debugThickness);
	}		
}

FVector UHolodeckSonar::spherToEuc(float r, float theta, float phi, FTransform SensortoWorld){
	float x = r*UKismetMathLibrary::DegSin(phi)*UKismetMathLibrary::DegCos(theta);
	float y = r*UKismetMathLibrary::DegSin(phi)*UKismetMathLibrary::DegSin(theta);
	float z = r*UKismetMathLibrary::DegCos(phi);
	return UKismetMathLibrary::TransformLocation(SensortoWorld, FVector(x, y, z));
}

void UHolodeckSonar::TickSensorComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) {
	// We initialize this here to make sure all agents are loaded
	// This does nothing if it's already been loaded
	initOctree();

	// If the sonar ran last timestep, show off what it saw
	if(TickCounter == 0){
		showBeam(DeltaTime);
		showRegion(DeltaTime);
	}

	// Count till next sonar timestep
	TickCounter++;
	// Show warning when we're about to compute sonar
	if(ShowWarning && TicksPerCapture - TickCounter <= 3){
		GEngine->AddOnScreenDebugMessage(-1, DeltaTime, FColor::Red, FString::Printf(TEXT("Sonar octree generation may slow down simulation, screen may appear frozen temporarily")));
	}
	if(TickCounter % TicksPerCapture == 0 && octree != nullptr && toMake.Num() == 0){
		TickCounter = 0;
	}
}