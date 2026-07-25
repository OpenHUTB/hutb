#include "Holodeck.h"
#include "TaskHelper.h"


FVector GetActorSocketLocation(AActor* actor, FString socket_name) {
	FVector StartVec = actor->GetActorLocation();
	UStaticMeshComponent* Mesh = (UStaticMeshComponent*)actor->GetComponentByClass(TSubclassOf<UStaticMeshComponent>());
	if (Mesh && Mesh->DoesSocketExist(FName(*socket_name))) {
		StartVec = Mesh->GetSocketLocation(FName(*socket_name));
	}
	return StartVec;
}


bool IsInSight(AActor* seeing_actor, AActor* target_actor, FVector& start, FVector& end, float FOVRad, FVector& DistanceVec, float Distance) {
	// Get angle to target
	float TargetAngle = FGenericPlatformMath::Acos(FVector::DotProduct(DistanceVec / Distance, seeing_actor->GetActorForwardVector()));
	GEngine->AddOnScreenDebugMessage(-1, 15.0f, FColor::Yellow, FString::Printf(TEXT("Angle %f"), TargetAngle));
	// Get trace to target
	FCollisionQueryParams QueryParams = FCollisionQueryParams();
	QueryParams.AddIgnoredActor(seeing_actor);
	FHitResult Hit = FHitResult();
	bool TraceResult = seeing_actor->GetWorld()->LineTraceSingleByChannel(Hit, start, end, ECollisionChannel::ECC_Visibility, QueryParams);

	// Evaluate - if the actor is in our field of view and either the ray trace has intersected with the target or there is nothing between ourself and the target
	return TargetAngle < FOVRad && (Hit.Actor == target_actor || Hit.Actor == nullptr);
}