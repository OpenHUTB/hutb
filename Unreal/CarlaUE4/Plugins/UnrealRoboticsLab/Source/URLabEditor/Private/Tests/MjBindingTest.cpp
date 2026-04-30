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

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "Tests/AutomationEditorCommon.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "MuJoCo/Components/Bodies/MjBody.h"
#include "MuJoCo/Components/Bodies/MjWorldBody.h"
#include "MuJoCo/Components/Geometry/MjGeom.h"
#include "MuJoCo/Components/Joints/MjJoint.h"
#include "MuJoCo/Core/AMjManager.h"
#include "MuJoCo/Core/MjPhysicsEngine.h"
#include "MuJoCo/Core/MjArticulation.h"

// Define a test with flags to run in Editor
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMjBindingIntegrationTest, "URLab.MuJoCo.Binding.Integration", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMjBindingIntegrationTest::RunTest(const FString& Parameters)
{
    // 1. Create a Temporary World
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
    if (!World)
    {
        AddError(TEXT("Failed to create temporary world."));
        return false;
    }
    FWorldContext& WorldContext = GEngine->CreateNewWorldContext(EWorldType::Game);
    WorldContext.SetCurrentWorld(World);

    // 2. Spawn Articulation (Robot)
    FActorSpawnParameters SpawnParams;
    AMjArticulation* Robot = World->SpawnActor<AMjArticulation>(SpawnParams);
    if (!Robot)
    {
        AddError(TEXT("Failed to spawn AMjArticulation."));
        World->DestroyWorld(false);
        GEngine->DestroyWorldContext(World);
        return false;
    }

    // 3. Construct Component Hierarchy: WorldBody -> Body -> { Geom, Joint }
    //    AMjArticulation::Setup() requires a UMjWorldBody as root, with UMjBody children.
    UMjWorldBody* WorldBody = NewObject<UMjWorldBody>(Robot, TEXT("WorldBody"));
    Robot->SetRootComponent(WorldBody);
    WorldBody->RegisterComponent();

    UMjBody* RootBody = NewObject<UMjBody>(Robot, TEXT("RootBody"));
    RootBody->RegisterComponent();
    RootBody->AttachToComponent(WorldBody, FAttachmentTransformRules::KeepRelativeTransform);

    UMjGeom* Geom = NewObject<UMjGeom>(Robot, TEXT("TestGeom"));
    Geom->Size = FVector(0.1f, 0.1f, 0.1f);  // Sphere radius 0.1m — must be non-zero for MuJoCo to accept
    Geom->bOverride_Size = true;              // ExportTo only writes size when this is true
    Geom->RegisterComponent();
    Geom->AttachToComponent(RootBody, FAttachmentTransformRules::KeepRelativeTransform);

    UMjJoint* Joint = NewObject<UMjJoint>(Robot, TEXT("TestJoint"));
    Joint->RegisterComponent();
    Joint->AttachToComponent(RootBody, FAttachmentTransformRules::KeepRelativeTransform);

    // 4. Spawn Manager — World->BeginPlay() does not propagate AActor::BeginPlay()
    //    without a GameMode, so we drive compilation directly.
    AAMjManager* Manager = World->SpawnActor<AAMjManager>(SpawnParams);
    if (!Manager)
    {
        AddError(TEXT("Failed to spawn AAMjManager."));
        World->DestroyWorld(false);
        GEngine->DestroyWorldContext(World);
        return false;
    }

    // 5. Drive compilation directly — bypasses the BeginPlay dispatch issue in test worlds
    Manager->Compile();

    // 6. Verify Manager State
    TestTrue(TEXT("Manager should be initialized"), Manager->IsInitialized());

    // 7. Verify Runtime Binding — Bind() resolves IDs in PostSetup
    TestTrue(TEXT("Geom should have valid View ID"), Geom->m_GeomView.id != -1);
    TestTrue(TEXT("Joint should have valid View ID"), Joint->m_JointView.id != -1);

    // 8. Verify API — SetFriction writes directly to mjData
    if (Geom->m_GeomView._m)
    {
        float InitialFriction = 0.5f;
        Geom->SetFriction(InitialFriction);
        TestEqual(TEXT("Geom friction should be updated"), Geom->m_GeomView.friction[0], (double)InitialFriction);
    }

    // 9. Cleanup
    Manager->PhysicsEngine->bShouldStopTask = true;
    World->DestroyWorld(false);
    GEngine->DestroyWorldContext(World);

    return true;
}
