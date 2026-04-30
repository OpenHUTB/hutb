// Copyright (c) 2026 Jonathan Embley-Riches. All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// --- LEGAL DISCLAIMER ---
// UnrealRoboticsLab is an independent software plugin. It is NOT affiliated with, 
// endorsed by, or sponsored by Epic Games, Inc. "Unreal" and "Unreal Engine" are 
// trademarks or registered trademarks of Epic Games, Inc. in the US and elsewhere.
//
// This plugin incorporates third-party software: MuJoCo (Apache 2.0), 
// CoACD (MIT), and libzmq (MPL 2.0). See ThirdPartyNotices.txt for details.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/SecureHash.h"
#include <functional>
namespace IO {

/** Compute an MD5 hash string from raw vertex + index data. */
template<typename VertexType, typename IndexType>
inline FString ComputeMeshHash(const TArray<VertexType>& Vertices, const TArray<IndexType>& Indices)
{
    FMD5 Md5;
    if (Vertices.Num() > 0)
    {
        Md5.Update((const uint8*)Vertices.GetData(), Vertices.Num() * sizeof(VertexType));
    }
    if (Indices.Num() > 0)
    {
        Md5.Update((const uint8*)Indices.GetData(), Indices.Num() * sizeof(IndexType));
    }
    FMD5Hash Hash;
    Hash.Set(Md5);
    return LexToString(Hash);
}

/** Save a hash string to a .hash file alongside the OBJ. */
inline void SaveMeshHash(const FString& ObjFilePath, const FString& Hash)
{
    FString HashPath = FPaths::ChangeExtension(ObjFilePath, TEXT("hash"));
    FFileHelper::SaveStringToFile(Hash, *HashPath);
}

/** Load a previously saved hash. Returns empty string if not found. */
inline FString LoadMeshHash(const FString& ObjFilePath)
{
    FString HashPath = FPaths::ChangeExtension(ObjFilePath, TEXT("hash"));
    FString Hash;
    if (FFileHelper::LoadFileToString(Hash, *HashPath))
    {
        return Hash.TrimStartAndEnd();
    }
    return FString();
}

/** Delete cached OBJ files (and hash) so they get re-exported. */
inline void DeleteMeshCache(const FString& BaseFilePath, bool bComplex)
{
    // Delete hash file
    IFileManager::Get().Delete(*FPaths::ChangeExtension(BaseFilePath, TEXT("hash")));

    if (!bComplex)
    {
        IFileManager::Get().Delete(*BaseFilePath);
    }
    else
    {
        FString BaseFileName = FPaths::GetBaseFilename(BaseFilePath);
        FString Directory = FPaths::GetPath(BaseFilePath);
        FString WildcardPattern = FString::Printf(TEXT("%s/%s_sub_*.obj"), *Directory, *BaseFileName);
        TArray<FString> FoundFiles;
        IFileManager::Get().FindFiles(FoundFiles, *WildcardPattern, true, false);
        for (const FString& File : FoundFiles)
        {
            IFileManager::Get().Delete(*FString::Printf(TEXT("%s/%s"), *Directory, *File));
        }
        // Also delete sub hash files
        WildcardPattern = FString::Printf(TEXT("%s/%s_sub_*.hash"), *Directory, *BaseFileName);
        IFileManager::Get().FindFiles(FoundFiles, *WildcardPattern, true, false);
        for (const FString& File : FoundFiles)
        {
            IFileManager::Get().Delete(*FString::Printf(TEXT("%s/%s"), *Directory, *File));
        }
    }
}

inline int NumFilesExist(const FString BaseFilePath, const bool ComplexMesh) {

    if (FPaths::FileExists(BaseFilePath))
        return 1;

    if (ComplexMesh) {

        FString BaseFileName = FPaths::GetBaseFilename(BaseFilePath);
        FString WildcardPattern = FString::Printf(TEXT("%s_sub_*.obj"), *BaseFileName);
        FString Directory = FPaths::GetPath(BaseFilePath);

        TArray<FString> FoundFiles;
        IFileManager::Get().FindFiles(FoundFiles, *Directory, *WildcardPattern);

        int32 SubMeshCount = FoundFiles.Num();
        return SubMeshCount;
    }

    // files not found
    return 0;
}

} // namespace IO
