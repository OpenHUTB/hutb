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

#include <mujoco/mjspec.h>

#include "CoreMinimal.h"
#include "MuJoCo/Components/MjComponent.h"
#include "MjDefault.generated.h"

// Forward declarations
class UMjGeom;
class UMjJoint;
class UMjActuator;
class UMjTendon;
class UMjSite;
class UMjCamera;

/**
 * @class UMjDefault
 * @brief Component that defines a MuJoCo <default> class.
 * 
 * Allows setting shared properties for nested bodies, joints, geoms, etc.
 * Any component specifying this ClassName can inherit these values.
 * 
 * Now inherits from UMjComponent to ensure consistency and bIsDefault support.
 * Child components (UMjGeom, UMjJoint, etc.) attached to this component define the default values.
 */
UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class URLAB_API UMjDefault : public UMjComponent
{
	GENERATED_BODY()

public:	
    /** @brief Default constructor. */
	UMjDefault();

    /** @brief Name of the default class. If empty, these defaults apply to the global scope. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MuJoCo|Default")
    FString ClassName;

    /** @brief Name of the parent default class for inheritance. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MuJoCo|Default")
    FString ParentClassName;

    /**
     * @brief Imports default settings from a MuJoCo XML <default> node.
     * @param Node Pointer to the FXmlNode representing the <default> block.
     * Note: This function now primarily sets the class name. Nested components are created by the importer.
     */
    void ImportFromXml(const class FXmlNode* Node);

#if WITH_EDITOR
    virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

    /**
     * @brief Exports default settings to a MuJoCo mjsDefault structure.
     * @param def Pointer to the mjsDefault structure to populate.
     */
    void ExportTo(mjsDefault* def);

    /**
     * @brief Finds the first direct child component of type T.
     * Used by editor-time default resolution to read property values from
     * the template components attached to this default class.
     */
    template<typename T>
    T* FindChildOfType() const
    {
        TArray<USceneComponent*> Children;
        const_cast<UMjDefault*>(this)->GetChildrenComponents(false, Children);
        for (USceneComponent* Child : Children)
        {
            if (T* Typed = Cast<T>(Child)) return Typed;
        }
        return nullptr;
    }

};
