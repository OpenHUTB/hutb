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
#include <functional>
#include <mujoco/mujoco.h>
#include "Chaos/ArrayCollectionArray.h"
#include "Coacd/coacd.h"

#include "Misc/FileHelper.h"
namespace CoacdInterface {
   

template <typename VertexType, typename IndexType>
CoACD_Mesh ConvertToCoACDMesh(const Chaos::TArrayCollectionArray<VertexType>& Vertices, const TArray<Chaos::TVector<IndexType, 3>>& Indices) {

    CoACD_Mesh coacdMesh;

    // Allocate memory for vertices
    coacdMesh.vertices_count = Vertices.Num();
    coacdMesh.vertices_ptr = new double[coacdMesh.vertices_count * 3];

    // Copy vertex data (FVector to double array)
    for (int i = 0; i < Vertices.Num(); ++i) {
        coacdMesh.vertices_ptr[3 * i] = Vertices[i].X;
        coacdMesh.vertices_ptr[3 * i + 1] = Vertices[i].Y;
        coacdMesh.vertices_ptr[3 * i + 2] = Vertices[i].Z;
    }

    // Allocate memory for indices
    coacdMesh.triangles_count = Indices.Num();
    coacdMesh.triangles_ptr = new int[coacdMesh.triangles_count * 3];

    // Copy index data (int32 array)
    for (int i = 0; i < Indices.Num(); ++i) {
        coacdMesh.triangles_ptr[3 * i] = Indices[i][0];
        coacdMesh.triangles_ptr[3 * i + 1] = Indices[i][1];
        coacdMesh.triangles_ptr[3 * i + 2] = Indices[i][2];
    }

    return coacdMesh;
}
void SaveCoACDMeshAsOBJ(CoACD_Mesh& Mesh, const FString& FilePath) ;

void SaveCoACDMeshArrayAsOBJ(CoACD_MeshArray& MeshArray, const FString& BaseFilePath) ;
}
