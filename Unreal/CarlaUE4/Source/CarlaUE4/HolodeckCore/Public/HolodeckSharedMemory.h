//
// Created by joshgreaves on 5/9/17.
//

#pragma once

#include <string>
#include <cstring>

#if PLATFORM_WINDOWS
#include "AllowWindowsPlatformTypes.h"
#include <windows.h>
#include "HideWindowsPlatformTypes.h"
#elif PLATFORM_LINUX
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <cerrno>
#endif

/**
  * HolodeckSharedMemory
  * A simple abstraction of memory mapped files for Windows
  * and Linux.
  */
class HOLODECK_API HolodeckSharedMemory {
public:
	/**
	  * Constructor
	  * Constructs a memory mapped file with a given name, prepended
	  * by "HOLODECK_MEM_".
	  * @param Name the name of the memory mapped file. Is prepended.
	  * @param BufferSize the number of bytes to allocated for the file.
	  * @param UUID a UUID for the Holodeck instance to allow multiple shared blocks on one machine.
	  */
    explicit HolodeckSharedMemory(const std::string& Name, unsigned int BufferSize, const std::string& UUID);

	/**
	  * Destructor
	  */
	~HolodeckSharedMemory();

	/**
	  * GetPtr
	  * Gets a pointer to the start of the memory mapped file.
	  * @return a void pointer to the start of the memory buffer.
	  */
	void* GetPtr() const {return MemPointer;}

	/**
	  * Size
	  * Gets the size of the allocated memory mapped file.
	  * @return the size in bytes of the file.
	  */
	int Size() const {return MemSize;}

private:
    std::string MemPath;
    unsigned int MemSize;
	void* MemPointer;

	#if PLATFORM_WINDOWS
	HANDLE MemFile;
	#elif PLATFORM_LINUX
	int MemFile;
    #endif

    void LogSystemError(const std::string &errorMessage);
};
