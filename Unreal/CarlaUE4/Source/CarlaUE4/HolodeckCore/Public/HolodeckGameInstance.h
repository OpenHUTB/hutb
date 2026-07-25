// Written by joshgreaves.

#pragma once

#include "Holodeck.h"

#include "Engine/GameInstance.h"
#include "HolodeckServer.h"
#include "HolodeckWorldSettings.h"

#include "HolodeckGameInstance.generated.h"

/**
* UHolodeckGameInstance
* An instance of a holodeck game.
* It is a placeholder for important information, and it is also used for starting the server. 
* tell their controller to open up the channels needed for giving commands across the python binding.
*/
UCLASS()
class UHolodeckGameInstance : public UGameInstance
{
	GENERATED_BODY()

public:
	/**
	  * Default Constructor
	  */
	UHolodeckGameInstance(const FObjectInitializer& ObjectInitializer);

	/**
	  * GetServer
	  * StartServer must be called before this is called.
	  * @return a pointer to the server, or nullptr if Holodeck is off.
	  */
	UHolodeckServer* GetServer();

	/**
	  * StartServer
	  * Should only be called by HolodeckGameMode.
	  * Starts the server.
	  */
	void StartServer();

	/**
	  * Tick
	  * Ticks the game instance.
	  * Is called by HolodeckGameMode.
	  * @param DeltaTime the seconds passed since the last tick.
	  */
	void Tick(float DeltaTime);

	/**
	  * Init
	  * Starts the GameInstance.
	  */
	void Init();

	UPROPERTY(BlueprintReadWrite)
		bool SpectatorMode;
	UPROPERTY(BlueprintReadWrite)
		bool DirectAttachMode;
	UPROPERTY(BlueprintReadWrite)
		int AttatchedAgentIndex = -1;

	UPROPERTY(BlueprintReadWrite)
		FTransform SpectatorTransform;

	UPROPERTY(BlueprintReadWrite)
		FVector ViewRotation;

private:
	UPROPERTY()
		UHolodeckServer* Server;
	AHolodeckWorldSettings* WorldSettings;
};
