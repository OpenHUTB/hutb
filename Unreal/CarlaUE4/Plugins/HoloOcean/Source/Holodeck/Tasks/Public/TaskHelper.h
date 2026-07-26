#pragma once

#include "Holodeck.h"

FVector GetActorSocketLocation(AActor* actor, FString socket_name);

bool IsInSight(AActor* seeing_actor, AActor* target_actor, FVector& start, FVector& end, float FOVRad, FVector& DistanceVec, float Distance);
