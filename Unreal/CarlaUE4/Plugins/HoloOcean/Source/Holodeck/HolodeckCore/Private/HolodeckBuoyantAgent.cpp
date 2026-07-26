// MIT License (c) 2021 BYU FRoStLab see LICENSE file

#include "Holodeck.h"
#include "HolodeckBuoyantAgent.h"

void AHolodeckBuoyantAgent::InitializeAgent(){
	Super::InitializeAgent();

	// Get GravityVectority from world
	AWorldSettings* WorldSettings = GetWorld()->GetWorldSettings(false, false);
	Gravity = WorldSettings->GetGravityZ() / -100;

	// Set Mass
	RootMesh->SetMassOverrideInKg("", MassInKG);
	// Set COM (have to do some calculation since it sets an offset)
	FVector COM_curr = GetActorRotation().UnrotateVector( RootMesh->GetCenterOfMass() - GetActorLocation() );
	RootMesh->SetCenterOfMass( CenterMass + OffsetToOrigin - COM_curr );

	// Set Bounding Box (if it hasn't been set by hand)
	if(BoundingBox.GetExtent() == FVector(0, 0, 0))
		BoundingBox = RootMesh->GetStaticMesh()->GetBoundingBox();

	// Sample points (if they haven't already been set)
	if(SurfacePoints.Num() == 0){
		for(int i=0;i<NumSurfacePoints;i++){
			FVector random = UKismetMathLibrary::RandomPointInBoundingBox(FVector(0,0,0), BoundingBox.GetExtent());
			// We pre-add all offsets to reduce computation during simulation
			SurfacePoints.Add( random + OffsetToOrigin + CenterVehicle );
		}
	}
	// Otherwise make sure our count is correct (we'll use it later)
	else{
		NumSurfacePoints = SurfacePoints.Num();
	}
}

void AHolodeckBuoyantAgent::Tick(float DeltaSeconds) {
	Super::Tick(DeltaSeconds);
	if(octreeGlobal != nullptr) updateOctree(octreeLocal, octreeGlobal);
}

void AHolodeckBuoyantAgent::BeginDestroy() {
	Super::BeginDestroy();

	if(octreeLocal != nullptr) delete octreeLocal;
	if(octreeGlobal != nullptr) delete octreeGlobal;
}

void AHolodeckBuoyantAgent::ApplyBuoyantForce(){
    //Get all the values we need once
    FVector ActorLocation = GetActorLocation();
	FRotator ActorRotation = GetActorRotation();

	// Check to see how underwater we are
	FVector* points = SurfacePoints.GetData();
	int count = 0;
	for(int i=0;i<NumSurfacePoints;i++){	
		FVector p_world = ActorLocation + ActorRotation.RotateVector(points[i]);
		if(p_world.Z < SurfaceLevel)
			count++;
	}
	float ratio = count*1.0 / NumSurfacePoints;

    // Get and apply Buoyant Force
	float BuoyantForce = Volume * Gravity * WaterDensity * ratio;
	FVector BuoyantVector = FVector(0, 0, BuoyantForce);
	BuoyantVector = ConvertLinearVector(BuoyantVector, ClientToUE);

    FVector COB_World = ActorLocation + ActorRotation.RotateVector(CenterBuoyancy + OffsetToOrigin);
	RootMesh->AddForceAtLocation(BuoyantVector, COB_World);

	FVector GravityVector = ConvertLinearVector(FVector(0, 0, -Gravity*MassInKG), ClientToUE);
	RootMesh->AddForceAtLocation(GravityVector, RootMesh->GetCenterOfMass());
}

void AHolodeckBuoyantAgent::ShowBoundingBox(){
	FVector location = GetActorLocation() + GetActorRotation().RotateVector(OffsetToOrigin + CenterVehicle);
	DrawDebugBox(GetWorld(), location, BoundingBox.GetExtent(), GetActorQuat(), FColor::Red, false, 0.05, 0, 1);
}

void AHolodeckBuoyantAgent::ShowSurfacePoints(){
	FVector ActorLocation = GetActorLocation();
	FRotator ActorRotation = GetActorRotation();
	FVector* points = SurfacePoints.GetData();

	for(int i=0;i<NumSurfacePoints;i++){
		FVector p_world = ActorLocation + ActorRotation.RotateVector(points[i]);
		DrawDebugPoint(GetWorld(), p_world, 5, FColor::Red, false, 0.05);
	}
}

Octree* AHolodeckBuoyantAgent::makeOctree(){
	if(octreeGlobal == nullptr){
		UE_LOG(LogHolodeck, Log, TEXT("HolodeckBuoyantAgent::Making Octree"));
		float OctreeMin = Octree::OctreeMin;
		float OctreeMax = Octree::OctreeMin;

		// Shrink to the smallest cube the actor fits in
		FVector center = BoundingBox.GetCenter() + GetActorLocation();
		float extent = BoundingBox.GetExtent().GetAbsMax()*2;
		while(OctreeMax < extent){
			OctreeMax *= 2;
		}

		// Otherwise, make the octrees
		octreeGlobal = Octree::makeOctree(center, OctreeMax, OctreeMin, GetName());
		if(octreeGlobal){
			octreeGlobal->isAgent = true;
			octreeGlobal->file = "AGENT";

			// Convert our global octree to a local one
			octreeLocal = cleanOctree(octreeGlobal);
		}
		else{
			UE_LOG(LogHolodeck, Warning, TEXT("HolodeckBuoyantAgent:: Failed to make Octree"));
		}

	}

	return octreeGlobal;
}

Octree* AHolodeckBuoyantAgent::cleanOctree(Octree* globalFrame){
	Octree* local = new Octree;
	local->loc = GetActorRotation().UnrotateVector(globalFrame->loc - GetActorLocation());
	local->normal = GetActorRotation().UnrotateVector(globalFrame->normal);

	for( Octree* tree : globalFrame->leaves){
		Octree* l = cleanOctree(tree);
		local->leaves.Add(l);
	}

	return local;
}

void AHolodeckBuoyantAgent::updateOctree(Octree* localFrame, Octree* globalFrame){
	globalFrame->loc = GetActorLocation() + GetActorRotation().RotateVector(localFrame->loc);
	globalFrame->normal = GetActorRotation().RotateVector(localFrame->normal);

	for(int i=0;i<globalFrame->leaves.Num();i++){
		updateOctree(localFrame->leaves[i], globalFrame->leaves[i]);
	}
}