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

#include "CoACD/CoacdInterface.h"
#include "Utils/URLabLogging.h"
void CoacdInterface::SaveCoACDMeshAsOBJ(CoACD_Mesh& Mesh, const FString& FilePath) {
    FString OutputString;

    // Write vertices
    for (uint64_t i = 0; i < Mesh.vertices_count; ++i) {
        double X = Mesh.vertices_ptr[3 * i];
        double Y = Mesh.vertices_ptr[3 * i + 1];
        double Z = Mesh.vertices_ptr[3 * i + 2];

        // Adjust Unreal Engine coordinates and scale (divide by 100)
        OutputString += FString::Printf(TEXT("v %f %f %f\n"), X / 100.0, -Y / 100.0, Z / 100.0);
    }

    // Write faces (indices are 1-based in OBJ format)
    for (uint64_t i = 0; i < Mesh.triangles_count; ++i) {
        int32 A = Mesh.triangles_ptr[3 * i] + 1;
        int32 B = Mesh.triangles_ptr[3 * i + 1] + 1;
        int32 C = Mesh.triangles_ptr[3 * i + 2] + 1;

        OutputString += FString::Printf(TEXT("f %d %d %d\n"), C, B, A);
    }

    // Save the OBJ string to a file
    if (FFileHelper::SaveStringToFile(OutputString, *FilePath)) {
        UE_LOG(LogURLab, Log, TEXT("Successfully saved mesh to %s"), *FilePath);
    } else {
        UE_LOG(LogURLab, Error, TEXT("Failed to save mesh to %s"), *FilePath);
    }
}

void CoacdInterface::SaveCoACDMeshArrayAsOBJ(CoACD_MeshArray& MeshArray, const FString& BaseFilePath) {
    FString BaseName = FPaths::GetBaseFilename(BaseFilePath);
    FString Directory = FPaths::GetPath(BaseFilePath);

    for (int32 i = 0; i < MeshArray.meshes_count; ++i) {
        // Construct the new file path with "_sub_n.obj" format
        FString FilePath = FString::Printf(TEXT("%s/%s_sub_%d.obj"), *Directory, *BaseName, i);
        SaveCoACDMeshAsOBJ(MeshArray.meshes_ptr[i], FilePath);
    }
}
