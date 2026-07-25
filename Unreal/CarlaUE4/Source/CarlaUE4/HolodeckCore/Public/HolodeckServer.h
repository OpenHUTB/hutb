// Created by joshgreaves on 5/9/17.

#pragma once

#include "Holodeck.h"

#include <map>
#include <memory>
#include <string>
#include <cstring>

#include "HolodeckSharedMemory.h"
#if PLATFORM_WINDOWS
#define LOADING_SEMAPHORE_PATH "Global\\HOLODECK_LOADING_SEM"
#define SEMAPHORE_PATH1 "Global\\HOLODECK_SEMAPHORE_SERVER"
#define SEMAPHORE_PATH2 "Global\\HOLODECK_SEMAPHORE_CLIENT"
#include "AllowWindowsPlatformTypes.h"
#include "Windows.h"
#include "HideWindowsPlatformTypes.h"
#elif PLATFORM_LINUX
#define LOADING_SEMAPHORE_PATH "/HOLODECK_LOADING_SEM"
#define SEMAPHORE_PATH1 "/HOLODECK_SEMAPHORE_SERVER"
#define SEMAPHORE_PATH2 "/HOLODECK_SEMAPHORE_CLIENT"
#include <sys/mman.h>
#include <unistd.h>
#include <semaphore.h>
#include <cerrno>

#endif

#include "HolodeckServer.generated.h"


/* Forward declare HolodeckAgent Class*/
class AHolodeckAgent;

/**
  * UHolodeckServer
  * This class resides in Holodeck, and handles the passing of messages through
  * shared memory. A new shared memory block is created for each sensor, action
  * space, and setting.
  * There should only be one UHolodeckServer, and it should be instantiated by
  * HolodeckGameInstance. HolodeckGameMode calls HolodeckGameInstance::StartServer()
  */
UCLASS()
class HOLODECK_API UHolodeckServer : public UObject {
	GENERATED_BODY()

public:

	/**
	  * Default Constructor
	  */
	UHolodeckServer();

	/**
	  * Default Destructor
	  */
	~UHolodeckServer();

	/**
	  * Start
	  * Starts the server.
	  */
	void Start();

	/**
	  * Kill
	  * Shuts the server down.
	  */
	void Kill();

	/**
	  * Malloc
	  * Mallocs shared memory.
	  * If memory has already been malloc'ed with the same key,
	  * the block is overwritten.
	  * @param Key the key for this block of memory.
	  * @param BufferSize the size to allocate in bytes.
	  * @return a pointer to the start of the assigned memory.
	  */
	void* Malloc(const std::string& Key, unsigned int BufferSize);

	/**
	  * Acquire
	  * Acquires the mutex to allow the next tick to occur. Will block until
	  * the client releases.
	  */
	void Acquire();

	/**
	  * Release
	  * Releases the mutex to allow the client to tick.
	  */
	void Release();

	/**
	  * IsRunning
	  * Gets whether the server is running.
	  * @return true if the server is running.
	  */
	bool IsRunning() const;

	/**
	* MakeKey
	* Makes a key for mallocing a specific item for a specific agent.
	* @param AgentName the name of the agent.
	* @param ItemName the name of the item.
	* @return a key for this agent/item.
	*/
	static const std::string MakeKey(const std::string& AgentName, const std::string& ItemName) {
		return AgentName + "_" + ItemName;
	}
	static const std::string MakeKey(const FString& AgentName, const std::string& ItemName) {
		return std::string(TCHAR_TO_UTF8(*AgentName)) + "_" + ItemName;
	}
	static const std::string MakeKey(const FString& AgentName, const FString& ItemName) {
		return std::string(TCHAR_TO_UTF8(*AgentName)) + "_" + std::string(TCHAR_TO_UTF8(*ItemName));
	}

	/* Stores pointers to all the agents within the world. */
	TMap<FString, AHolodeckAgent*> AgentMap;

private:

	FString UUID;
	std::map<std::string, std::unique_ptr<HolodeckSharedMemory>> Memory;
	bool bIsRunning;

	#if PLATFORM_WINDOWS
	HANDLE LockingSemaphore1;
	HANDLE LockingSemaphore2;
	#elif PLATFORM_LINUX
	sem_t* LockingSemaphore1;
	sem_t* LockingSemaphore2;
    #endif

	void LogSystemError(const std::string &errorMessage);
};
