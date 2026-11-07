// Copyright (c) OpenHUTB. All rights reserved.
//
// URovMuJoCoBridge 的自动化测试
//
// 目标：验证「AirSim ROV Pawn 跟随 MuJoCo」这条链路的组件级行为：
//   1. 桥接能找到 MuJoCo 的 body（后缀匹配）
//   2. 全局位姿源被正确注册/注销
//   3. 位姿源的取值与 UMjBody 的物理状态一致
//
// 说明：这是组件级验证，不覆盖完整的 AirSim SimMode 集成。
// 完整集成（Pawn 真的跟着动、相机跟随、无浮力双计）需要带场景的可视化验证。
//
// 设计依据：docs/decisions/2026-11-05-ROV物理视觉统一改动方案.md

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Engine/World.h"
#include "GameFramework/Actor.h"

#include "Tests/MjTestHelpers.h"
#include "Vehicles/Rov/RovMuJoCoBridge.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FRovBridgeFollowerTest,
    "URLab.RovBridge.Follower",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRovBridgeFollowerTest::RunTest(const FString& Parameters)
{
    // --- 准备一个最小 MuJoCo 世界（一个 body + geom + joint）---
    FMjUESession S;
    if (!S.Init()) {
        AddError(FString::Printf(TEXT("FMjUESession 初始化失败: %s"), *S.LastError));
        return false;
    }

    if (!S.Body) {
        AddError(TEXT("FMjUESession 未创建 body"));
        S.Cleanup();
        return false;
    }

    AddInfo(FString::Printf(TEXT("MuJoCo body 名: %s"), *S.Body->GetName()));
    AddInfo(FString::Printf(TEXT("Articulation 的 body 数: %d"), S.Robot->GetBodies().Num()));

    // --- 建一个承载桥接组件的 Actor ---
    AActor* Host = S.World->SpawnActor<AActor>();
    if (!Host) {
        AddError(TEXT("SpawnActor 失败"));
        S.Cleanup();
        return false;
    }

    URovMuJoCoBridge* Bridge = NewObject<URovMuJoCoBridge>(Host, TEXT("RovMuJoCoBridge"));
    if (!Bridge) {
        AddError(TEXT("创建 URovMuJoCoBridge 失败"));
        S.Cleanup();
        return false;
    }
    Bridge->RegisterComponent();

    // FMjUESession 的 body 叫 "RootBody"，测试里改成匹配它
    Bridge->TargetBodyNameSuffix = TEXT("RootBody");
    // 测试 Actor 上没有需要关闭的物理，跳过以免干扰
    Bridge->bDisablePawnPhysics = false;

    // 测试世界不会自动派发 BeginPlay，手动触发
    Bridge->BeginPlay();

    // --- 断言 1：桥接激活 ---
    TestTrue(TEXT("桥接应成功激活（找到目标 body）"), Bridge->IsBridgeActive());
    TestEqual(TEXT("解析到的 body 名应与目标一致"),
        Bridge->GetResolvedBodyName(), S.Body->GetName());

    // --- 断言 2：全局位姿源已注册 ---
    TestTrue(TEXT("全局位姿源应已注册"),
        static_cast<bool>(URovMuJoCoBridge::ExternalPoseProvider));

    if (!URovMuJoCoBridge::ExternalPoseProvider) {
        Bridge->EndPlay(EEndPlayReason::Destroyed);
        S.Cleanup();
        return false;
    }

    // --- 断言 3：步进后位姿与 MuJoCo body 一致 ---
    S.Step(20);

    FVector ProviderPos = FVector::ZeroVector;
    FQuat ProviderRot = FQuat::Identity;
    const bool bGotPose = URovMuJoCoBridge::ExternalPoseProvider(ProviderPos, ProviderRot);
    TestTrue(TEXT("位姿源应返回有效位姿"), bGotPose);

    const FVector BodyPos = S.Body->GetWorldPosition();
    const FQuat BodyRot = S.Body->GetWorldRotation();

    AddInfo(FString::Printf(TEXT("provider 位置: %s"), *ProviderPos.ToString()));
    AddInfo(FString::Printf(TEXT("body     位置: %s"), *BodyPos.ToString()));

    TestTrue(TEXT("位姿源返回的位置应与 MuJoCo body 一致"),
        ProviderPos.Equals(BodyPos, 0.01));
    TestTrue(TEXT("位姿源返回的姿态应与 MuJoCo body 一致"),
        ProviderRot.Equals(BodyRot, 0.001));

    // --- 断言 4：EndPlay 后清空 ---
    Bridge->EndPlay(EEndPlayReason::Destroyed);
    TestFalse(TEXT("EndPlay 后全局位姿源应被清空"),
        static_cast<bool>(URovMuJoCoBridge::ExternalPoseProvider));
    TestFalse(TEXT("EndPlay 后桥接应标记为未激活"), Bridge->IsBridgeActive());

    S.Cleanup();
    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
