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

    // 1. Create the Scene Component to act as the Root
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

    // Auto-create TwistController if none exists
    if (!FindComponentByClass<UMjTwistController>())
    {
        UMjTwistController* TwistCtrl = NewObject<UMjTwistController>(this, TEXT("TwistController"));

        // Load default input assets from plugin content
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

    // Add twist mapping context
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

    // Attach a spring arm + camera to the root body so the camera follows physics
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

        // Tag them so we can clean up on unpossess
        Arm->ComponentTags.Add(TEXT("PossessCamera"));
        Cam->ComponentTags.Add(TEXT("PossessCamera"));

        UE_LOG(LogURLab, Log, TEXT("Attached follow camera to root body '%s'"), *RootBody->GetName());
    }
}

void AMjArticulation::UnPossessed()
{
    AController* OldController = GetController();
    APlayerController* PC = Cast<APlayerController>(OldController);

    // Remove twist mapping context and zero state
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

    // Remove the follow camera components we created on possession
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

    // Apply this articulation's simulation options to the child spec.
    // mjs_attach will merge these into the root spec at compile time.
    SimOptions.ApplyToSpec(m_ChildSpec);

    m_prefix = GetName() + TEXT("_");
    
    m_wrapper = new FMujocoSpecWrapper(m_ChildSpec, m_vfs);
    m_wrapper->MeshCacheSubDir = GetClass()->GetName();

    // 1b. Auto-resolve bIsDefault and sync ParentClassName from hierarchy.
    // This ensures correctness even if OnBlueprintCompiled hasn't run.
    {
        TArray<UMjDefault*> AllDefaults;
        GetComponents<UMjDefault>(AllDefaults);
        for (UMjDefault* Def : AllDefaults)
        {
            Def->bIsDefault = true;

            // Sync ParentClassName from attachment parent if attached to another UMjDefault.
            // If not (e.g. attached to root), keep existing ParentClassName as fallback
            // for programmatically created defaults that set it explicitly.
            if (UMjDefault* ParentDef = Cast<UMjDefault>(Def->GetAttachParent()))
            {
                Def->ParentClassName = ParentDef->ClassName;
            }
            else if (Def->ParentClassName.IsEmpty())
            {
                // No parent default in hierarchy and no explicit ParentClassName — root default
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

    // 2. Process Defaults in hierarchy order (parents before children) so that
    //    mjs_findDefault can resolve parent classes during AddDefault.
    {
        TArray<UMjDefault*> AllDefaults;
        GetComponents<UMjDefault>(AllDefaults);

        // Find root defaults (those whose parent is NOT a UMjDefault)
        // and process them recursively, depth-first
        TFunction<void(UMjDefault*)> ProcessDefaultTree = [&](UMjDefault* Def)
        {
            m_wrapper->AddDefault(Def);
            // Find child defaults attached to this one
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
            // Only start from roots (parent is not a UMjDefault)
            UMjDefault* ParentDef = Cast<UMjDefault>(Def->GetAttachParent());
            if (!ParentDef)
            {
                ProcessDefaultTree(Def);
            }
        }
    }
    
    // 3. Find UMjWorldBody and build body hierarchy normally (into child spec)
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

     // Worldbody-level IMjSpecElement children (sites, etc. attached directly to
     // <worldbody>) need to register against the child spec's world body so
     // downstream references (tendons wrapping worldbody sites, for instance)
     // resolve at compile time.
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

    // 3b. Register flexcomp components (can be worldbody children or body children)
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

    // 4. Add Tendons (into child spec, after bodies so joint names are set)
    TArray<UMjTendon*> Tendons;
    GetComponents<UMjTendon>(Tendons);
    for (UMjTendon* Tendon : Tendons)
    {
        if (Tendon && !Tendon->bIsDefault)
            Tendon->RegisterToSpec(*m_wrapper);
    }

    // 5. Add Sensors and Actuators (into child spec, after tendons strings might be referenced)
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
        // Get error from both specs
        const char* childErr = mjs_getError(m_ChildSpec);
        const char* rootErr = mjs_getError(Spec);
        bAttachFailed = true;
        UE_LOG(LogURLab, Error, TEXT("[mjs_attach] '%s' — FAILED (returned null). Child spec elements will not appear in compiled model."), *GetName());
        UE_LOG(LogURLab, Error, TEXT("[mjs_attach] '%s' — Child spec error: %hs"), *GetName(), childErr ? childErr : "(none)");
        UE_LOG(LogURLab, Error, TEXT("[mjs_attach] '%s' — Root spec error: %hs"), *GetName(), rootErr ? rootErr : "(none)");

        // Log child spec body count for diagnosis
        mjsBody* childWorld = mjs_findBody(m_ChildSpec, "world");
        UE_LOG(LogURLab, Error, TEXT("[mjs_attach] '%s' — Child spec world body: %p, child spec element: %p"),
            *GetName(), childWorld, m_ChildSpec ? m_ChildSpec->element : nullptr);
    }
    else
    {
        UE_LOG(LogURLab, Log, TEXT("[mjs_attach] '%s' — SUCCESS (returned %p). Prefix='%s'"), *GetName(), attachResult, *m_prefix);
        // Child spec elements have been moved into the root spec — the child spec is now consumed.
        // Null it out so EndPlay doesn't try to double-free it.
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
    GetComponents(ComponentClass, AllComponents);

    UWorld* MyWorld = GetWorld();
    for (USceneComponent* Comp : AllComponents)
    {
        // Skip SCS template components that leak into PIE
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

    // Build component-name maps for Blueprint API (O(1) look up by name)
    // We use the MuJoCo-side name (GetMjName()) as the key for consistency with UI/data.
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

    // Build MuJoCo ID maps (O(1) resolve from ID to Component)
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

    // Bind any articulation controller component (PD, passthrough, or user-custom)
    // and cache it so ApplyControls (physics thread) doesn't have to iterate
    // OwnedComponents on every step — that race causes crashes under flex load.
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
    // Thread safety: ActuatorIdMap is built during PostSetup (game thread) and
    // only read here (physics thread). The map is never modified after construction.
    // RunMujocoAsync starts after PostSetup completes, guaranteeing visibility.
    if (!m_model || !m_data) return;

    // If holding a keyframe, override normal control flow
    if (bHoldingKeyframe)
    {
        if (bHoldViaQpos && HeldKeyframeQpos.Num() > 0)
        {
            // Direct qpos injection — kinematic hold, bypasses actuators.
            // Skip freejoint DOFs to preserve world position.
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
            // Ctrl-based hold — actuators drive to target positions
            int32 Count = FMath::Min(HeldKeyframeCtrl.Num(), m_model->nu);
            for (int32 i = 0; i < Count; i++)
            {
                m_data->ctrl[i] = (mjtNum)HeldKeyframeCtrl[i];
            }
        }
        return;
    }

    // Delegate to custom controller if present and active. Use the cached
    // pointer from PostSetup — iterating OwnedComponents on the physics
    // thread races against game-thread mutations and corrupts nearby heap.
    if (CachedController && CachedController->bEnabled && CachedController->IsBound())
    {
        CachedController->ComputeAndApply(m_model, m_data, ControlSource);
        return;
    }

    // Default path: write control values directly to d->ctrl
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
// Blueprint Runtime API — Discovery
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

    // Find the keyframe index by name
    int32 KeyId = -1;
    if (KeyframeName.IsEmpty())
    {
        KeyId = 0; // Default to first keyframe
    }
    else
    {
        // Search by prefixed name (mjs_attach prepends the articulation prefix)
        FString PrefixedName = m_prefix + KeyframeName;
        KeyId = mj_name2id(m_model, mjOBJ_KEY, TCHAR_TO_UTF8(*PrefixedName));
        if (KeyId < 0)
        {
            // Try without prefix
            KeyId = mj_name2id(m_model, mjOBJ_KEY, TCHAR_TO_UTF8(*KeyframeName));
        }
    }

    if (KeyId < 0 || KeyId >= m_model->nkey)
    {
        UE_LOG(LogURLab, Warning, TEXT("[MjArticulation] ResetToKeyframe: '%s' not found on '%s'"),
            *KeyframeName, *GetName());
        return false;
    }

    // Copy joint qpos from the keyframe, skipping freejoint DOFs.
    // mj_resetDataKeyframe would also set the freejoint (world position),
    // which teleports the robot — we only want to set joint angles.
    const mjtNum* KeyQpos = m_model->key_qpos + KeyId * m_model->nq;
    const mjtNum* KeyQvel = m_model->key_qvel + KeyId * m_model->nv;
    const mjtNum* KeyCtrl = m_model->key_ctrl + KeyId * m_model->nu;

    for (int32 j = 0; j < m_model->njnt; j++)
    {
        int32 JointType = m_model->jnt_type[j];
        if (JointType == mjJNT_FREE) continue; // Skip freejoints

        int32 QposAdr = m_model->jnt_qposadr[j];
        int32 DofAdr = m_model->jnt_dofadr[j];

        // Copy qpos (1 for hinge/slide, 4 for ball)
        int32 NqPos = (JointType == mjJNT_BALL) ? 4 : 1;
        for (int32 k = 0; k < NqPos; k++)
        {
            m_data->qpos[QposAdr + k] = KeyQpos[QposAdr + k];
        }

        // Copy qvel (1 for hinge/slide, 3 for ball)
        int32 NvDof = (JointType == mjJNT_BALL) ? 3 : 1;
        for (int32 k = 0; k < NvDof; k++)
        {
            m_data->qvel[DofAdr + k] = KeyQvel[DofAdr + k];
        }
    }

    // Copy ctrl (actuators have no freejoint involvement)
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

    // Find keyframe
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

    // Strategy 1: ctrl values available — drive actuators
    if (Target->bOverride_Ctrl && Target->Ctrl.Num() > 0)
    {
        HeldKeyframeCtrl = Target->Ctrl;
        bHoldViaQpos = false;
        bHoldingKeyframe = true;
        UE_LOG(LogURLab, Log, TEXT("[MjArticulation] Holding keyframe '%s' on '%s' via ctrl (%d values)"),
            *KeyframeName, *GetName(), HeldKeyframeCtrl.Num());
        return true;
    }

    // Strategy 2: qpos available — inject directly into d->qpos each step
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
            // Runtime mode: use compiled model data
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

            // Current position for 1-DOF joints
            if ((MjType == mjJNT_HINGE || MjType == mjJNT_SLIDE) && JV.qpos)
            {
                CurrentPos = (float)JV.qpos[0];
            }

            // Reference position (qpos0) from compiled model
            int qposAdr = m_model->jnt_qposadr[JV.id];
            RefPos = (float)m_model->qpos0[qposAdr];

            // Slide joints: MuJoCo stores in meters, DrawDebugJoint expects cm
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
            // Editor-preview mode: use resolved defaults
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

            // Ref from component property
            if (Joint->bOverride_Ref)
            {
                RefPos = Joint->Ref;
            }
            else
            {
                RefPos = 0.0f; // MuJoCo default
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
            // Runtime: use compiled model data
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
            // Editor: use component transform and properties
            Pos = Site->GetComponentLocation();
            Radius = Site->Size.X * 100.0f; // Size is in meters on UMjSite
            Color = Site->Rgba.ToFColor(true);
            Color.A = 200;
        }

        // Clamp radius for visibility
        Radius = FMath::Max(Radius, 0.5f);

        // Draw cross-hair at site location
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

    // Register for Blueprint compile callback
    if (bValidateOnBlueprintCompile && !BlueprintCompiledHandle.IsValid())
    {
        if (UBlueprint* BP = UBlueprint::GetBlueprintFromClass(GetClass()))
        {
            BlueprintCompiledHandle = BP->OnCompiled().AddUObject(this, &AMjArticulation::OnBlueprintCompiled);
        }
    }
}

void AMjArticulation::OnBlueprintCompiled(UBlueprint* Blueprint)
{
    // Sync MjDefault ClassName and ParentClassName from SCS hierarchy, and
    // auto-populate MjName on user-authored non-Default components from their
    // SCS variable name when it hasn't been set explicitly (e.g. by the XML
    // importer, which writes the raw MJCF name= attribute into MjName).
    if (Blueprint && Blueprint->SimpleConstructionScript)
    {
        USimpleConstructionScript* SCS = Blueprint->SimpleConstructionScript;
        for (USCS_Node* Node : SCS->GetAllNodes())
        {
            UMjComponent* MjComp = Cast<UMjComponent>(Node->ComponentTemplate);
            if (!MjComp) continue;

            if (UMjDefault* DefComp = Cast<UMjDefault>(MjComp))
            {
                // Sync ClassName from variable name
                FString VarName = Node->GetVariableName().ToString();
                if (DefComp->ClassName != VarName)
                {
                    DefComp->ClassName = VarName;
                }

                // Sync ParentClassName from SCS parent hierarchy
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
                        // Parent is not a UMjDefault (e.g. DefaultsRoot) — no parent class
                        DefComp->ParentClassName.Empty();
                    }
                }
            }
            else
            {
                // Non-Default MjComponent: sync MjName from SCS variable name
                // only when empty. Imported components have MjName set from
                // the MJCF name= attribute and must not be overwritten here,
                // because SCS uniqueness may have disambiguated the variable
                // name (e.g. joint "waist" -> "waist1" when a Default class
                // already claimed "waist"), and MjName is the source of truth
                // for the MuJoCo spec lookup.
                if (MjComp->MjName.IsEmpty())
                {
                    MjComp->MjName = Node->GetVariableName().ToString();
                }
            }
        }
    }

    if (bValidateOnBlueprintCompile)
    {
        ValidateSpec();
    }
}

void AMjArticulation::ValidateSpec()
{
    // Create a temporary spec, export this articulation's components, and try to compile.
    // This mirrors the runtime compile pipeline but in isolation.
    mjSpec* TempSpec = mj_parseXMLString("<mujoco><worldbody/></mujoco>", nullptr, nullptr, 0);
    if (!TempSpec)
    {
        UE_LOG(LogURLab, Error, TEXT("[ValidateSpec] Failed to create temporary spec"));
        return;
    }

    mjVFS TempVFS;
    mj_defaultVFS(&TempVFS);

    // Run the same Setup path that the runtime uses
    Setup(TempSpec, &TempVFS);

    // Attempt compile
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
    // 1. Gather all Defaults to support lookups
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

    // Build a map of ClassName -> Group by finding geoms that define defaults
    TMap<FString, int> DefaultGroupMap;
    TArray<UMjDefault*> ArticulationDefaults;
    GetComponents<UMjDefault>(ArticulationDefaults);

    for (UMjDefault* Def : ArticulationDefaults)
    {
        if (!Def) continue;

        // Find geoms attached to this default
        TArray<USceneComponent*> DefaultChildren;
        Def->GetChildrenComponents(false, DefaultChildren); // Geoms should be direct children of the default

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

    // 2. Iterate over ALL UMjGeom components
    TArray<UMjGeom*> ArticulationGeoms;
    GetComponents<UMjGeom>(ArticulationGeoms);

    int Count = 0;
    for (UMjGeom* Geom : ArticulationGeoms)
    {
        // Skip template geoms used for defaults
        if (Geom->bIsDefault)
        {
            continue;
        }

        // Resolve effective Group
        int EffectiveGroup = Geom->Group;

        // If geom doesn't override Group, check defaults
        if (!Geom->bOverride_Group && !Geom->MjClassName.IsEmpty())
        {
            if (int* FoundGroup = DefaultGroupMap.Find(Geom->MjClassName))
            {
                EffectiveGroup = *FoundGroup;
            }
        }

        // Apply visibility based on Group 3 and bShowGroup3
        if (EffectiveGroup == 3)
        {
            Geom->SetGeomVisibility(bShowGroup3);
            Count++;
        }
    }

    UE_LOG(LogURLab, Log, TEXT("UpdateGroup3Visibility('%s'): updated %d Group-3 meshes (Show=%d)"), *GetName(), Count, bShowGroup3);
}

