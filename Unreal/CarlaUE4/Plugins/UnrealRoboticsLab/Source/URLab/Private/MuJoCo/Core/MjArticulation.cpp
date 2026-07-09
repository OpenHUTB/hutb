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

#include "MuJoCo/Core/MjArticulation.h"
#include "MuJoCo/Core/Spec/MjSpecWrapper.h"
#include "MuJoCo/Components/Controllers/MjArticulationController.h"
#include "MuJoCo/Input/MjTwistController.h"
#include "Misc/MessageDialog.h"
#include "Engine/Blueprint.h"
#include "Engine/SimpleConstructionScript.h"
#include "Engine/SCS_Node.h"
#include "MuJoCo/Components/Defaults/MjDefault.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "GameFramework/SpringArmComponent.h"
#include "Camera/CameraComponent.h"
#include "Components/SceneComponent.h"
#include "Containers/Queue.h"
#include "MuJoCo/Components/Bodies/MjBody.h"
#include "MuJoCo/Components/Bodies/MjFrame.h"
#include "MuJoCo/Components/Bodies/MjWorldBody.h"
#include "mujoco/mujoco.h"
#include "Chaos/TriangleMeshImplicitObject.h"
#include "MuJoCo/Components/Joints/MjJoint.h"
#include "MuJoCo/Components/Geometry/MjSite.h"
#include "MuJoCo/Components/Sensors/MjSensor.h"
#include "MuJoCo/Components/Actuators/MjActuator.h"
#include "MuJoCo/Components/Tendons/MjTendon.h"
#include "MuJoCo/Components/Deformable/MjFlexcomp.h"
#include "MuJoCo/Components/Defaults/MjDefault.h"
#include "MuJoCo/Components/Physics/MjContactPair.h"
#include "MuJoCo/Components/Physics/MjContactExclude.h"
#include "PhysicsEngine/BodySetup.h"
#include "Utils/MeshUtils.h"
#include "MuJoCo/Components/Geometry/MjGeom.h"
#include "DrawDebugHelpers.h"
#include "MuJoCo/Utils/MjUtils.h"
#include "Utils/URLabLogging.h"

AMjArticulation::AMjArticulation()
{
    PrimaryActorTick.bCanEverTick = true;

    // 1. 创建场景组件作为根组件
    DefaultSceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("ArticulationRoot"));
    RootComponent = DefaultSceneRoot;
}

bool AMjArticulation::ShouldTickIfViewportsOnly() const
{
    return bDrawDebugJoints || bDrawDebugCollision || bDrawDebugSites;
}

void AMjArticulation::BeginPlay()
{
    Super::BeginPlay();
    UpdateGroup3Visibility();

    // 如果没有找到 TwistController，则自动创建
    if (!FindComponentByClass<UMjTwistController>())
    {
        UMjTwistController* TwistCtrl = NewObject<UMjTwistController>(this, TEXT("TwistController"));

        // 从插件内容加载默认输入资产（需要转成UE4资产）
        static UInputMappingContext* DefaultIMC = LoadObject<UInputMappingContext>(
            nullptr, TEXT("/UnrealRoboticsLab/Input/IMC_TwistControl.IMC_TwistControl"));
        static UInputAction* DefaultMove = LoadObject<UInputAction>(
            nullptr, TEXT("/UnrealRoboticsLab/Input/IA_TwistMove.IA_TwistMove"));
        static UInputAction* DefaultTurn = LoadObject<UInputAction>(
            nullptr, TEXT("/UnrealRoboticsLab/Input/IA_TwistTurn.IA_TwistTurn"));

        if (DefaultIMC) TwistCtrl->TwistMappingContext = DefaultIMC;
        if (DefaultMove) TwistCtrl->MoveAction = DefaultMove;
        if (DefaultTurn) TwistCtrl->TurnAction = DefaultTurn;

        TwistCtrl->RegisterComponent();
        UE_LOG(LogURLab, Log, TEXT("Auto-created UMjTwistController on '%s' (IMC=%s, Move=%s, Turn=%s)"),
            *GetName(),
            DefaultIMC ? TEXT("OK") : TEXT("MISSING"),
            DefaultMove ? TEXT("OK") : TEXT("MISSING"),
            DefaultTurn ? TEXT("OK") : TEXT("MISSING"));
    }
}

void AMjArticulation::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    Super::EndPlay(EndPlayReason);

    if (m_wrapper)
    {
        delete m_wrapper;
        m_wrapper = nullptr;
    }

    if (m_ChildSpec)
    {
        mj_deleteSpec(m_ChildSpec);
        m_ChildSpec = nullptr;
    }
}

void AMjArticulation::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
    Super::SetupPlayerInputComponent(PlayerInputComponent);

    UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(PlayerInputComponent);
    if (!EIC) return;

    UMjTwistController* TwistCtrl = FindComponentByClass<UMjTwistController>();
    if (TwistCtrl)
    {
        TwistCtrl->BindInput(EIC);
        UE_LOG(LogURLab, Log, TEXT("AMjArticulation::SetupPlayerInputComponent — Bound twist input for '%s'"), *GetName());
    }
}

void AMjArticulation::PossessedBy(AController* NewController)
{
    Super::PossessedBy(NewController);

    APlayerController* PC = Cast<APlayerController>(NewController);
    if (!PC) return;

    // 添加扭转映射上下文
    UMjTwistController* TwistCtrl = FindComponentByClass<UMjTwistController>();
    if (TwistCtrl && TwistCtrl->TwistMappingContext)
    {
        if (ULocalPlayer* LP = PC->GetLocalPlayer())
        {
            if (UEnhancedInputLocalPlayerSubsystem* Subsystem = LP->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())
            {
                Subsystem->AddMappingContext(TwistCtrl->TwistMappingContext, 1);
                UE_LOG(LogURLab, Log, TEXT("AMjArticulation::PossessedBy — Added twist mapping context for '%s'"), *GetName());
            }
        }
    }

    // 附加弹簧臂 和 相机到根身体，以便相机跟随物理
    UMjBody* RootBody = nullptr;
    TArray<UMjBody*> Bodies;
    GetComponents<UMjBody>(Bodies);
    for (UMjBody* B : Bodies)
    {
        if (!B->bIsDefault)
        {
            RootBody = B;
            break;
        }
    }

    if (RootBody)
    {
        USpringArmComponent* Arm = NewObject<USpringArmComponent>(this, TEXT("PossessCameraArm"));
        Arm->SetupAttachment(RootBody);
        Arm->TargetArmLength = PossessCameraDistance;
        Arm->SetRelativeRotation(FRotator(PossessCameraPitch, 0.0f, 0.0f));
        Arm->bDoCollisionTest = false;
        Arm->bUsePawnControlRotation = false;
        Arm->bEnableCameraLag = true;
        Arm->CameraLagSpeed = PossessCameraLagSpeed;
        Arm->CameraLagMaxDistance = 100.0f;
        Arm->bEnableCameraRotationLag = true;
        Arm->CameraRotationLagSpeed = PossessCameraRotationLagSpeed;
        Arm->SocketOffset = PossessCameraOffset;
        Arm->RegisterComponent();

        UCameraComponent* Cam = NewObject<UCameraComponent>(this, TEXT("PossessCamera"));
        Cam->SetupAttachment(Arm);
        Cam->RegisterComponent();

        // 标记它们，以便我们可以在取消占有时清理
        Arm->ComponentTags.Add(TEXT("PossessCamera"));
        Cam->ComponentTags.Add(TEXT("PossessCamera"));

        UE_LOG(LogURLab, Log, TEXT("Attached follow camera to root body '%s'"), *RootBody->GetName());
    }
}

void AMjArticulation::UnPossessed()
{
    AController* OldController = GetController();
    APlayerController* PC = Cast<APlayerController>(OldController);

    // 移除扭转映射上下文并重置状态
    UMjTwistController* TwistCtrl = FindComponentByClass<UMjTwistController>();
    if (TwistCtrl)
    {
        TwistCtrl->ResetTwist();

        if (PC && TwistCtrl->TwistMappingContext)
        {
            if (ULocalPlayer* LP = PC->GetLocalPlayer())
            {
                if (UEnhancedInputLocalPlayerSubsystem* Subsystem = LP->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())
                {
                    Subsystem->RemoveMappingContext(TwistCtrl->TwistMappingContext);
                }
            }
        }
    }

    // 移除我们在占有时创建的跟随相机组件
    TArray<UActorComponent*> ToRemove;
    for (UActorComponent* Comp : GetComponents())
    {
        if (Comp && Comp->ComponentTags.Contains(TEXT("PossessCamera")))
        {
            ToRemove.Add(Comp);
        }
    }
    for (UActorComponent* Comp : ToRemove)
    {
        Comp->DestroyComponent();
    }

    Super::UnPossessed();
}


void AMjArticulation::Setup(mjSpec* Spec, mjVFS* VFS)
{
    m_spec = Spec;
    m_vfs = VFS;
    
    m_ChildSpec = mj_makeSpec();
    m_ChildSpec->compiler.degree = false;

    // 将此关节的仿真选项应用于子规格。
    // mjs_attach将在编译时将这些合并到根规格中。
    SimOptions.ApplyToSpec(m_ChildSpec);

    m_prefix = GetName() + TEXT("_");
    
    m_wrapper = new FMujocoSpecWrapper(m_ChildSpec, m_vfs);
    m_wrapper->MeshCacheSubDir = GetClass()->GetName();

    // 1b. 自动解析 bIsDefault 并从层次结构同步 ParentClassName。
    // 这确保了即使 OnBlueprintCompiled 尚未运行，也能正确。
    {
        TArray<UMjDefault*> AllDefaults;
        GetComponents<UMjDefault>(AllDefaults);
        for (UMjDefault* Def : AllDefaults)
        {
            Def->bIsDefault = true;

            // 如果附加到另一个 UMjDefault，则从附加父级同步 ParentClassName。
            // 如果没有（例如，附加到根），则保留现有的 ParentClassName 作为后备
            // 对于以编程方式创建的默认值，明确设置它。
            if (UMjDefault* ParentDef = Cast<UMjDefault>(Def->GetAttachParent()))
            {
                Def->ParentClassName = ParentDef->ClassName;
            }
            else if (Def->ParentClassName.IsEmpty())
            {
                // 层级中没有父类默认值，也没有明确的 ParentClassName — 使用根默认
            }

            TArray<USceneComponent*> DefChildren;
            Def->GetChildrenComponents(true, DefChildren);
            for (USceneComponent* Child : DefChildren)
            {
                if (UMjComponent* MjChild = Cast<UMjComponent>(Child))
                {
                    MjChild->bIsDefault = true;
                }
            }
        }
    }

    // 2. 按层次顺序处理默认值（父级在子级之前），以便
    //    mjs_findDefault 可以在 AddDefault 期间解析父类。
    {
        TArray<UMjDefault*> AllDefaults;
        GetComponents<UMjDefault>(AllDefaults);

        // 找到根默认值（其父级不是 UMjDefault 的那些）
        // 并递归处理它们，深度优先
        TFunction<void(UMjDefault*)> ProcessDefaultTree = [&](UMjDefault* Def)
        {
            m_wrapper->AddDefault(Def);
            // 找到附加到此默认值的子默认值
            TArray<USceneComponent*> Children;
            Def->GetChildrenComponents(false, Children);
            for (USceneComponent* Child : Children)
            {
                if (UMjDefault* ChildDef = Cast<UMjDefault>(Child))
                {
                    ProcessDefaultTree(ChildDef);
                }
            }
        };

        for (UMjDefault* Def : AllDefaults)
        {
            // 仅从根开始（父级不是 UMjDefault）
            UMjDefault* ParentDef = Cast<UMjDefault>(Def->GetAttachParent());
            if (!ParentDef)
            {
                ProcessDefaultTree(Def);
            }
        }
    }

    // 3. 查找 UMjWorldBody 并正常构建身体层次结构（进入子规格）
    UMjWorldBody* WorldBody = nullptr;
    TArray<UMjWorldBody*> AllWorldBodies;
    GetComponents<UMjWorldBody>(AllWorldBodies);
    if (AllWorldBodies.Num() > 0)
    {
         WorldBody = AllWorldBodies[0];
    }

    if (!WorldBody)
    {
        UE_LOG(LogURLab, Error, TEXT("AMjArticulation::Setup - No UMjWorldBody component found for %s"), *GetName());
        return;
    }

    TArray<UMjBody*> RootBodies;
    TArray<USceneComponent*> WorldChildren = WorldBody->GetAttachChildren();
     for (USceneComponent* Child : WorldChildren)
     {
          if (UMjBody* BodyChild = Cast<UMjBody>(Child))
          {
               if (!BodyChild->bIsDefault)
               {
                    RootBodies.Add(BodyChild);
                    BodyChild->Setup(nullptr, nullptr, m_wrapper);
               }
          }
          else if (UMjFrame* FrameChild = Cast<UMjFrame>(Child))
          {
               if (!FrameChild->bIsDefault)
               {
                    FrameChild->Setup(nullptr, nullptr, m_wrapper);
               }
          }
     }

     // Worldbody 级别的 IMjSpecElement 子项（直接附加到 <worldbody> 的站点等）需要在子规范的 world body 上进行注册，
     // 以便下游引用（例如包裹 worldbody 位点的肌腱）在编译时就能解析。
     mjsBody* ChildWorld = mjs_findBody(m_ChildSpec, "world");
     if (ChildWorld)
     {
          for (USceneComponent* Child : WorldChildren)
          {
               if (Cast<UMjBody>(Child) || Cast<UMjFrame>(Child)) continue;
               if (UMjComponent* MjComp = Cast<UMjComponent>(Child))
               {
                    if (MjComp->bIsDefault) continue;
                    if (IMjSpecElement* SpecElem = Cast<IMjSpecElement>(Child))
                    {
                         SpecElem->RegisterToSpec(*m_wrapper, ChildWorld);
                    }
               }
          }
     }

    // 3b. 注册 flexcomp 组件（可以是 worldbody 子级或 body 子级）
    TArray<UMjFlexcomp*> Flexcomps;
    GetComponents<UMjFlexcomp>(Flexcomps);
    for (UMjFlexcomp* Flex : Flexcomps)
    {
        if (Flex && !Flex->bIsDefault)
        {
            mjsBody* ParentSpecBody = nullptr;

            USceneComponent* Parent = Flex->GetAttachParent();
            while (Parent)
            {
                if (UMjBody* Body = Cast<UMjBody>(Parent))
                {
                    FString BodyName = Body->MjName.IsEmpty() ? Body->GetName() : Body->MjName;
                    ParentSpecBody = mjs_findBody(m_wrapper->Spec, TCHAR_TO_UTF8(*BodyName));
                    if (ParentSpecBody) break;
                }
                Parent = Parent->GetAttachParent();
            }

            if (!ParentSpecBody)
            {
                ParentSpecBody = mjs_findBody(m_wrapper->Spec, "world");
            }

            Flex->RegisterToSpec(*m_wrapper, ParentSpecBody);
        }
    }

    // 4. 添加肌腱（进入子规范，在身体之后，以便关节名称被设置）
    TArray<UMjTendon*> Tendons;
    GetComponents<UMjTendon>(Tendons);
    for (UMjTendon* Tendon : Tendons)
    {
        if (Tendon && !Tendon->bIsDefault)
            Tendon->RegisterToSpec(*m_wrapper);
    }

    // 5. 添加传感器和执行器（进入子规范，在肌腱之后，可能会引用字符串）
    TArray<UMjSensor*> Sensors;
    GetComponents<UMjSensor>(Sensors);
    for (UMjSensor* Sensor : Sensors)
    {
        if (Sensor && !Sensor->bIsDefault)
            Sensor->RegisterToSpec(*m_wrapper);
    }

    TArray<UMjActuator*> Actuators;
    GetComponents<UMjActuator>(Actuators);
    for (UMjActuator* Actuator : Actuators)
    {
        if (Actuator && !Actuator->bIsDefault)
            Actuator->RegisterToSpec(*m_wrapper);
    }

    TArray<UMjContactPair*> ContactPairs;
    GetComponents<UMjContactPair>(ContactPairs);
    for (UMjContactPair* Pair : ContactPairs)
    {
        Pair->RegisterToSpec(*m_wrapper);
    }

    TArray<UMjContactExclude*> ContactExcludes;
    GetComponents<UMjContactExclude>(ContactExcludes);
    for (UMjContactExclude* Exclude : ContactExcludes)
    {
        Exclude->RegisterToSpec(*m_wrapper);
    }

    TArray<UMjEquality*> Equalities;
    GetComponents<UMjEquality>(Equalities);
    for (UMjEquality* Equality : Equalities)
    {
        if (Equality && !Equality->bIsDefault)
            Equality->RegisterToSpec(*m_wrapper);
    }

    TArray<UMjKeyframe*> Keyframes;
    GetComponents<UMjKeyframe>(Keyframes);
    for (UMjKeyframe* Keyframe : Keyframes)
    {
        if (Keyframe && !Keyframe->bIsDefault)
            Keyframe->RegisterToSpec(*m_wrapper);
    }
    
    mjsBody* parentWorld = mjs_findBody(Spec, "world");
    mjsFrame* attachmentFrame = mjs_addFrame(parentWorld, 0);

    UE_LOG(LogURLab, Log, TEXT("[mjs_attach] '%s' — Attaching child spec (element=%p) to world frame with prefix='%s'"),
        *GetName(), m_ChildSpec ? m_ChildSpec->element : nullptr, *m_prefix);

    mjsElement* attachResult = mjs_attach(attachmentFrame->element, m_ChildSpec->element, TCHAR_TO_UTF8(*m_prefix), "");

    if (!attachResult)
    {
        // 获取两个规格的错误
        const char* childErr = mjs_getError(m_ChildSpec);
        const char* rootErr = mjs_getError(Spec);
        bAttachFailed = true;
        UE_LOG(LogURLab, Error, TEXT("[mjs_attach] '%s' — FAILED (returned null). Child spec elements will not appear in compiled model."), *GetName());
        UE_LOG(LogURLab, Error, TEXT("[mjs_attach] '%s' — Child spec error: %hs"), *GetName(), childErr ? childErr : "(none)");
        UE_LOG(LogURLab, Error, TEXT("[mjs_attach] '%s' — Root spec error: %hs"), *GetName(), rootErr ? rootErr : "(none)");

        // 记录子规格体计数以进行诊断
        mjsBody* childWorld = mjs_findBody(m_ChildSpec, "world");
        UE_LOG(LogURLab, Error, TEXT("[mjs_attach] '%s' — Child spec world body: %p, child spec element: %p"),
            *GetName(), childWorld, m_ChildSpec ? m_ChildSpec->element : nullptr);
    }
    else
    {
        UE_LOG(LogURLab, Log, TEXT("[mjs_attach] '%s' — SUCCESS (returned %p). Prefix='%s'"), *GetName(), attachResult, *m_prefix);
        // 子规格元素已移入根规格 — 子规格现在已被消耗。
        // 将其置为空，以便 EndPlay 不会尝试双重释放它。
    }

    FTransform ActorTransform = GetActorTransform();
    double MjPos[3];
    double MjQuat[4];
    MjUtils::UEToMjPosition(ActorTransform.GetLocation(), MjPos);
    MjUtils::UEToMjRotation(ActorTransform.GetRotation(), MjQuat);

    attachmentFrame->pos[0] = MjPos[0];
    attachmentFrame->pos[1] = MjPos[1];
    attachmentFrame->pos[2] = MjPos[2];
    attachmentFrame->quat[0] = MjQuat[0];
    attachmentFrame->quat[1] = MjQuat[1];
    attachmentFrame->quat[2] = MjQuat[2];
    attachmentFrame->quat[3] = MjQuat[3];

    m_ChildSpec = nullptr;
}

TArray<USceneComponent*> AMjArticulation::GetRuntimeComponentsOfClass(TSubclassOf<USceneComponent> ComponentClass) const
{
    TArray<USceneComponent*> Result;
    TArray<USceneComponent*> AllComponents;
    // GetComponents(ComponentClass, AllComponents);

    UWorld* MyWorld = GetWorld();
    for (USceneComponent* Comp : AllComponents)
    {
        // 跳过泄漏到 PIE（Play In Editor）的 SCS 模板组件
        if (MyWorld && Comp->GetWorld() != MyWorld) continue;

        if (UMjComponent* MjComp = Cast<UMjComponent>(Comp))
        {
            if (!MjComp->bIsDefault)
            {
                Result.Add(Comp);
            }
        }
        else
        {
            Result.Add(Comp);
        }
    }
    return Result;
}

void AMjArticulation::PostSetup(mjModel* Model, mjData* Data)
{
    m_model = Model;
    m_data = Data;

    TArray<UMjComponent*> AllMjComponents;
    GetRuntimeComponents<UMjComponent>(AllMjComponents);

    UE_LOG(LogURLab, Log, TEXT("AMjArticulation::PostSetup - Found %d runtime MjComponents for articulation '%s'"), AllMjComponents.Num(), *GetName());
    for (UMjComponent* MjComp : AllMjComponents)
    {
        if (MjComp) MjComp->Bind(Model, Data, m_prefix);
    }

    // 为蓝图 API 构建组件名称映射（O(1) 按名称查找）
    // 我们使用 MuJoCo 侧名称（GetMjName()）作为一致性键，以便与 UI/数据保持一致。
    ActuatorComponentMap.Empty();
    JointComponentMap.Empty();
    SensorComponentMap.Empty();
    BodyComponentMap.Empty();
    TendonComponentMap.Empty();
    EqualityComponentMap.Empty();
    KeyframeComponentMap.Empty();

    for (UMjComponent* MjComp : AllMjComponents)
    {
        FString UE_Name = MjComp->GetName();
        FString MJ_Name = MjComp->GetMjName();

        auto AddToMap = [&](auto& Map, auto* Comp)
        {
            if (!UE_Name.IsEmpty()) Map.Add(UE_Name, Comp);
            if (!MJ_Name.IsEmpty() && MJ_Name != UE_Name) Map.Add(MJ_Name, Comp);
        };

        if (UMjActuator* A = Cast<UMjActuator>(MjComp)) AddToMap(ActuatorComponentMap, A);
        else if (UMjJoint* J = Cast<UMjJoint>(MjComp)) AddToMap(JointComponentMap, J);
        else if (UMjSensor* S = Cast<UMjSensor>(MjComp)) AddToMap(SensorComponentMap, S);
        else if (UMjBody* B = Cast<UMjBody>(MjComp)) AddToMap(BodyComponentMap, B);
        else if (UMjTendon* T = Cast<UMjTendon>(MjComp)) AddToMap(TendonComponentMap, T);
        else if (UMjEquality* E = Cast<UMjEquality>(MjComp)) AddToMap(EqualityComponentMap, E);
        else if (UMjKeyframe* K = Cast<UMjKeyframe>(MjComp)) AddToMap(KeyframeComponentMap, K);
    }

    UE_LOG(LogURLab, Log, TEXT("AMjArticulation::PostSetup - %s maps (using prefix '%s'): %d actuators, %d joints, %d sensors, %d bodies, %d tendons"),
           *GetName(), *m_prefix, ActuatorComponentMap.Num(), JointComponentMap.Num(), SensorComponentMap.Num(), BodyComponentMap.Num(), TendonComponentMap.Num());

    // 构建 MuJoCo ID 映射（O(1) 从 ID 解析到组件）
    BodyIdMap.Empty();
    GeomIdMap.Empty();
    JointIdMap.Empty();
    SensorIdMap.Empty();
    ActuatorIdMap.Empty();
    TendonIdMap.Empty();

    for (UMjComponent* MjComp : AllMjComponents)
    {
        int ID = MjComp->GetMjID();
        if (ID < 0) continue;

        if (UMjBody* Body = Cast<UMjBody>(MjComp)) BodyIdMap.Add(ID, Body);
        else if (UMjGeom* Geom = Cast<UMjGeom>(MjComp)) GeomIdMap.Add(ID, Geom);
        else if (UMjJoint* Joint = Cast<UMjJoint>(MjComp)) JointIdMap.Add(ID, Joint);
        else if (UMjSensor* Sensor = Cast<UMjSensor>(MjComp)) SensorIdMap.Add(ID, Sensor);
        else if (UMjActuator* Actuator = Cast<UMjActuator>(MjComp)) ActuatorIdMap.Add(ID, Actuator);
        else if (UMjTendon* Tendon = Cast<UMjTendon>(MjComp)) TendonIdMap.Add(ID, Tendon);
    }

    UE_LOG(LogURLab, Log, TEXT("AMjArticulation::PostSetup - %s maps (using prefix '%s'): %d actuators, %d joints, %d sensors, %d bodies, %d tendons"),
           *GetName(), *m_prefix, ActuatorIdMap.Num(), JointIdMap.Num(), SensorIdMap.Num(), BodyIdMap.Num(), TendonIdMap.Num());

    // 绑定任何关节控制器组件（PD、直通或用户自定义）
    // 并缓存它，以便 ApplyControls（物理线程）不必在每个步骤中迭代
    // OwnedComponents — 这种竞争会导致在 flex 负载下崩溃。
    CachedController = FindComponentByClass<UMjArticulationController>();
    if (CachedController)
    {
        CachedController->Bind(m_model, m_data, ActuatorIdMap);
        UE_LOG(LogURLab, Log, TEXT("AMjArticulation::PostSetup - Bound controller '%s' with %d actuators"),
               *CachedController->GetClass()->GetName(), CachedController->GetNumBindings());
    }
}

void AMjArticulation::ApplyControls()
{
    // 线程安全：ActuatorIdMap 在 PostSetup（游戏线程）期间构建，这里（物理线程）只读。
    // 构建完成后，这个映射从未被修改。
    // RunMujocoAsync 会在 PostSetup 完成后启动，保证可见性。
    if (!m_model || !m_data) return;

    // 如果按住关键帧，则覆盖正常的控制流程
    if (bHoldingKeyframe)
    {
        if (bHoldViaQpos && HeldKeyframeQpos.Num() > 0)
        {
            // 直接 qpos 注入 — 运动学保持，绕过执行器。
            // 跳过自由关节 DOF 以保持世界位置。
            for (int32 j = 0; j < m_model->njnt; j++)
            {
                int32 JointType = m_model->jnt_type[j];
                if (JointType == mjJNT_FREE) continue;

                int32 QposAdr = m_model->jnt_qposadr[j];
                int32 DofAdr = m_model->jnt_dofadr[j];
                int32 NqPos = (JointType == mjJNT_BALL) ? 4 : 1;
                int32 NvDof = (JointType == mjJNT_BALL) ? 3 : 1;

                for (int32 k = 0; k < NqPos && (QposAdr + k) < HeldKeyframeQpos.Num(); k++)
                {
                    m_data->qpos[QposAdr + k] = (mjtNum)HeldKeyframeQpos[QposAdr + k];
                }
                for (int32 k = 0; k < NvDof; k++)
                {
                    m_data->qvel[DofAdr + k] = 0.0;
                }
            }
        }
        else if (HeldKeyframeCtrl.Num() > 0)
        {
            // 基于Ctrl的保持 — 执动器驱动到目标位置
            int32 Count = FMath::Min((float)(HeldKeyframeCtrl.Num()), (float)(m_model->nu));
            for (int32 i = 0; i < Count; i++)
            {
                m_data->ctrl[i] = (mjtNum)HeldKeyframeCtrl[i];
            }
        }
        return;
    }

    // 如果存在且激活，就委托给自定义控制器。
    // 使用 PostSetup 缓存的指针——在物理线程上迭代 OwnedComponents 会与游戏线程的修改产生竞争，并破坏附近的堆。
    if (CachedController && CachedController->bEnabled && CachedController->IsBound())
    {
        CachedController->ComputeAndApply(m_model, m_data, ControlSource);
        return;
    }

    // 默认路径：直接将控制值写入 d->ctrl
    for (auto& Elem : ActuatorIdMap)
    {
        if (UMjActuator* Actuator = Elem.Value)
        {
            int id = Actuator->GetMjID();
            if (id >= 0 && id < m_model->nu)
            {
                m_data->ctrl[id] = (mjtNum)Actuator->ResolveDesiredControl(ControlSource);
            }
        }
    }
}

// =========================================================================
// 蓝图运行时 API — 发现
// =========================================================================

TArray<FString> AMjArticulation::GetActuatorNames() const
{
    TArray<FString> Names;
    for (auto& Elem : ActuatorIdMap)
    {
        if (Elem.Value) Names.Add(Elem.Value->GetMjName());
    }
    return Names;
}

TArray<UMjActuator*> AMjArticulation::GetActuators() const
{
    TArray<UMjActuator*> Components;
    ActuatorIdMap.GenerateValueArray(Components);
    return Components;
}

TArray<FString> AMjArticulation::GetJointNames() const
{
    TArray<FString> Names;
    for (auto& Elem : JointIdMap)
    {
        if (Elem.Value) Names.Add(Elem.Value->GetMjName());
    }
    return Names;
}

TArray<UMjJoint*> AMjArticulation::GetJoints() const
{
    TArray<UMjJoint*> Components;
    JointIdMap.GenerateValueArray(Components);
    return Components;
}

TArray<FString> AMjArticulation::GetSensorNames() const
{
    TArray<FString> Names;
    for (auto& Elem : SensorIdMap)
    {
        if (Elem.Value) Names.Add(Elem.Value->GetMjName());
    }
    return Names;
}

TArray<UMjSensor*> AMjArticulation::GetSensors() const
{
    TArray<UMjSensor*> Components;
    SensorIdMap.GenerateValueArray(Components);
    return Components;
}

TArray<FString> AMjArticulation::GetBodyNames() const
{
    TArray<FString> Names;
    for (auto& Elem : BodyIdMap)
    {
        if (Elem.Value) Names.Add(Elem.Value->GetMjName());
    }
    return Names;
}

TArray<UMjBody*> AMjArticulation::GetBodies() const
{
    TArray<UMjBody*> Components;
    BodyIdMap.GenerateValueArray(Components);
    return Components;
}

void AMjArticulation::WakeAll()
{
    for (UMjBody* Body : GetBodies())
    {
        if (Body) Body->Wake();
    }
}

void AMjArticulation::SleepAll()
{
    for (UMjBody* Body : GetBodies())
    {
        if (Body) Body->Sleep();
    }
}

TArray<UMjFrame*> AMjArticulation::GetFrames() const
{
    TArray<UMjFrame*> Components;
    GetRuntimeComponents<UMjFrame>(Components);
    return Components;
}

TArray<UMjGeom*> AMjArticulation::GetGeoms() const
{
    TArray<UMjGeom*> Components;
    GeomIdMap.GenerateValueArray(Components);
    return Components;
}


UMjActuator* AMjArticulation::GetActuator(const FString& Name) const
{
    if (const auto* Ptr = ActuatorComponentMap.Find(Name)) return *Ptr;
    return nullptr;
}

UMjJoint* AMjArticulation::GetJoint(const FString& Name) const
{
    if (const auto* Ptr = JointComponentMap.Find(Name)) return *Ptr;
    return nullptr;
}

UMjSensor* AMjArticulation::GetSensor(const FString& Name) const
{
    if (const auto* Ptr = SensorComponentMap.Find(Name)) return *Ptr;
    return nullptr;
}

UMjBody* AMjArticulation::GetBody(const FString& Name) const
{
    if (const auto* Ptr = BodyComponentMap.Find(Name)) return *Ptr;
    return nullptr;
}

TArray<UMjTendon*> AMjArticulation::GetTendons() const
{
    TArray<UMjTendon*> Components;
    TendonIdMap.GenerateValueArray(Components);
    return Components;
}

TArray<FString> AMjArticulation::GetTendonNames() const
{
    TArray<FString> Names;
    for (auto& Elem : TendonIdMap)
    {
        if (Elem.Value) Names.Add(Elem.Value->GetMjName());
    }
    return Names;
}

UMjTendon* AMjArticulation::GetTendon(const FString& Name) const
{
    if (const auto* Ptr = TendonComponentMap.Find(Name)) return *Ptr;
    return nullptr;
}

TArray<UMjEquality*> AMjArticulation::GetEqualities() const
{
    TArray<UMjEquality*> Components;
    EqualityComponentMap.GenerateValueArray(Components);
    
    TArray<UMjEquality*> UniqueComponents;
    for (UMjEquality* E : Components) UniqueComponents.AddUnique(E);
    return UniqueComponents;
}

TArray<UMjKeyframe*> AMjArticulation::GetKeyframes() const
{
    TArray<UMjKeyframe*> Components;
    KeyframeComponentMap.GenerateValueArray(Components);
    
    TArray<UMjKeyframe*> UniqueComponents;
    for (UMjKeyframe* K : Components) UniqueComponents.AddUnique(K);
    return UniqueComponents;
}

TArray<FString> AMjArticulation::GetKeyframeNames() const
{
    TArray<FString> Names;
    TArray<UMjKeyframe*> Keyframes = GetKeyframes();
    for (UMjKeyframe* K : Keyframes)
    {
        if (K) Names.Add(K->MjName.IsEmpty() ? K->GetName() : K->MjName);
    }
    return Names;
}

bool AMjArticulation::ResetToKeyframe(const FString& KeyframeName)
{
    if (!m_model || !m_data) return false;

    // 通过名称找到关键帧索引
    int32 KeyId = -1;
    if (KeyframeName.IsEmpty())
    {
        KeyId = 0; // 默认使用第一个关键帧
    }
    else
    {
        // 通过前缀名称搜索（mjs_attach 会添加关节前缀）
        FString PrefixedName = m_prefix + KeyframeName;
        KeyId = mj_name2id(m_model, mjOBJ_KEY, TCHAR_TO_UTF8(*PrefixedName));
        if (KeyId < 0)
        {
            // 尝试不带前缀
            KeyId = mj_name2id(m_model, mjOBJ_KEY, TCHAR_TO_UTF8(*KeyframeName));
        }
    }

    if (KeyId < 0 || KeyId >= m_model->nkey)
    {
        UE_LOG(LogURLab, Warning, TEXT("[MjArticulation] ResetToKeyframe: '%s' not found on '%s'"),
            *KeyframeName, *GetName());
        return false;
    }

    // 从关键帧复制关节 qpos，跳过自由关节 DOF。
    // mj_resetDataKeyframe 还会设置自由关节（世界位置），
    // 这会传送机器人 — 我们只想设置关节角度。
    const mjtNum* KeyQpos = m_model->key_qpos + KeyId * m_model->nq;
    const mjtNum* KeyQvel = m_model->key_qvel + KeyId * m_model->nv;
    const mjtNum* KeyCtrl = m_model->key_ctrl + KeyId * m_model->nu;

    for (int32 j = 0; j < m_model->njnt; j++)
    {
        int32 JointType = m_model->jnt_type[j];
        if (JointType == mjJNT_FREE) continue; // 跳过自由关节

        int32 QposAdr = m_model->jnt_qposadr[j];
        int32 DofAdr = m_model->jnt_dofadr[j];

        // 复制 qpos（1 表示铰链/滑动，4 表示球型）
        int32 NqPos = (JointType == mjJNT_BALL) ? 4 : 1;
        for (int32 k = 0; k < NqPos; k++)
        {
            m_data->qpos[QposAdr + k] = KeyQpos[QposAdr + k];
        }

        // 复制 qvel（1 表示铰链/滑动，3 表示球型）
        int32 NvDof = (JointType == mjJNT_BALL) ? 3 : 1;
        for (int32 k = 0; k < NvDof; k++)
        {
            m_data->qvel[DofAdr + k] = KeyQvel[DofAdr + k];
        }
    }

    // 复制 ctrl（执行器不涉及自由关节）
    for (int32 i = 0; i < m_model->nu; i++)
    {
        m_data->ctrl[i] = KeyCtrl[i];
    }

    mj_forward(m_model, m_data);

    UE_LOG(LogURLab, Log, TEXT("[MjArticulation] Reset to keyframe '%s' (id=%d, joints only, freejoint preserved) on '%s'"),
        *KeyframeName, KeyId, *GetName());
    return true;
}

bool AMjArticulation::HoldKeyframe(const FString& KeyframeName)
{
    if (!m_model || !m_data) return false;

    // 查找关键帧
    TArray<UMjKeyframe*> Keyframes = GetKeyframes();
    UMjKeyframe* Target = nullptr;

    if (KeyframeName.IsEmpty() && Keyframes.Num() > 0)
    {
        Target = Keyframes[0];
    }
    else
    {
        for (UMjKeyframe* K : Keyframes)
        {
            if (K && (K->MjName == KeyframeName || K->GetName() == KeyframeName))
            {
                Target = K;
                break;
            }
        }
    }

    if (!Target)
    {
        UE_LOG(LogURLab, Warning, TEXT("[MjArticulation] HoldKeyframe: '%s' not found on '%s'"),
            *KeyframeName, *GetName());
        return false;
    }

    // 策略 1：可用的 ctrl 值 — 驱动执行器
    if (Target->bOverride_Ctrl && Target->Ctrl.Num() > 0)
    {
        HeldKeyframeCtrl = Target->Ctrl;
        bHoldViaQpos = false;
        bHoldingKeyframe = true;
        UE_LOG(LogURLab, Log, TEXT("[MjArticulation] Holding keyframe '%s' on '%s' via ctrl (%d values)"),
            *KeyframeName, *GetName(), HeldKeyframeCtrl.Num());
        return true;
    }

    // 策略 2：可用的 qpos — 每步直接注入到 d->qpos
    if (Target->bOverride_Qpos && Target->Qpos.Num() > 0)
    {
        HeldKeyframeQpos = Target->Qpos;
        bHoldViaQpos = true;
        bHoldingKeyframe = true;
        UE_LOG(LogURLab, Log, TEXT("[MjArticulation] Holding keyframe '%s' on '%s' via qpos injection (%d values)"),
            *KeyframeName, *GetName(), HeldKeyframeQpos.Num());
        return true;
    }

    UE_LOG(LogURLab, Warning, TEXT("[MjArticulation] HoldKeyframe: '%s' has no ctrl or qpos data"),
        *KeyframeName);
    return false;
}

void AMjArticulation::StopHoldKeyframe()
{
    bHoldingKeyframe = false;
    bHoldViaQpos = false;
    HeldKeyframeCtrl.Empty();
    HeldKeyframeQpos.Empty();
    UE_LOG(LogURLab, Log, TEXT("[MjArticulation] Stopped holding keyframe on '%s'"), *GetName());
}

UMjComponent* AMjArticulation::GetComponentByMjId(mjtObj type, int32 id) const
{
    switch (type)
    {
        case mjOBJ_BODY:     return GetBodyByMjId(id);
        case mjOBJ_GEOM:     return GetGeomByMjId(id);
        case mjOBJ_JOINT:    return JointIdMap.Contains(id) ? JointIdMap[id] : nullptr;
        case mjOBJ_SENSOR:   return SensorIdMap.Contains(id) ? SensorIdMap[id] : nullptr;
        case mjOBJ_ACTUATOR: return ActuatorIdMap.Contains(id) ? ActuatorIdMap[id] : nullptr;
        default:             return nullptr;
    }
}

UMjBody* AMjArticulation::GetBodyByMjId(int32 id) const
{
    if (const auto* Ptr = BodyIdMap.Find(id)) return *Ptr;
    return nullptr;
}

UMjGeom* AMjArticulation::GetGeomByMjId(int32 id) const
{
    if (const auto* Ptr = GeomIdMap.Find(id)) return *Ptr;
    return nullptr;
}

bool AMjArticulation::SetActuatorControl(const FString& ActuatorName, float Value)
{
    if (UMjActuator* A = GetActuator(ActuatorName))
    {
        A->SetControl(Value);
        return true;
    }
    return false;
}

FVector2D AMjArticulation::GetActuatorRange(const FString& ActuatorName) const
{
    if (UMjActuator* Act = GetActuator(ActuatorName))
    {
        return Act->GetControlRange();
    }
    return FVector2D(0.0f, 0.0f);
}

float AMjArticulation::GetJointAngle(const FString& JointName) const
{
    if (UMjJoint* J = GetJoint(JointName))
        return J->GetPosition();
    return 0.0f;
}

float AMjArticulation::GetSensorScalar(const FString& SensorName) const
{
    if (UMjSensor* S = GetSensor(SensorName))
        return S->GetScalarReading();
    return 0.0f;
}

TArray<float> AMjArticulation::GetSensorReading(const FString& SensorName) const
{
    if (UMjSensor* S = GetSensor(SensorName))
        return S->GetReading();
    return TArray<float>();
}


void AMjArticulation::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);
    
    if (bDrawDebugCollision)
    {
        DrawDebugCollision();
    }
    if (bDrawDebugJoints)
    {
        DrawDebugJoints();
    }
    if (bDrawDebugSites)
    {
        DrawDebugSites();
    }
}

void AMjArticulation::DrawDebugCollision()
{
    if (!m_model) return;

    float Multiplier = 100.0f;
    UWorld* World = GetWorld();
    if (!World) return;

    FColor DrawColor = FColor::Magenta;

	TArray<UMjGeom*> Geoms;
	GetRuntimeComponents<UMjGeom>(Geoms);

	for (UMjGeom* Geom : Geoms)
    {
        if (Geom && Geom->IsBound())
        {
            MjUtils::DrawDebugGeom(World, m_model, Geom->GetMj(), DrawColor, Multiplier);
        }
    }
}

void AMjArticulation::DrawDebugJoints()
{
    UWorld* World = GetWorld();
    if (!World) return;

    TArray<UMjJoint*> Joints;
    GetRuntimeComponents<UMjJoint>(Joints);

    for (UMjJoint* Joint : Joints)
    {
        if (!Joint) continue;

        int MjType;
        FVector Anchor, Axis;
        bool bLimited;
        float RangeMin = 0.0f, RangeMax = 0.0f;
        float CurrentPos = NAN;
        float RefPos = NAN;

        if (Joint->IsBound() && m_model)
        {
            // 运行时模式：使用编译的模型数据
            const JointView& JV = Joint->GetMj();
            MjType = JV.type;
            Anchor = Joint->GetWorldAnchor();
            Axis = Joint->GetWorldAxis();

            if (JV.range)
            {
                RangeMin = (float)JV.range[0];
                RangeMax = (float)JV.range[1];
                bLimited = (RangeMin != 0.0f || RangeMax != 0.0f);
            }
            else
            {
                bLimited = false;
            }

            // 当前 1-DOF 关节的位置
            if ((MjType == mjJNT_HINGE || MjType == mjJNT_SLIDE) && JV.qpos)
            {
                CurrentPos = (float)JV.qpos[0];
            }

            // 参考位置（qpos0）来自编译模型
            int qposAdr = m_model->jnt_qposadr[JV.id];
            RefPos = (float)m_model->qpos0[qposAdr];

            // 滑动关节：MuJoCo 以米为单位存储，DrawDebugJoint 期望以厘米为单位
            if (MjType == mjJNT_SLIDE)
            {
                RangeMin *= 100.0f;
                RangeMax *= 100.0f;
                if (!FMath::IsNaN(CurrentPos)) CurrentPos *= 100.0f;
                if (!FMath::IsNaN(RefPos)) RefPos *= 100.0f;
            }
        }
        else
        {
            // 编辑器预览模式：使用解析的默认值
            EMjJointType ResolvedType = Joint->GetResolvedType();
            switch (ResolvedType)
            {
                case EMjJointType::Hinge: MjType = mjJNT_HINGE; break;
                case EMjJointType::Slide: MjType = mjJNT_SLIDE; break;
                default: continue;
            }

            Anchor = Joint->GetComponentLocation();
            Axis = Joint->GetComponentTransform().TransformVectorNoScale(Joint->GetResolvedAxis());
            FVector2D Range = Joint->GetResolvedRange();
            RangeMin = Range.X;
            RangeMax = Range.Y;
            bLimited = Joint->GetResolvedLimited() || (RangeMin != 0.0f || RangeMax != 0.0f);

            // 参考来自组件属性
            if (Joint->bOverride_Ref)
            {
                RefPos = Joint->Ref;
            }
            else
            {
                RefPos = 0.0f; // MuJoCo 的默认值
            }
        }

        if (MjType != mjJNT_HINGE && MjType != mjJNT_SLIDE) continue;

        MjUtils::DrawDebugJoint(World, Anchor, Axis, MjType, bLimited, RangeMin, RangeMax, CurrentPos, RefPos);
    }
}

void AMjArticulation::DrawDebugSites()
{
    UWorld* World = GetWorld();
    if (!World) return;

    TArray<UMjSite*> Sites;
    GetRuntimeComponents<UMjSite>(Sites);

    for (UMjSite* Site : Sites)
    {
        if (!Site) continue;

        FVector Pos;
        float Radius;
        FColor Color;

        if (Site->IsBound())
        {
            // 运行时：使用编译的模型数据
            const SiteView& SV = Site->GetMj();
            if (!SV.xpos) continue;

            Pos = MjUtils::MjToUEPosition(SV.xpos);
            Radius = (SV.size) ? (float)SV.size[0] * 100.0f : 1.0f; // meters → cm
            Color = (SV.rgba) ? FColor(
                (uint8)(SV.rgba[0] * 255), (uint8)(SV.rgba[1] * 255),
                (uint8)(SV.rgba[2] * 255), 200) : FColor(128, 128, 128, 200);
        }
        else
        {
            // 编辑器：使用组件变换和属性
            Pos = Site->GetComponentLocation();
            Radius = Site->Size.X * 100.0f; // UMjSite上的尺寸是以米为单位
            Color = Site->Rgba.ToFColor(true);
            Color.A = 200;
        }

        // 限制半径以提高可见性
        Radius = FMath::Max(Radius, 0.5f);

        // 在位点位置绘制十字准线
        float CrossSize = FMath::Max(Radius * 2.0f, 2.0f);
        DrawDebugPoint(World, Pos, 6.0f, Color, false, -1);
        DrawDebugLine(World, Pos - FVector(CrossSize, 0, 0), Pos + FVector(CrossSize, 0, 0), Color, false, -1, 0, 1.0f);
        DrawDebugLine(World, Pos - FVector(0, CrossSize, 0), Pos + FVector(0, CrossSize, 0), Color, false, -1, 0, 1.0f);
        DrawDebugLine(World, Pos - FVector(0, 0, CrossSize), Pos + FVector(0, 0, CrossSize), Color, false, -1, 0, 1.0f);
    }
}

void AMjArticulation::ToggleGroup3Visibility()
{
    bShowGroup3 = !bShowGroup3;
    UpdateGroup3Visibility();
}

#if WITH_EDITOR
void AMjArticulation::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
    Super::PostEditChangeProperty(PropertyChangedEvent);

    FName PropertyName = (PropertyChangedEvent.Property != nullptr) ? PropertyChangedEvent.Property->GetFName() : NAME_None;

    if (PropertyName == GET_MEMBER_NAME_CHECKED(AMjArticulation, bShowGroup3))
    {
        UpdateGroup3Visibility();
    }

}

void AMjArticulation::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);
    UpdateGroup3Visibility();

    // 注册蓝图编译回调
    if (bValidateOnBlueprintCompile && !BlueprintCompiledHandle.IsValid())
    {
        if (UBlueprint* BP = UBlueprint::GetBlueprintFromClass(GetClass()))
        {
            BlueprintCompiledHandle = BP->OnCompiled().AddUObject(this, &AMjArticulation::OnBlueprintCompiled);
        }
    }
}


// 蓝图编译后同步名验证铰链相关组件 的回调函数
void AMjArticulation::OnBlueprintCompiled(UBlueprint* Blueprint)
{
    // 从 简单构造脚本（SimpleConstructionScript, SCS）层级同步 Mujoco 默认的（MjDefault）类名（ClassName）和父类名（ParentClassName），
    // 并在用户创建的非默认组件上自动填充 Mujoco 名（MjName），
    // 当它没有被明确设置时（例如由 XML 导入器设置），
    // 它会将原始 MJCF 的 name 属性写入 MjName
    if (Blueprint && Blueprint->SimpleConstructionScript)
    {
        USimpleConstructionScript* SCS = Blueprint->SimpleConstructionScript;
        for (USCS_Node* Node : SCS->GetAllNodes())
        {
            UMjComponent* MjComp = Cast<UMjComponent>(Node->ComponentTemplate);
            if (!MjComp) continue;

            if (UMjDefault* DefComp = Cast<UMjDefault>(MjComp))
            {
                // 从变量名同步类名（ClassName）
                FString VarName = Node->GetVariableName().ToString();
                if (DefComp->ClassName != VarName)
                {
                    DefComp->ClassName = VarName;
                }

                // 从简单构造脚本（SCS）的父层级同步 父类名（ParentClassName）
                USCS_Node* ParentNode = SCS->FindParentNode(Node);
                if (ParentNode)
                {
                    if (UMjDefault* ParentDef = Cast<UMjDefault>(ParentNode->ComponentTemplate))
                    {
                        FString ParentVarName = ParentNode->GetVariableName().ToString();
                        if (DefComp->ParentClassName != ParentVarName)
                        {
                            DefComp->ParentClassName = ParentVarName;
                        }
                    }
                    else
                    {
                        // 父类不是 UMjDefault（例如 DefaultsRoot）——没有父类
                        DefComp->ParentClassName.Empty();
                    }
                }
            }
            else
            {
                // 非默认 MjComponent：
                // 仅当为空时才从简单构造脚本（SCS）变量名同步 MjName。
                // 导入的组件其 MjName 是从 MJCF 的 name= 属性设置的，这里不能被覆盖，
                // 因为 SCS 的唯一性可能已经使变量名不再歧义
                // （例如，当默认类已经使用了“waist”时，关节“waist”会变成“waist1”），
                // 而 MjName 是 MuJoCo 规格查找的真实来源。
                if (MjComp->MjName.IsEmpty())
                {
                    MjComp->MjName = Node->GetVariableName().ToString();
                }
            }
        }
    }

    if (bValidateOnBlueprintCompile)
    {
        ValidateSpec();  // 在隔离的临时规范上执行编译流程，帮助尽快发现 Mujoco 规范问题
    }
}

void AMjArticulation::ValidateSpec()
{
    // 创建一个临时规范，导出这个铰链的组件，并尝试编译。
    // 这类似于运行时的编译流程，但是在隔离的环境中进行。
    mjSpec* TempSpec = mj_parseXMLString("<mujoco><worldbody/></mujoco>", nullptr, nullptr, 0);
    if (!TempSpec)
    {
        UE_LOG(LogURLab, Error, TEXT("[ValidateSpec] Failed to create temporary spec"));
        return;
    }

    mjVFS TempVFS;
    mj_defaultVFS(&TempVFS);

    // 运行与运行时使用相同的 Setup 路径
    Setup(TempSpec, &TempVFS);

    // 尝试编译
    mjModel* TempModel = mj_compile(TempSpec, &TempVFS);
    if (TempModel)
    {
        UE_LOG(LogURLab, Log, TEXT("[ValidateSpec] '%s': Valid (%d bodies, %d joints, %d actuators)"),
            *GetName(), TempModel->nbody, TempModel->njnt, TempModel->nu);
        mj_deleteModel(TempModel);
    }
    else
    {
        const char* SpecError = mjs_getError(TempSpec);
        FString ErrorMsg = SpecError ? UTF8_TO_TCHAR(SpecError) : TEXT("Unknown error");
        UE_LOG(LogURLab, Error, TEXT("[ValidateSpec] '%s': FAILED — %s"), *GetName(), *ErrorMsg);

        FMessageDialog::Open(EAppMsgType::Ok,
            FText::Format(
                NSLOCTEXT("URLab", "ValidateSpecError", "MuJoCo Validation Failed for '{0}':\n\n{1}"),
                FText::FromString(GetName()),
                FText::FromString(ErrorMsg)));
    }

    mj_deleteSpec(TempSpec);
    mj_deleteVFS(&TempVFS);
}
#endif

void AMjArticulation::UpdateGroup3Visibility()
{
    // 1. 收集所有默认值以支持查找
    TMap<FString, UMjDefault*> DefaultMap;
    TArray<UMjDefault*> Defaults;
    GetComponents<UMjDefault>(Defaults);
    for (UMjDefault* Def : Defaults)
    {
        if (!Def->ClassName.IsEmpty())
        {
            DefaultMap.Add(Def->ClassName, Def);
        }
    }

    // 构建 ClassName -> Group 的映射，通过查找定义默认值的几何体
    TMap<FString, int> DefaultGroupMap;
    TArray<UMjDefault*> ArticulationDefaults;
    GetComponents<UMjDefault>(ArticulationDefaults);

    for (UMjDefault* Def : ArticulationDefaults)
    {
        if (!Def) continue;

        // 查找附加到此默认值的几何体
        TArray<USceneComponent*> DefaultChildren;
        Def->GetChildrenComponents(false, DefaultChildren); // Geoms 应该是默认值的直接子级

        for (USceneComponent* Child : DefaultChildren)
        {
            if (UMjGeom* DefaultGeom = Cast<UMjGeom>(Child))
            {
                if (DefaultGeom->bOverride_Group)
                {
                    DefaultGroupMap.Add(Def->ClassName, DefaultGeom->Group);
                }
            }
        }
    }

    // 2. 遍历所有 UMjGeom 组件
    TArray<UMjGeom*> ArticulationGeoms;
    GetComponents<UMjGeom>(ArticulationGeoms);

    int Count = 0;
    for (UMjGeom* Geom : ArticulationGeoms)
    {
        // 跳过用于默认值的模板几何体
        if (Geom->bIsDefault)
        {
            continue;
        }

        // 解析有效组
        int EffectiveGroup = Geom->Group;

        // 如果几何体没有覆盖组，则检查默认值
        if (!Geom->bOverride_Group && !Geom->MjClassName.IsEmpty())
        {
            if (int* FoundGroup = DefaultGroupMap.Find(Geom->MjClassName))
            {
                EffectiveGroup = *FoundGroup;
            }
        }

        // 应用基于 Group 3 和 bShowGroup3 的可见性
        if (EffectiveGroup == 3)
        {
            Geom->SetGeomVisibility(bShowGroup3);
            Count++;
        }
    }

    UE_LOG(LogURLab, Log, TEXT("UpdateGroup3Visibility('%s'): updated %d Group-3 meshes (Show=%d)"), *GetName(), Count, bShowGroup3);
}

