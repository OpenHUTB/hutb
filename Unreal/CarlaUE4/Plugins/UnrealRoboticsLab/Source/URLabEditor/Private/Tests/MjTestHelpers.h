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
#include "Engine/World.h"
#include "Engine/Blueprint.h"
#include "Engine/SCS_Node.h"
#include "Engine/SimpleConstructionScript.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "MuJoCo/Core/MjPhysicsEngine.h"
#include "UObject/Package.h"
#include "PackageTools.h"
#include "mujoco/mujoco.h"
#include "MuJoCo/Core/AMjManager.h"
#include "MuJoCo/Core/MjArticulation.h"
#include "MuJoCo/Components/MjComponent.h"
#include "MuJoCo/Components/Bodies/MjWorldBody.h"
#include "MuJoCo/Components/Bodies/MjBody.h"
#include "MuJoCo/Components/Geometry/MjGeom.h"
#include "MuJoCo/Components/Joints/MjJoint.h"
#include "MujocoGenerationAction.h"

/**
 * @file MjTestHelpers.h
 * @brief Shared test utilities for all URLab automation tests.
 *
 * Two usage patterns:
 *
 * 1. FMjTestSession — direct MuJoCo (no UE world).
 *    Use ONLY for MjTransformTests (testing the plugin's coordinate-convert
 *    utilities) or as a reference compile to compare against FMjUESession.
 *    Do NOT use for compilation/physics/sensor/actuator tests — those must
 *    go through the plugin's component pipeline.
 *
 * 2. FMjUESession — full Unreal component hierarchy.
 *    Use for ALL tests that verify the plugin's behaviour (component export,
 *    binding, physics integration, sensor readings, actuator control, etc.).
 *    The optional ConfigCallback lets a test customise component properties
 *    before Manager->Compile() is called.
 */

// ---------------------------------------------------------------------------
// FMjTestSession: lightweight RAII wrapper around mjSpec/mjModel/mjData
// ---------------------------------------------------------------------------

/**
 * @struct FMjTestSession
 * @brief Compiles an MJCF XML string directly and provides access to model/data.
 *
 * Usage:
 *   FMjTestSession S;
 *   if (S.CompileXml(TEXT("<mujoco>...</mujoco>"))) {
 *       S.Step(10);
 *       float z = S.d->xpos[2 * 3 + 2];  // body 2 z-position
 *   }
 */
struct FMjTestSession
{
    mjSpec*  spec = nullptr;
    mjModel* m    = nullptr;
    mjData*  d    = nullptr;

    FString LastError;

    /** Compile an inline MJCF XML string. Returns true on success. */
    bool CompileXml(const FString& Xml)
    {
        Cleanup();
        char szErr[1000] = "";
        spec = mj_parseXMLString(TCHAR_TO_UTF8(*Xml), nullptr, szErr, sizeof(szErr));
        if (!spec)
        {
            LastError = FString::Printf(TEXT("mj_parseXMLString failed: %hs"), szErr);
            return false;
        }
        m = mj_compile(spec, nullptr);
        if (!m)
        {
            const char* specErr = mjs_getError(spec);
            LastError = FString::Printf(TEXT("mj_compile failed: %hs"), specErr ? specErr : "unknown");
            return false;
        }
        d = mj_makeData(m);
        if (!d)
        {
            LastError = TEXT("mj_makeData returned null");
            return false;
        }
        return true;
    }

    /** Step physics N times. */
    void Step(int N = 1)
    {
        if (m && d)
            for (int i = 0; i < N; ++i)
                mj_step(m, d);
    }

    /** Forward kinematics without stepping (updates xpos, xquat etc.). */
    void Forward()
    {
        if (m && d) mj_forward(m, d);
    }

    /** Reset simulation to initial state. */
    void Reset()
    {
        if (m && d) mj_resetData(m, d);
    }

    /** Get body ID by name (prefixed or not). Returns -1 if not found. */
    int BodyId(const char* Name) const
    {
        if (!m) return -1;
        return mj_name2id(m, mjOBJ_BODY, Name);
    }

    /** Get geom ID by name. Returns -1 if not found. */
    int GeomId(const char* Name) const
    {
        if (!m) return -1;
        return mj_name2id(m, mjOBJ_GEOM, Name);
    }

    /** Get joint ID by name. Returns -1 if not found. */
    int JointId(const char* Name) const
    {
        if (!m) return -1;
        return mj_name2id(m, mjOBJ_JOINT, Name);
    }

    /** Get sensor ID by name. Returns -1 if not found. */
    int SensorId(const char* Name) const
    {
        if (!m) return -1;
        return mj_name2id(m, mjOBJ_SENSOR, Name);
    }

    /** Get actuator ID by name. Returns -1 if not found. */
    int ActuatorId(const char* Name) const
    {
        if (!m) return -1;
        return mj_name2id(m, mjOBJ_ACTUATOR, Name);
    }

    /** True if model and data are valid. */
    bool IsValid() const { return m != nullptr && d != nullptr; }

    /** Free all MuJoCo resources. */
    void Cleanup()
    {
        if (d)    { mj_deleteData(d);    d    = nullptr; }
        if (m)    { mj_deleteModel(m);   m    = nullptr; }
        if (spec) { mj_deleteSpec(spec); spec = nullptr; }
    }

    ~FMjTestSession() { Cleanup(); }
};

// ---------------------------------------------------------------------------
// FMjUESession: full Unreal Engine world + component hierarchy session
// ---------------------------------------------------------------------------

/**
 * @struct FMjUESession
 * @brief RAII wrapper for tests that need a full UE world with actors.
 *
 * Creates a temporary game world, spawns an AMjManager and one AMjArticulation
 * with a minimal body+geom+joint hierarchy, then drives Compile() directly
 * (bypassing BeginPlay, which does not fire in headless test worlds).
 *
 * Usage:
 *   FMjUESession S;
 *   if (!S.Init()) { AddError(S.LastError); return false; }
 *   TestTrue(TEXT("..."), S.Manager->IsInitialized());
 *   S.Cleanup();
 */
struct FMjUESession
{
    UWorld*           World   = nullptr;
    AAMjManager*      Manager = nullptr;
    AMjArticulation*  Robot   = nullptr;
    UMjBody*          Body    = nullptr;   ///< The single user body (child of WorldBody)
    UMjGeom*          Geom    = nullptr;
    UMjJoint*         Joint   = nullptr;

    FString LastError;

    /**
     * Create world, spawn manager + minimal articulation, optionally configure
     * components, then compile.
     *
     * @param ConfigCallback  Optional lambda called AFTER spawning components
     *                        but BEFORE Manager->Compile().  Use it to set
     *                        component properties specific to a test, e.g.
     *                        Joint->Type = EMjJointType::Slide.
     *
     * @return true on success.
     */
    bool Init(TFunction<void(FMjUESession&)> ConfigCallback = nullptr)
    {
        World = UWorld::CreateWorld(EWorldType::Game, false);
        if (!World) { LastError = TEXT("CreateWorld failed"); return false; }

        FWorldContext& Ctx = GEngine->CreateNewWorldContext(EWorldType::Game);
        Ctx.SetCurrentWorld(World);

        FActorSpawnParameters P;

        Robot = World->SpawnActor<AMjArticulation>(P);
        if (!Robot) { LastError = TEXT("SpawnActor AMjArticulation failed"); return false; }

        UMjWorldBody* WB = NewObject<UMjWorldBody>(Robot, TEXT("WorldBody"));
        Robot->SetRootComponent(WB);
        WB->RegisterComponent();

        Body = NewObject<UMjBody>(Robot, TEXT("RootBody"));
        Body->RegisterComponent();
        Body->AttachToComponent(WB, FAttachmentTransformRules::KeepRelativeTransform);

        Geom = NewObject<UMjGeom>(Robot, TEXT("TestGeom"));
        Geom->Size = FVector(0.1f, 0.1f, 0.1f);
        Geom->bOverride_Size = true;
        Geom->RegisterComponent();
        Geom->AttachToComponent(Body, FAttachmentTransformRules::KeepRelativeTransform);

        Joint = NewObject<UMjJoint>(Robot, TEXT("TestJoint"));
        Joint->RegisterComponent();
        Joint->AttachToComponent(Body, FAttachmentTransformRules::KeepRelativeTransform);

        Manager = World->SpawnActor<AAMjManager>(P);
        if (!Manager) { LastError = TEXT("SpawnActor AAMjManager failed"); return false; }

        // Allow the test to customise component properties before compilation
        if (ConfigCallback) ConfigCallback(*this);

        // Drive compilation directly — BeginPlay is not dispatched in test worlds
        Manager->Compile();

        if (!Manager->IsInitialized())
        {
            LastError = TEXT("Manager->Compile() failed — model not initialized");
            return false;
        }
        return true;
    }

    /** Step physics N times on the manager's model/data. */
    void Step(int N = 1)
    {
        if (Manager && Manager->PhysicsEngine->m_model && Manager->PhysicsEngine->m_data)
            for (int i = 0; i < N; ++i)
                mj_step(Manager->PhysicsEngine->m_model, Manager->PhysicsEngine->m_data);
    }

    /** Free world and engine context. */
    void Cleanup()
    {
        if (Manager) Manager->PhysicsEngine->bShouldStopTask = true;
        if (World)
        {
            World->DestroyWorld(false);
            GEngine->DestroyWorldContext(World);
            World   = nullptr;
            Manager = nullptr;
            Robot   = nullptr;
        }
    }

    ~FMjUESession() { Cleanup(); }
};

// ---------------------------------------------------------------------------
// FMjXmlImportSession: full XML→Blueprint import + optional compile
// ---------------------------------------------------------------------------

/**
 * @struct FMjXmlImportSession
 * @brief Writes an inline MJCF XML string to a temp file, runs it through
 *        UMujocoGenerationAction, and provides access to the Blueprint's
 *        component templates for inspection.  Optionally spawns a world and
 *        compiles the model so mjModel values can also be checked.
 *
 * Two-tier usage:
 *
 *   // Tier 1 — inspect importer output (UE component properties):
 *   FMjXmlImportSession S;
 *   if (!S.Init(TEXT("<mujoco>...</mujoco>"))) { ... }
 *   UMjGeom* G = S.FindTemplate<UMjGeom>(TEXT("mygeom"));
 *   TestEqual("friction", G->Friction.X, 0.8f);
 *   S.Cleanup();
 *
 *   // Tier 2 — also verify compiled mjModel values:
 *   if (!S.Compile()) { ... }
 *   TestEqual("ngeom", S.Model()->ngeom, 2);
 *   S.Cleanup();
 */
struct FMjXmlImportSession
{
    UBlueprint*       Blueprint = nullptr;
    UWorld*           World     = nullptr;
    AAMjManager*      Manager   = nullptr;
    AMjArticulation*  Robot     = nullptr;

    FString LastError;
    FString TempXmlPath;

    /**
     * Write @p XmlContent to a temp file and run UMujocoGenerationAction.
     * After this call FindTemplate<T>() is valid.
     * Call Compile() afterwards if you also need mjModel values.
     *
     * @return true on success.
     */
    /**
     * Load from an existing XML file on disk (no temp copy).
     * Use for testing real Menagerie models with mesh references.
     */
    bool InitFromFile(const FString& XmlFilePath)
    {
        if (!FPaths::FileExists(XmlFilePath))
        {
            LastError = FString::Printf(TEXT("File not found: %s"), *XmlFilePath);
            return false;
        }
        TempXmlPath = XmlFilePath;
        bOwnsTempFile = false;

        // Quick MuJoCo validation
        {
            char szErr[1000] = "";
            mjModel* TmpM = mj_loadXML(TCHAR_TO_UTF8(*TempXmlPath), nullptr, szErr, sizeof(szErr));
            if (!TmpM)
            {
                LastError = FString::Printf(TEXT("MuJoCo rejected XML: %hs"), szErr);
                return false;
            }
            NativeBodyCount = TmpM->nbody;
            NativeJointCount = TmpM->njnt;
            NativeActuatorCount = TmpM->nu;
            NativeGeomCount = TmpM->ngeom;
            mj_deleteModel(TmpM);
        }

        // Create Blueprint and run generator. Both the package path AND the
        // blueprint class name are unique-per-session — hardcoding the class
        // name caused intermittent Kismet2.cpp:424 FindObject-uniqueness
        // assertion failures when GC hadn't collected the previous test's
        // blueprint-generated class before the next test ran.
        const FString UniqueSfx = FGuid::NewGuid().ToString(EGuidFormats::Digits).Left(12);
        FString PkgName = UPackageTools::SanitizePackageName(
            FString(TEXT("/Temp/URLabImportTest_")) + UniqueSfx);
        UPackage* Pkg = CreatePackage(*PkgName);

        const FString BPName = FString(TEXT("ImportTestArt_")) + UniqueSfx;
        Blueprint = FKismetEditorUtilities::CreateBlueprint(
            AMjArticulation::StaticClass(), Pkg, *BPName,
            BPTYPE_Normal, UBlueprint::StaticClass(), UBlueprintGeneratedClass::StaticClass());

        if (!Blueprint)
        {
            LastError = TEXT("CreateBlueprint failed");
            return false;
        }

        if (AMjArticulation* CDO = Cast<AMjArticulation>(Blueprint->GeneratedClass->GetDefaultObject()))
            CDO->MuJoCoXMLFile.FilePath = TempXmlPath;

        UMujocoGenerationAction* Gen = NewObject<UMujocoGenerationAction>();
        Gen->GenerateForBlueprint(Blueprint, TempXmlPath);

        return true;
    }

    // Native MuJoCo counts (populated by InitFromFile for comparison)
    int32 NativeBodyCount = 0;
    int32 NativeJointCount = 0;
    int32 NativeActuatorCount = 0;
    int32 NativeGeomCount = 0;
    bool bOwnsTempFile = true;

    bool Init(const FString& XmlContent)
    {
        bOwnsTempFile = true;
        // 1. Write temp XML
        FString TempDir = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("URLab/Tests"));
        IFileManager::Get().MakeDirectory(*TempDir, true);
        TempXmlPath = FPaths::Combine(TempDir,
            FString::Printf(TEXT("import_%d.xml"), FMath::RandRange(0, 999999)));

        if (!FFileHelper::SaveStringToFile(XmlContent, *TempXmlPath))
        {
            LastError = TEXT("Failed to write temp XML");
            return false;
        }

        // 2. Quick MuJoCo validation (no UE objects yet)
        {
            char szErr[1000] = "";
            mjModel* TmpM = mj_loadXML(TCHAR_TO_UTF8(*TempXmlPath), nullptr, szErr, sizeof(szErr));
            if (!TmpM)
            {
                LastError = FString::Printf(TEXT("MuJoCo rejected XML: %hs"), szErr);
                IFileManager::Get().Delete(*TempXmlPath);
                TempXmlPath.Empty();
                return false;
            }
            mj_deleteModel(TmpM);
        }

        // 3. Create in-memory Blueprint. Both the package path AND the
        //    blueprint class name are unique-per-session (GUID suffix). See
        //    InitFromFile above for the rationale — hardcoded class names
        //    caused intermittent Kismet2.cpp:424 uniqueness assertions.
        const FString UniqueSfx = FGuid::NewGuid().ToString(EGuidFormats::Digits).Left(12);
        FString PkgName = UPackageTools::SanitizePackageName(
            FString(TEXT("/Temp/URLabImportTest_")) + UniqueSfx);
        UPackage* Pkg = CreatePackage(*PkgName);

        const FString BPName = FString(TEXT("ImportTestArt_")) + UniqueSfx;
        Blueprint = FKismetEditorUtilities::CreateBlueprint(
            AMjArticulation::StaticClass(), Pkg, *BPName,
            BPTYPE_Normal, UBlueprint::StaticClass(), UBlueprintGeneratedClass::StaticClass());

        if (!Blueprint)
        {
            LastError = TEXT("FKismetEditorUtilities::CreateBlueprint failed");
            return false;
        }

        // 4. Set XML path on CDO so options merge works
        if (AMjArticulation* CDO = Cast<AMjArticulation>(Blueprint->GeneratedClass->GetDefaultObject()))
            CDO->MuJoCoXMLFile.FilePath = TempXmlPath;

        // 5. Run the generator (no-editor calls like texture import are no-ops in headless)
        UMujocoGenerationAction* Gen = NewObject<UMujocoGenerationAction>();
        Gen->GenerateForBlueprint(Blueprint, TempXmlPath);

        return true;
    }

    /**
     * Spawn a test world, create a Manager, spawn the imported Blueprint
     * actor and call Manager->Compile().  Requires Init() to have succeeded.
     *
     * @return true if compile succeeded.
     */
    bool Compile()
    {
        if (!Blueprint)
        {
            LastError = TEXT("Call Init() before Compile()");
            return false;
        }

        World = UWorld::CreateWorld(EWorldType::Game, false);
        if (!World) { LastError = TEXT("CreateWorld failed"); return false; }

        FWorldContext& Ctx = GEngine->CreateNewWorldContext(EWorldType::Game);
        Ctx.SetCurrentWorld(World);

        FActorSpawnParameters P;
        Manager = World->SpawnActor<AAMjManager>(P);
        if (!Manager) { LastError = TEXT("SpawnActor AAMjManager failed"); return false; }

        // Spawn the Blueprint-generated articulation class
        Robot = World->SpawnActor<AMjArticulation>(Blueprint->GeneratedClass, FVector::ZeroVector, FRotator::ZeroRotator, P);
        // Robot may be nullptr for models with no worldbody — that's OK

        Manager->Compile();
        if (!Manager->IsInitialized())
        {
            LastError = TEXT("Manager->Compile() failed — model not initialized");
            return false;
        }
        return true;
    }

    /** Access the compiled mjModel (only valid after Compile()). */
    mjModel* Model() const { return Manager ? Manager->PhysicsEngine->m_model : nullptr; }
    mjData*  Data()  const { return Manager ? Manager->PhysicsEngine->m_data  : nullptr; }

    /**
     * Find a component template in the Blueprint's SCS nodes by MjName or
     * variable name.  Returns the first match of type T, or nullptr.
     *
     * @tparam T  USceneComponent subclass to search for.
     * @param Name  MjName or SCS variable name to match.
     */
    template<typename T>
    T* FindTemplate(const FString& Name) const
    {
        if (!Blueprint || !Blueprint->SimpleConstructionScript) return nullptr;
        for (USCS_Node* Node : Blueprint->SimpleConstructionScript->GetAllNodes())
        {
            T* Tmpl = Cast<T>(Node->ComponentTemplate);
            if (!Tmpl) continue;
            if (UMjComponent* MjC = Cast<UMjComponent>(Tmpl))
            {
                if (MjC->MjName == Name) return Tmpl;
            }
            if (Node->GetVariableName().ToString() == Name) return Tmpl;
        }
        return nullptr;
    }

    /**
     * Find ANY component template of type T (returns the first one found).
     * Useful when the test only creates one of that type.
     */
    template<typename T>
    T* FindFirstTemplate() const
    {
        if (!Blueprint || !Blueprint->SimpleConstructionScript) return nullptr;
        for (USCS_Node* Node : Blueprint->SimpleConstructionScript->GetAllNodes())
        {
            if (T* Tmpl = Cast<T>(Node->ComponentTemplate)) return Tmpl;
        }
        return nullptr;
    }

    /** Count how many SCS nodes have component templates of type T. */
    template<typename T>
    int32 CountTemplates() const
    {
        int32 N = 0;
        if (!Blueprint || !Blueprint->SimpleConstructionScript) return 0;
        for (USCS_Node* Node : Blueprint->SimpleConstructionScript->GetAllNodes())
        {
            if (Cast<T>(Node->ComponentTemplate)) ++N;
        }
        return N;
    }

    void Cleanup()
    {
        if (Manager) Manager->PhysicsEngine->bShouldStopTask = true;
        if (World)
        {
            World->DestroyWorld(false);
            GEngine->DestroyWorldContext(World);
            World   = nullptr;
            Manager = nullptr;
            Robot   = nullptr;
        }
        if (!TempXmlPath.IsEmpty() && bOwnsTempFile)
        {
            IFileManager::Get().Delete(*TempXmlPath);
        }
        TempXmlPath.Empty();
        Blueprint = nullptr;
    }

    ~FMjXmlImportSession() { Cleanup(); }
};

// ---------------------------------------------------------------------------
// Comparison helpers
// ---------------------------------------------------------------------------

namespace MjTestMath
{
    inline bool NearlyEqual(float A, float B, float Eps = 1e-4f)
    {
        return FMath::Abs(A - B) <= Eps;
    }

    inline bool NearlyEqual(double A, double B, double Eps = 1e-4)
    {
        return FMath::Abs(A - B) <= Eps;
    }

    inline bool NearlyEqual(const FVector& A, const FVector& B, float Eps = 0.1f)
    {
        return A.Equals(B, Eps);
    }

    inline bool NearlyEqual(const FQuat& A, const FQuat& B, float Eps = 0.01f)
    {
        // Quats q and -q represent the same rotation
        return A.Equals(B, Eps) || A.Equals(B.Inverse() * FQuat(0, 0, 0, -1) /* flip */ , Eps)
            || FQuat::ErrorAutoNormalize(A, B) < Eps;
    }
}
