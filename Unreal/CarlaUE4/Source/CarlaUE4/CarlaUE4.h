// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Engine.h"

#include "Util/NonCopyable.h"

DECLARE_LOG_CATEGORY_EXTERN(LogDReyeVR, Log, All);
DECLARE_LOG_CATEGORY_EXTERN(LogHolodeck, Log, All);

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

// fancy logging that includes [filename::function:line] prefix
#define __DReyeVR_LOG(msg, verbosity, ...)                                                                             \
    UE_LOG(LogDReyeVR, verbosity, TEXT("[%s::%s:%d] %s"), UTF8_TO_TCHAR(file_name_only(__FILE__)),                     \
           UTF8_TO_TCHAR(__func__), __LINE__, *FString::Printf(TEXT(msg), ##__VA_ARGS__));

#define LOG(msg, ...) __DReyeVR_LOG(msg, Log, ##__VA_ARGS__);
#define LOG_WARN(msg, ...) __DReyeVR_LOG(msg, Warning, ##__VA_ARGS__);
#define LOG_ERROR(msg, ...) __DReyeVR_LOG(msg, Error, ##__VA_ARGS__);
