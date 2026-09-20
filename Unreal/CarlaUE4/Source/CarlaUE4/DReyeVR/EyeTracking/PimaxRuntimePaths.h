#pragma once

#include "CoreMinimal.h"
#include "Misc/Paths.h"

namespace PimaxRuntimePaths
{
// Only explicit local absolute paths; never search the working directory or PATH.
inline bool IsAbsoluteLocalPath(const FString &Path)
{
    return Path.Len() > 2 && FChar::IsAlpha(Path[0]) && Path[1] == TCHAR(':') &&
           (Path[2] == TCHAR('/') || Path[2] == TCHAR('\\'));
}

inline TArray<FString> Build(const FString &Override, const FString &SystemDirectory,
                             const TArray<FString> &InstallRoots)
{
    TArray<FString> Paths;
    auto Add = [&Paths](FString Path) {
        if (IsAbsoluteLocalPath(Path))
        {
            FPaths::NormalizeFilename(Path);
            Paths.AddUnique(Path);
        }
    };
    if (!Override.IsEmpty())
    {
        Add(Override); // An explicit override must not silently fall back to another version.
        return Paths;
    }
    if (!SystemDirectory.IsEmpty())
        Add(FPaths::Combine(SystemDirectory, TEXT("libPVRClient64.dll")));
    for (const FString &Root : InstallRoots)
    {
        if (!IsAbsoluteLocalPath(Root))
            continue;
        Add(FPaths::Combine(Root, TEXT("Runtime/libPVRClient64.dll")));
        Add(FPaths::Combine(Root, TEXT("libPVRClient64.dll")));
    }
    return Paths;
}
} // namespace PimaxRuntimePaths
