// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

// This file is included before any other file in every compile unit within the
// plugin.
#pragma once


#include "Util/NonCopyable.h"
#include "Logging/LogMacros.h"
#include "Modules/ModuleInterface.h"

DECLARE_LOG_CATEGORY_EXTERN(LogCarla, Log, All);
DECLARE_LOG_CATEGORY_EXTERN(LogCarlaServer, Log, All);

// DisplayName, GroupName, Third param is always Advanced.
// DECLARE_STATS_GROUP(TEXT("Carla"), STATGROUP_Carla, STATCAT_Advanced);
DECLARE_STATS_GROUP(TEXT("CarlaSensor"), STATGROUP_CarlaSensor, STATCAT_Advanced);

//DECLARE_MEMORY_STAT(TEXT("CARLAMEMORY"), STATGROUP_CARLAMEMORY, STATCAT_Advanced)

DECLARE_CYCLE_STAT(TEXT("Read RT"),     STAT_CarlaSensorReadRT,     STATGROUP_CarlaSensor);
DECLARE_CYCLE_STAT(TEXT("Copy Text"),   STAT_CarlaSensorCopyText,   STATGROUP_CarlaSensor);
DECLARE_CYCLE_STAT(TEXT("Buffer Copy"), STAT_CarlaSensorBufferCopy, STATGROUP_CarlaSensor);
DECLARE_CYCLE_STAT(TEXT("Stream Send"), STAT_CarlaSensorStreamSend, STATGROUP_CarlaSensor);

// Options to compile with extra debug log.
#if WITH_EDITOR
// #define CARLA_AI_VEHICLES_EXTRA_LOG
// #define CARLA_AI_WALKERS_EXTRA_LOG
// #define CARLA_ROAD_GENERATOR_EXTRA_LOG
// #define CARLA_SERVER_EXTRA_LOG
// #define CARLA_TAGGER_EXTRA_LOG
// #define CARLA_WEATHER_EXTRA_LOG
#endif // WITH_EDITOR

class FCarlaModule : public IModuleInterface
{
	
	void AddShaderSearchPaths();
	void RegisterSettings();
	void UnregisterSettings();
	bool HandleSettingsSaved();
	void LoadChronoDll();

public:

	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

};

///////////////////////////////////////////////////////////////////////////////
//////////////////////DReyeVR logging from CarlaUE4.h//////////////////////////
///////////////////////////////////////////////////////////////////////////////

// fancy logging that includes [filename::function:line] prefix
#ifndef __DReyeVR_LOG

/// TODO: remove duplicate code! (Also defined in CarlaUE4.h)
constexpr inline const char *file_name_only(const char *path)
{
    // note since this is a constexpr function, it gets evaluated at compile time rather
    // than runtime so there is no runtime performance overhead!
#ifdef _WIN32
    constexpr char os_sep = '\\';
#else
    constexpr char os_sep = '/';
#endif
    const char *filename_start = path;
    while (*path)
    {
        if (*path++ == os_sep) // keep searching until found last os sep
            filename_start = path;
    }
    return filename_start; // includes extension
}


#define __DReyeVR_LOG(msg, verbosity, ...)                                                                           \
    UE_LOG(LogCarla, verbosity, TEXT("[%s::%s:%d] %s"), UTF8_TO_TCHAR(file_name_only(__FILE__)),                     \
           UTF8_TO_TCHAR(__func__), __LINE__, *FString::Printf(TEXT(msg), ##__VA_ARGS__));
#endif

#define DReyeVR_LOG(msg, ...) __DReyeVR_LOG(msg, Log, ##__VA_ARGS__);
#define DReyeVR_LOG_WARN(msg, ...) __DReyeVR_LOG(msg, Warning, ##__VA_ARGS__);
#define DReyeVR_LOG_ERROR(msg, ...) __DReyeVR_LOG(msg, Error, ##__VA_ARGS__);