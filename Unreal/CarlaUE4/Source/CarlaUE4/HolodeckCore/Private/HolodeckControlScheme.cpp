#include "Holodeck.h"

#include "HolodeckControlScheme.h"


UHolodeckControlScheme::UHolodeckControlScheme() {}

UHolodeckControlScheme::UHolodeckControlScheme(const FObjectInitializer& ObjectInitializer) :
		Super(ObjectInitializer) {}

void UHolodeckControlScheme::Execute(void* const CommandArray, void* const InputCommand, float DeltaSeconds) {
	check(0 && "You must override Execute");
}

unsigned int UHolodeckControlScheme::GetControlSchemeSizeInBytes() const {
	check(0 && "You must override GetControlSchemeByteSize");
	return 0;
}
