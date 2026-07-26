// Written by joshgreaves

#include "Holodeck.h"
#include "HolodeckGameInstance.h"

void UHolodeckGameInstance::StartServer() {
	// HolodeckGameMode should start the server, so it will call this function. 
	if (Server == nullptr) {
		Server = NewObject<UHolodeckServer>();
		Server->Start();
	}
}

UHolodeckServer* UHolodeckGameInstance::GetServer() {
	return Server;
}

UHolodeckGameInstance::UHolodeckGameInstance(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer) {
	UE_LOG(LogHolodeck, Log, TEXT("Holodeck Game Instance initialized"));
}

void UHolodeckGameInstance::Tick(float DeltaTime) {
	// HolodeckGameMode will call tick on this.
	// The release and acquire is what allows this to work in lock step with the client.
	static bool bFirstTime = true;
	if (!bFirstTime) Server->Release();
	Server->Acquire();
	bFirstTime = false;
}

void UHolodeckGameInstance::Init(){
	Super::Init();

	// TODO: Ensure this code also gets called when a new level is loaded
	UWorld* World = GetWorld();
	if (World)
		WorldSettings = static_cast<AHolodeckWorldSettings*>(World->GetWorldSettings());
}
