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
#include "Components/SceneComponent.h"
#include <mujoco/mjspec.h>
#include <mujoco/mujoco.h>
#include "MuJoCo/Core/Spec/MjSpecElement.h"
#include "MuJoCo/Utils/MjBind.h"
#include "Utils/URLabLogging.h"
#include "Serialization/BufferArchive.h"
class UMjDefault;

#include "MjComponent.generated.h"

/**
 * @class UMjComponent
 * @brief Base class for all MuJoCo-related components in Unreal Engine.
 * 
 * Provides centralized logic for:
 * 1. Spec Element registration and tracking.
 * 2. Runtime binding to MuJoCo data (mjModel/mjData) via ID or Name.
 * 3. Common properties and accessors.
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class URLAB_API UMjComponent : public USceneComponent, public IMjSpecElement
{
	GENERATED_BODY()

public:	
	UMjComponent();

protected:
	virtual void BeginPlay() override;

public:	
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

    // --- IMjSpecElement Interface ---
    /** 
     * @brief Registers this component to the MuJoCo spec. 
     * Subclasses must implement this to create their specific mjsElement.
     */
    virtual void RegisterToSpec(class FMujocoSpecWrapper& Wrapper, mjsBody* ParentBody = nullptr) override;

    /**
     * @brief Binds this component to the runtime MuJoCo simulation.
     * Base implementation caches Model/Data and attempts to resolve ID.
     */
    virtual void Bind(mjModel* model, mjData* data, const FString& Prefix = TEXT("")) override;

    /** @brief Serializes the component's runtime state into a binary buffer for network transmission. */
    virtual void BuildBinaryPayload(FBufferArchive& OutBuffer) const {}

    /** @brief Gets the topic name used for broadcasting this component's data. */
    virtual FString GetTelemetryTopicName() const { return FString(); }

    /** @brief Returns the number of objects of a given type in the compiled model. */
    static int GetMjObjectCount(const mjModel* M, mjtObj ObjType)
    {
        switch (ObjType)
        {
            case mjOBJ_BODY:     return M->nbody;
            case mjOBJ_JOINT:    return M->njnt;
            case mjOBJ_GEOM:     return M->ngeom;
            case mjOBJ_SITE:     return M->nsite;
            case mjOBJ_ACTUATOR: return M->nu;
            case mjOBJ_SENSOR:   return M->nsensor;
            case mjOBJ_TENDON:   return M->ntendon;
            default:             return 0;
        }
    }

    /**
     * @brief Helper to create a specific View struct for this component.
     * Uses generic ID binding if possible, falling back to name.
     */
    template <typename ViewType>
    ViewType BindToView(const FString& Prefix)
    {
        if (!m_Model || !m_Data)
        {
            UE_LOG(LogURLabBind, Warning, TEXT("[BindToView] '%s' — m_Model or m_Data is null, cannot bind."), *GetName());
            return ViewType();
        }

        // 1. Try ID based binding via spec element
        // NOTE: mjs_getId can return stale/garbage IDs after mjs_attach merges
        // a child spec into the root. We validate the ID against model bounds
        // before using it.
        if (m_SpecElement)
        {
            int id = mjs_getId(m_SpecElement);
            int maxCount = GetMjObjectCount(m_Model, ViewType::obj_type);
            if (id >= 0 && id < maxCount)
            {
                UE_LOG(LogURLabBind, Verbose, TEXT("[BindToView] '%s' — Path 1 SUCCESS: mjs_getId returned %d (max=%d, SpecElement=%p)"), *GetName(), id, maxCount, m_SpecElement);
                return ViewType(m_Model, m_Data, id);
            }
            else if (id >= 0)
            {
                UE_LOG(LogURLabBind, Warning, TEXT("[BindToView] '%s' — Path 1 REJECTED: mjs_getId returned %d but max for type %d is %d (stale spec element). Falling back to name lookup."), *GetName(), id, ViewType::obj_type, maxCount);
            }
            else
            {
                UE_LOG(LogURLabBind, Warning, TEXT("[BindToView] '%s' — Path 1 FAILED: mjs_getId returned -1 (SpecElement=%p). Falling back to name lookup."), *GetName(), m_SpecElement);
            }
        }
        else
        {
            UE_LOG(LogURLabBind, Warning, TEXT("[BindToView] '%s' — Path 1 SKIPPED: m_SpecElement is null. Falling back to name lookup."), *GetName());
        }

        // 2. Fallback: Name based binding
        FString NameToLookup = MjName.IsEmpty() ? GetName() : MjName;
        FString PrefixedName = Prefix + NameToLookup;
        int id = mj_name2id(m_Model, ViewType::obj_type, TCHAR_TO_UTF8(*PrefixedName));
        if (id >= 0)
        {
            UE_LOG(LogURLabBind, Verbose, TEXT("[BindToView] '%s' — Path 2 SUCCESS: mj_name2id('%s', type=%d) returned %d"), *GetName(), *PrefixedName, ViewType::obj_type, id);
            return ViewType(m_Model, m_Data, id);
        }
        else
        {
            UE_LOG(LogURLabBind, Warning, TEXT("[BindToView] '%s' — Path 2 FAILED: mj_name2id('%s', type=%d) returned -1. MjName='%s', Prefix='%s', UEName='%s'"),
                *GetName(), *PrefixedName, ViewType::obj_type, *MjName, *Prefix, *GetName());
        }

        return ViewType();
    }

    /** @brief Checks if the component is successfully bound to MuJoCo runtime. */
    UFUNCTION(BlueprintCallable, Category = "MuJoCo|Base")
    bool IsBound() const;

    /** @brief Returns the MuJoCo ID of this component. Returns -1 if not bound. */
    UFUNCTION(BlueprintCallable, Category = "MuJoCo|Base")
    int GetMjID() const { return m_ID; }

    /** @brief If true, this component is a template in a <default> block. Auto-resolved in Setup(). */
    UPROPERTY()
    bool bIsDefault = false;

    /** @brief Gets the full prefixed name of this component as it appears in the compiled MuJoCo model. */
    UFUNCTION(BlueprintCallable, Category = "MuJoCo|Base")
    virtual FString GetMjName() const;

    /** @brief The original name of this element in the MuJoCo Spec (from XML). */
    UPROPERTY()
    FString MjName;

#if WITH_EDITOR
    /** Returns names of sibling components of the given class in the same Blueprint SCS tree.
     *  Static so non-UMjComponent classes (ContactPair, etc.) can also use it. */
    static TArray<FString> GetSiblingComponentOptions(const UObject* CallerComponent, UClass* FilterClass, bool bIncludeDefaults = false);
#endif

protected:
    /** Resolves the MuJoCo default class for a given class name.
     * Looks up ClassName; falls back to the spec's global default. */
    static mjsDefault* ResolveDefault(mjSpec* Spec, const FString& ClassName);

    // --- Editor-time default resolution ---

    /**
     * Finds the effective UMjDefault for this component at editor time (no mjSpec needed).
     * Checks the component's MjClassName first (subclass must expose it).
     * Falls back to walking up attachment parents to find the nearest UMjBody
     * with ChildClassName set. Returns nullptr if no default class applies.
     */
    UMjDefault* FindEditorDefault() const;

    /**
     * Returns this component's MjClassName for editor-time default resolution.
     * Base returns empty string. Subclasses with MjClassName override this.
     */
    virtual FString GetMjClassName() const { return FString(); }

    /** Finds a UMjDefault component by ClassName on the owning actor. */
    static UMjDefault* FindDefaultByClassName(const AActor* Owner, const FString& ClassName);

    /** Sets the spec element name using GetUniqueName for deduplication. */
    void SetSpecElementName(class FMujocoSpecWrapper& Wrapper, mjsElement* Elem, mjtObj ObjType);

    /**Pointer to the MuJoCo spec element created during RegisterToSpec. */
    mjsElement* m_SpecElement = nullptr;

    /** Generic ID of this element in the compiled model (body_id, joint_id, etc.). */
    int m_ID = -1;

    mjModel* m_Model = nullptr;
    mjData* m_Data = nullptr;
};
