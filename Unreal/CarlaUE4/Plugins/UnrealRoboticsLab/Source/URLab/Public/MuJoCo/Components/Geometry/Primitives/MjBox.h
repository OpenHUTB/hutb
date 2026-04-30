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
#include "MuJoCo/Components/Geometry/MjGeom.h"
#include "MjBox.generated.h"

/**
 * @class UMjBox
 * @brief Box primitive geometry.
 */
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class URLAB_API UMjBox : public UMjGeom
{
	GENERATED_BODY()

public:
	UMjBox();
	virtual void OnRegister() override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MuJoCo|Primitive")
	FVector Extents = FVector(0.0f);

    /** @brief Internal-only visual mesh for the editor. */
    class UStaticMeshComponent* VisualizerMesh = nullptr;


	virtual void ImportFromXml(const class FXmlNode* Node) override;
	virtual void ImportFromXml(const class FXmlNode* Node, const struct FMjCompilerSettings& CompilerSettings) override;
	virtual void ExportTo(mjsGeom* geom, mjsDefault* def = nullptr) override;

    virtual void SyncUnrealTransformFromMj() override;
    virtual void SetGeomVisibility(bool bNewVisibility) override;

	virtual class UStaticMeshComponent* GetVisualizerMesh() const override { const_cast<UMjBox*>(this)->EnsureVisualizerMesh(); return VisualizerMesh; }
	virtual void ApplyOverrideMaterial(class UMaterialInterface* Material) override;
	void EnsureVisualizerMesh();
};
