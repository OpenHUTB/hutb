#pragma once

#include "CoreMinimal.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

namespace DrivingStudy
{
// No option preserves the original input behaviour for existing launchers.
inline FString InputMode(const TCHAR *CommandLine = FCommandLine::Get())
{
    FString Mode;
    FParse::Value(CommandLine, TEXT("DReyeVRInput="), Mode);
    return Mode == TEXT("wheel") || Mode == TEXT("controller") ? Mode : TEXT("legacy");
}
} // namespace DrivingStudy
