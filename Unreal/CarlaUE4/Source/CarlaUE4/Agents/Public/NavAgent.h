// MIT License (c) 2019 BYU PCCL see LICENSE file

#pragma once

#include "CoreMinimal.h"
#include "HolodeckCore/Public/HolodeckAgent.h"
#include "Holodeck.h"
#include "HolodeckAgent.h"

#include "NavAgent.generated.h"

UCLASS()
class HOLODECK_API ANavAgent : public AHolodeckAgent
{
	GENERATED_BODY()

public:
	/**
	* Default Constructor.
	*/
	ANavAgent();

	/**
	* BeginPlay
	* Called when the game starts.
	* Registers the reward and terminal signals.
	*/
	void InitializeAgent() override;

	/**
	* Tick
	* Called each frame.
	* @param DeltaSeconds the time since the last tick.
	*/
	void Tick(float DeltaSeconds) override;

	UFUNCTION(BlueprintCallable, Category="Holodeck")
	FVector GetTarget();

	UFUNCTION(BlueprintCallable, Category = "Holodeck")
	void SetTarget(float x, float y, float z);

	unsigned int GetRawActionSizeInBytes() const override { return 3 * sizeof(float); };
	void* GetRawActionBuffer() const override { return (void*)CommandArray; };

private:
	float CommandArray[3];
	FVector Target;

};
