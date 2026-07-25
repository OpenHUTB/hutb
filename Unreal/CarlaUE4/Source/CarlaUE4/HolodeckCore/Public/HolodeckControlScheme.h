#pragma once

#include "Holodeck.h"

#include "HolodeckControlScheme.generated.h"

/**
  * UHolodeckControlScheme
  */
UCLASS()
class CARLAUE4_API UHolodeckControlScheme : public UObject {
	GENERATED_BODY()

public:
	UHolodeckControlScheme();
	UHolodeckControlScheme(const FObjectInitializer& ObjectInitializer);

	virtual void Execute(void* const CommandArray, void* const InputCommand, float DeltaSeconds);

	virtual unsigned int GetControlSchemeSizeInBytes() const;
};
