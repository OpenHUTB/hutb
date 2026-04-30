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

#include "Chaos/ArrayCollectionArray.h"

#include "EngineUtils.h"
#include "Misc/FileHelper.h"
#include "Utils/URLabLogging.h"
#include "CoreMinimal.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "LandscapeComponent.h"
#include "Utils/MJHelper.h"
#include "CoACD/CoacdInterface.h"
namespace MeshUtils {

struct TotalLandscapeData{

    TArray<float> HeightData;
    TArray<float> HeightDataNormalised;
    MJHelper::HeightFieldData HeightFieldData;
    int NumRows;


};

ALandscape* FindLandscapeActor(UWorld* World);

template <typename VertexType, typename IndexType>
int SaveMeshAsOBJSimple(const FString& FilePath, const Chaos::TArrayCollectionArray<VertexType>& Vertices,
                        const TArray<Chaos::TVector<IndexType, 3>>& Indices) {
    FString OutputString;

    // Write vertices
    for (const auto& Vertex : Vertices) {
        OutputString += FString::Printf(TEXT("v %f %f %f\n"), Vertex.X / 100, -Vertex.Y / 100, Vertex.Z / 100);
    }

    // Write indices
    for (const auto& Index : Indices) {
        OutputString += FString::Printf(TEXT("f %d %d %d\n"), Index.Z + 1, Index.Y + 1, Index.X + 1);
    }

    return FFileHelper::SaveStringToFile(OutputString, *FilePath) ? 1 : 0;
}

template <typename VertexType, typename IndexType>
int SaveMeshAsOBJComplex(const FString& FilePath, const Chaos::TArrayCollectionArray<VertexType>& Vertices,
                         const TArray<Chaos::TVector<IndexType, 3>>& Indices, float Threshold = 0.05f) {
    CoACD_Mesh inputMesh = CoacdInterface::ConvertToCoACDMesh(Vertices, Indices);

    CoACD_MeshArray result = CoACD_run(inputMesh, Threshold, -1, preprocess_auto, 30, 1000, 20, 150, 3, false, true, false, 100,
                                       false, 0.01, apx_ch, 0, false);
    int MeshCount = result.meshes_count;

    UE_LOG(LogURLab, Log, TEXT("CoACD: %d convex hulls created (threshold=%.3f)"), result.meshes_count, Threshold);

    CoacdInterface::SaveCoACDMeshArrayAsOBJ(result, FilePath);
    CoACD_freeMeshArray(result);
    return MeshCount;
}

template <typename VertexType, typename IndexType>
int SaveMesh(const FString& FilePath, const Chaos::TArrayCollectionArray<VertexType>& Vertices,
             const TArray<Chaos::TVector<IndexType, 3>>& Indices, bool ComplexMeshRequired, float Threshold = 0.05f) {
    if (ComplexMeshRequired)
        return MeshUtils::SaveMeshAsOBJComplex(FilePath, Vertices, Indices, Threshold);

    return MeshUtils::SaveMeshAsOBJSimple(FilePath, Vertices, Indices);
}

}

