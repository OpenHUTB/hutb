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
#include "Tests/MjTestHelpers.h"
#include "MuJoCo/Components/Sensors/MjCamera.h"
#include "MuJoCo/Core/MjDebugVisualizer.h"
#include "MuJoCo/Core/AMjManager.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/TextureRenderTarget2D.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
    /**
     * Spawn a UMjCamera on the test articulation with the given mode and enable streaming.
     * Returns the camera so callers can inspect state after configuration.
     */
    UMjCamera* SpawnCameraAndStream(FMjUESession& Sess, EMjCameraMode Mode)
    {
        UMjCamera* Cam = NewObject<UMjCamera>(Sess.Robot, TEXT("TestCamera"));
        Cam->CaptureMode = Mode;
        Cam->RegisterComponent();
        Cam->AttachToComponent(Sess.Body, FAttachmentTransformRules::KeepRelativeTransform);
        Cam->SetStreamingEnabled(true);
        return Cam;
    }
}

// ============================================================================
// URLab.Camera.RealMode_ConfiguresFinalColorBGRA
//   Default Real mode → RT is RGBA8, CaptureSource is SCS_FinalToneCurveHDR.
// ============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMjCameraRealModeConfig,
    "URLab.Camera.RealMode_ConfiguresFinalColorBGRA",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMjCameraRealModeConfig::RunTest(const FString& Parameters)
{
    FMjUESession S;
    if (!S.Init())
    {
        AddError(FString::Printf(TEXT("FMjUESession::Init failed: %s"), *S.LastError));
        return false;
    }

    UMjCamera* Cam = SpawnCameraAndStream(S, EMjCameraMode::Real);
    if (!TestNotNull(TEXT("camera"), Cam)) { S.Cleanup(); return false; }

    if (!TestNotNull(TEXT("RT"), Cam->RenderTarget)) { S.Cleanup(); return false; }
    TestEqual(TEXT("RT format"), (int32)Cam->RenderTarget->RenderTargetFormat, (int32)ETextureRenderTargetFormat::RTF_RGBA8);

    if (TestNotNull(TEXT("capture component"), Cam->CaptureComponent))
    {
        TestEqual(TEXT("capture source"),
            (int32)Cam->CaptureComponent->CaptureSource,
            (int32)ESceneCaptureSource::SCS_FinalToneCurveHDR);
    }

    S.Cleanup();
    return true;
}

// ============================================================================
// URLab.Camera.DepthMode_ConfiguresSceneDepthFloat
//   Depth mode → RT is R32f, CaptureSource is SCS_SceneDepth, near clip overridden.
// ============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMjCameraDepthModeConfig,
    "URLab.Camera.DepthMode_ConfiguresSceneDepthFloat",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMjCameraDepthModeConfig::RunTest(const FString& Parameters)
{
    FMjUESession S;
    if (!S.Init())
    {
        AddError(FString::Printf(TEXT("FMjUESession::Init failed: %s"), *S.LastError));
        return false;
    }

    UMjCamera* Cam = SpawnCameraAndStream(S, EMjCameraMode::Depth);
    if (!TestNotNull(TEXT("camera"), Cam)) { S.Cleanup(); return false; }

    if (!TestNotNull(TEXT("RT"), Cam->RenderTarget)) { S.Cleanup(); return false; }
    TestEqual(TEXT("RT format"), (int32)Cam->RenderTarget->RenderTargetFormat, (int32)ETextureRenderTargetFormat::RTF_R32f);

    if (TestNotNull(TEXT("capture component"), Cam->CaptureComponent))
    {
        TestEqual(TEXT("capture source"),
            (int32)Cam->CaptureComponent->CaptureSource,
            (int32)ESceneCaptureSource::SCS_SceneDepth);
        TestTrue(TEXT("near clip overridden"), Cam->CaptureComponent->bOverride_CustomNearClippingPlane);
        TestEqual(TEXT("near clip value"),
            Cam->CaptureComponent->CustomNearClippingPlane, Cam->DepthNearCm);
    }

    S.Cleanup();
    return true;
}

// ============================================================================
// URLab.Camera.ModeCycle_RebuildsRenderTarget
//   Toggling streaming off then on after a mode change rebuilds the RT with
//   the new format. Real → Depth should switch RTF_RGBA8 → RTF_R32f.
// ============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMjCameraModeCycleRebuildsRT,
    "URLab.Camera.ModeCycle_RebuildsRenderTarget",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMjCameraModeCycleRebuildsRT::RunTest(const FString& Parameters)
{
    FMjUESession S;
    if (!S.Init())
    {
        AddError(FString::Printf(TEXT("FMjUESession::Init failed: %s"), *S.LastError));
        return false;
    }

    UMjCamera* Cam = SpawnCameraAndStream(S, EMjCameraMode::Real);
    if (!TestNotNull(TEXT("camera"), Cam)) { S.Cleanup(); return false; }
    TestEqual(TEXT("initial format"), (int32)Cam->RenderTarget->RenderTargetFormat, (int32)ETextureRenderTargetFormat::RTF_RGBA8);

    Cam->SetStreamingEnabled(false);
    TestNull(TEXT("RT cleared after disable"), Cam->RenderTarget);

    Cam->CaptureMode = EMjCameraMode::Depth;
    Cam->SetStreamingEnabled(true);
    if (!TestNotNull(TEXT("RT rebuilt"), Cam->RenderTarget)) { S.Cleanup(); return false; }
    TestEqual(TEXT("new format"), (int32)Cam->RenderTarget->RenderTargetFormat, (int32)ETextureRenderTargetFormat::RTF_R32f);

    S.Cleanup();
    return true;
}

// ============================================================================
// URLab.Camera.SegPool_AcquireReleaseRefcount
//   Pool lifecycle: two cameras acquire → shared pool is built once, survives
//   one release, is destroyed on the second release.
// ============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMjCameraSegPoolRefcount,
    "URLab.Camera.SegPool_AcquireReleaseRefcount",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMjCameraSegPoolRefcount::RunTest(const FString& Parameters)
{
    FMjUESession S;
    if (!S.Init())
    {
        AddError(FString::Printf(TEXT("FMjUESession::Init failed: %s"), *S.LastError));
        return false;
    }

    UMjDebugVisualizer* Viz = S.Manager ? S.Manager->FindComponentByClass<UMjDebugVisualizer>() : nullptr;
    if (!TestNotNull(TEXT("visualizer"), Viz)) { S.Cleanup(); return false; }

    // The visualizer's OverlayParentMaterial is initialized in BeginPlay; test worlds
    // don't dispatch BeginPlay, so trigger initialization manually.
    Viz->InitializeOverlayMaterial();

    // FMjUESession's base UMjGeom has no visualizer mesh. Attach a static-mesh child
    // so BuildSegPool's child walk has something to mirror into a sibling.
    UStaticMesh* CubeMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
    if (!TestNotNull(TEXT("engine cube mesh"), CubeMesh)) { S.Cleanup(); return false; }

    UStaticMeshComponent* ChildMesh = NewObject<UStaticMeshComponent>(S.Robot, TEXT("GeomChildMesh"));
    ChildMesh->SetStaticMesh(CubeMesh);
    ChildMesh->RegisterComponent();
    ChildMesh->AttachToComponent(S.Geom, FAttachmentTransformRules::KeepRelativeTransform);

    UMjCamera* CamA = NewObject<UMjCamera>(S.Robot, TEXT("CamA"));
    CamA->CaptureMode = EMjCameraMode::InstanceSegmentation;
    CamA->RegisterComponent();
    CamA->AttachToComponent(S.Body, FAttachmentTransformRules::KeepRelativeTransform);

    UMjCamera* CamB = NewObject<UMjCamera>(S.Robot, TEXT("CamB"));
    CamB->CaptureMode = EMjCameraMode::InstanceSegmentation;
    CamB->RegisterComponent();
    CamB->AttachToComponent(S.Body, FAttachmentTransformRules::KeepRelativeTransform);

    TArray<UPrimitiveComponent*> PoolA;
    Viz->AcquireSegPool(EMjCameraMode::InstanceSegmentation, CamA, PoolA);
    TestTrue(TEXT("pool has entries after first acquire"), PoolA.Num() > 0);

    TArray<UPrimitiveComponent*> PoolB;
    Viz->AcquireSegPool(EMjCameraMode::InstanceSegmentation, CamB, PoolB);
    TestEqual(TEXT("second acquire sees same pool size"), PoolB.Num(), PoolA.Num());
    if (PoolA.Num() > 0 && PoolB.Num() > 0)
    {
        TestEqual(TEXT("pool entries are shared"), PoolA[0], PoolB[0]);
    }

    // First release — pool should still exist because CamB still subscribed.
    Viz->ReleaseSegPool(EMjCameraMode::InstanceSegmentation, CamA);
    TArray<UPrimitiveComponent*> Snapshot;
    Viz->GetSegPoolSiblings(EMjCameraMode::InstanceSegmentation, Snapshot);
    TestTrue(TEXT("pool still alive after one release"), Snapshot.Num() > 0);

    // Final release — pool should be destroyed.
    Viz->ReleaseSegPool(EMjCameraMode::InstanceSegmentation, CamB);
    Viz->GetSegPoolSiblings(EMjCameraMode::InstanceSegmentation, Snapshot);
    TestEqual(TEXT("pool empty after last release"), Snapshot.Num(), 0);

    S.Cleanup();
    return true;
}

// ============================================================================
// URLab.Camera.SegMode_WiresShowOnlyListFromPool
//   A seg camera at SetStreamingEnabled(true) configures its CaptureComponent:
//   PRM_UseShowOnlyList, CaptureSource = SCS_BaseColor, ShowOnlyComponents
//   populated from the pool.
// ============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMjCameraSegWiresShowOnly,
    "URLab.Camera.SegMode_WiresShowOnlyListFromPool",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMjCameraSegWiresShowOnly::RunTest(const FString& Parameters)
{
    FMjUESession S;
    if (!S.Init())
    {
        AddError(FString::Printf(TEXT("FMjUESession::Init failed: %s"), *S.LastError));
        return false;
    }

    if (S.Manager && S.Manager->DebugVisualizer)
    {
        S.Manager->DebugVisualizer->InitializeOverlayMaterial();
    }

    UStaticMesh* CubeMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
    UStaticMeshComponent* ChildMesh = NewObject<UStaticMeshComponent>(S.Robot, TEXT("GeomChildMesh"));
    ChildMesh->SetStaticMesh(CubeMesh);
    ChildMesh->RegisterComponent();
    ChildMesh->AttachToComponent(S.Geom, FAttachmentTransformRules::KeepRelativeTransform);

    UMjCamera* Cam = NewObject<UMjCamera>(S.Robot, TEXT("SegCam"));
    Cam->CaptureMode = EMjCameraMode::InstanceSegmentation;
    Cam->RegisterComponent();
    Cam->AttachToComponent(S.Body, FAttachmentTransformRules::KeepRelativeTransform);
    Cam->SetStreamingEnabled(true);

    if (!TestNotNull(TEXT("capture component"), Cam->CaptureComponent)) { S.Cleanup(); return false; }
    TestEqual(TEXT("capture source"),
        (int32)Cam->CaptureComponent->CaptureSource,
        (int32)ESceneCaptureSource::SCS_FinalToneCurveHDR);
    TestEqual(TEXT("primitive render mode"),
        (int32)Cam->CaptureComponent->PrimitiveRenderMode,
        (int32)ESceneCapturePrimitiveRenderMode::PRM_UseShowOnlyList);
    TestTrue(TEXT("ShowOnlyComponents populated"),
        Cam->CaptureComponent->ShowOnlyComponents.Num() > 0);

    Cam->SetStreamingEnabled(false);
    TestEqual(TEXT("ShowOnlyComponents cleared on disable"),
        Cam->CaptureComponent->ShowOnlyComponents.Num(), 0);

    S.Cleanup();
    return true;
}

// ============================================================================
// URLab.Camera.NonSegMode_HidesSiblingPool
//   A Real-mode camera and an InstanceSeg camera coexist: the Real camera's
//   HiddenComponents list includes the seg pool siblings.
// ============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMjCameraNonSegHidesSiblings,
    "URLab.Camera.NonSegMode_HidesSiblingPool",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMjCameraNonSegHidesSiblings::RunTest(const FString& Parameters)
{
    FMjUESession S;
    if (!S.Init())
    {
        AddError(FString::Printf(TEXT("FMjUESession::Init failed: %s"), *S.LastError));
        return false;
    }

    if (S.Manager && S.Manager->DebugVisualizer)
    {
        S.Manager->DebugVisualizer->InitializeOverlayMaterial();
    }

    UStaticMesh* CubeMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
    UStaticMeshComponent* ChildMesh = NewObject<UStaticMeshComponent>(S.Robot, TEXT("GeomChildMesh"));
    ChildMesh->SetStaticMesh(CubeMesh);
    ChildMesh->RegisterComponent();
    ChildMesh->AttachToComponent(S.Geom, FAttachmentTransformRules::KeepRelativeTransform);

    // Start the seg camera first so the pool exists when the Real camera subscribes.
    UMjCamera* Seg = NewObject<UMjCamera>(S.Robot, TEXT("SegCam"));
    Seg->CaptureMode = EMjCameraMode::InstanceSegmentation;
    Seg->RegisterComponent();
    Seg->AttachToComponent(S.Body, FAttachmentTransformRules::KeepRelativeTransform);
    Seg->SetStreamingEnabled(true);

    UMjCamera* Real = NewObject<UMjCamera>(S.Robot, TEXT("RealCam"));
    Real->CaptureMode = EMjCameraMode::Real;
    Real->RegisterComponent();
    Real->AttachToComponent(S.Body, FAttachmentTransformRules::KeepRelativeTransform);
    Real->SetStreamingEnabled(true);

    if (!TestNotNull(TEXT("real capture component"), Real->CaptureComponent)) { S.Cleanup(); return false; }
    const int32 ExpectedHidden = Seg->CaptureComponent->ShowOnlyComponents.Num();
    TestEqual(TEXT("real cam hides seg siblings"),
        Real->CaptureComponent->HiddenComponents.Num(), ExpectedHidden);

    S.Cleanup();
    return true;
}
