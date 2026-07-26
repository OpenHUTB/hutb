#pragma once

#include "Holodeck.h"

#include "HolodeckAgentInterface.h"
#include "HolodeckControlScheme.h"

#include "RawControlScheme.generated.h"

/**
* URawControlScheme
*/
UCLASS()
class HOLODECK_API URawControlScheme : public UHolodeckControlScheme {
	GENERATED_BODY()

public:
	// Constructor required by engine. Shouldn't be used
	URawControlScheme(const FObjectInitializer& ObjectInitializer);

	URawControlScheme(AHolodeckAgentInterface* const ControlledAgent);

	void Execute(void* const CommandArray, void* const InputCommand, float DeltaSeconds) override;

	unsigned int GetControlSchemeSizeInBytes() const {
		return Agent->GetRawActionSizeInBytes();
	}

	// Cannot be private and be added to TArray
	UPROPERTY()
	AHolodeckAgentInterface* Agent;
};
