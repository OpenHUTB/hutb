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

#include "MuJoCo/Components/Bodies/MjBody.h"
#include "MuJoCo/Components/Bodies/MjFrame.h"
#include "Utils/URLabLogging.h"
#include "MuJoCo/Components/Geometry/MjGeom.h"
#include "MuJoCo/Components/Physics/MjInertial.h"
#include "MuJoCo/Components/Geometry/MjSite.h"
#include "MuJoCo/Components/Joints/MjJoint.h"
#include "MuJoCo/Core/Spec/MjSpecWrapper.h"
#include "MuJoCo/Utils/MjXmlUtils.h"
#include "MuJoCo/Utils/MjUtils.h"
#include "MuJoCo/Utils/MjOrientationUtils.h"
#include "XmlNode.h"
#include "PhysicsEngine/BodySetup.h"

UMjBody::UMjBody()
{
	PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.bStartWithTickEnabled = false;

	m_BodyView = BodyView();

}

void UMjBody::BeginPlay()
{
	Super::BeginPlay();
}

void UMjBody::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    if (m_IsSetup)
    {
        if (bDrivenByUnreal && m_MocapPos && m_MocapQuat)
        {
        	MjUtils::UEToMjPosition(GetComponentLocation(), m_MocapPos);
        	MjUtils::UEToMjRotation(GetComponentQuat(), m_MocapQuat);
        }
        else
        {
            if (!m_BodyView.xpos || !m_BodyView.xquat)
            {
                UE_LOG(LogURLabBind, Warning, TEXT("MjBody::TickComponent - Body '%s' has null xpos/xquat (bind failed). Disabling tick."), *GetName());
                m_IsSetup = false;
                SetComponentTickEnabled(false);
                return;
            }

            // 线程安全：xpos/xquat 读取操作无需加锁。
            // 在 x86-64 架构上，对齐的双读操作是原子性的（不会出现撕裂）。
            // 最坏的情况是 1 帧的位置/旋转不同步，这种不同步在视觉上是无法察觉的。
            // 这与 MuJoCo Simulate 自身的线程模型相符。
    	    FVector MuJoCoWorldPos = MjUtils::MjToUEPosition(m_BodyView.xpos);
    	    FQuat MuJoCoWorldQuat = MjUtils::MjToUERotation(m_BodyView.xquat);
    	    
            FVector CorrectedPos = MuJoCoWorldPos;
            
            // 仅当使用快速转换（Unreal -> MuJoCo 流程枢轴问题）时才应用网格枢轴偏移校正。
            if (bIsQuickConverted)
            {
    	        // MuJoCo 的 xpos 位于身体中心，但 UE 网格可能存在偏离中心的枢轴点。
        	    FVector OffsetVector = MuJoCoWorldQuat.RotateVector(m_MeshPivotOffset);
        	    CorrectedPos = MuJoCoWorldPos - OffsetVector;
            }
    
    	    SetWorldLocationAndRotation(CorrectedPos, MuJoCoWorldQuat);
        }
    }
}


void UMjBody::Setup(USceneComponent* Parent, mjsBody* ParentBody, FMujocoSpecWrapper* Wrapper)
{
    FTransform TargetTransform;
    
    bool bIsAttachingToWorld = (ParentBody && mjs_getId(ParentBody->element) == 0); 
    
    if (bIsAttachingToWorld)
    {
        TargetTransform = GetComponentTransform();
    }
    else
    {
        TargetTransform = GetRelativeTransform();
    }
    
    FString NameToRegister = MjName.IsEmpty() ? GetName() : MjName;
	mjsBody* BodyToAttachTo = Wrapper->CreateBody(
		NameToRegister,
		ParentBody,
		TargetTransform
	);
    if(BodyToAttachTo)
    {
        m_SpecElement = BodyToAttachTo->element;
        if (bOverride_Gravcomp)
        {
            BodyToAttachTo->gravcomp = Gravcomp;
        }
        
        
    }

    if (bDrivenByUnreal && BodyToAttachTo)
    {
        BodyToAttachTo->mocap = 1;
    }

    // 睡眠策略（MuJoCo 3.4+）。仅在非默认设置下写入，否则全局选项生效。
    if (BodyToAttachTo && SleepPolicy != EMjBodySleepPolicy::Default)
    {
        BodyToAttachTo->sleep = static_cast<mjtSleepPolicy>(static_cast<uint8>(SleepPolicy));
    }
    
    if (bOverride_ChildClassName)
    {

        mjs_setString(BodyToAttachTo->childclass, TCHAR_TO_UTF8(*ChildClassName));
    }
	m_Root = Parent;


	TArray<USceneComponent*> DirectChildren = GetAttachChildren();

	for (USceneComponent* CurrentComponent : DirectChildren)
	{
		if (UMjBody* MjBodyComp = Cast<UMjBody>(CurrentComponent))
		{
			UE_LOG(LogURLab, Verbose, TEXT("LEVEL N: Detected MjsBody: %s. Creating external articulated body."),
			       *MjBodyComp->GetName());
			m_Children.Add(MjBodyComp);
			MjBodyComp->Setup(this, BodyToAttachTo, Wrapper);
            continue; 
		}

		if (UMjFrame* MjFrameComp = Cast<UMjFrame>(CurrentComponent))
		{
			UE_LOG(LogURLab, Verbose, TEXT("LEVEL N: Detected MjFrame: %s. Creating coordinate frame."),
			       *MjFrameComp->GetName());
			MjFrameComp->Setup(this, BodyToAttachTo, Wrapper);
			continue;
		}

        if (CurrentComponent->GetClass()->ImplementsInterface(UMjSpecElement::StaticClass()))
        {
             IMjSpecElement* SpecElem = Cast<IMjSpecElement>(CurrentComponent);
             if (SpecElem)
             {
                 if (UMjGeom* MjGeomComp = Cast<UMjGeom>(CurrentComponent))
                 {
                    if (BodyToAttachTo && MjGeomComp->Type == EMjGeomType::Mesh)
				    {
					    TArray<USceneComponent*> GeomChildren;
					    MjGeomComp->GetChildrenComponents(true, GeomChildren);
					    
					    for(USceneComponent* ChildOfGeom : GeomChildren)
					    {
						    if (UStaticMeshComponent* SMC = Cast<UStaticMeshComponent>(ChildOfGeom))
						    {
							    Wrapper->PrepareMeshForMuJoCo(SMC, MjGeomComp->bComplexMeshRequired);
							    break;
						    }
					    }
				    }
                 }

                 if (UMjComponent* MjComp = Cast<UMjComponent>(CurrentComponent))
                 {
                     if (!MjComp->bIsDefault)
                     {
                         SpecElem->RegisterToSpec(*Wrapper, BodyToAttachTo);
                     }
                 }
                 else
                 {
                     SpecElem->RegisterToSpec(*Wrapper, BodyToAttachTo);
                 }

                 m_SpecElements.Emplace(CurrentComponent);

                 if (UMjGeom* Geom = Cast<UMjGeom>(CurrentComponent))
                 {
                     m_Geoms.Add(Geom);
                 }
                 else if (UMjJoint* Joint = Cast<UMjJoint>(CurrentComponent))
                 {
                     m_Joints.Add(Joint);
                 }
                 else if (UMjSensor* Sensor = Cast<UMjSensor>(CurrentComponent))
                 {
                     m_Sensors.Add(Sensor);
                 }
                 else if (UMjActuator* Actuator = Cast<UMjActuator>(CurrentComponent))
                 {
                     m_Actuators.Add(Actuator);
                 }
             }
        }
	}
	
	
	
}

void UMjBody::ImportFromXml(const FXmlNode* Node)
{
    FMjCompilerSettings DefaultSettings;
    ImportFromXml(Node, DefaultSettings);
}

void UMjBody::ImportFromXml(const FXmlNode* Node, const FMjCompilerSettings& CompilerSettings)
{
    if (!Node) return;

    FString PosStr = Node->GetAttribute(TEXT("pos"));
    if (!PosStr.IsEmpty())
    {
        FVector MjPos = MjXmlUtils::ParseVector(PosStr);
        SetRelativeLocation(MjUtils::MjToUEPosition(&MjPos.X));
    }
    
    double MjQuat[4];
    if (MjOrientationUtils::OrientationToMjQuat(Node, CompilerSettings, MjQuat))
    {
        SetRelativeRotation(MjUtils::MjToUERotation(MjQuat));
    }

    MjXmlUtils::ReadAttrFloat(Node, TEXT("gravcomp"), Gravcomp, bOverride_Gravcomp);

    if (MjXmlUtils::ReadAttrString(Node, TEXT("childclass"), ChildClassName))
        bOverride_ChildClassName = true;

    // mocap="true" 将此身体标记为由外部模拟驱动的运动学参考。
    bool bMocap = false;
    bool bDummyMocap = false;
    if (MjXmlUtils::ReadAttrBool(Node, TEXT("mocap"), bMocap, bDummyMocap) && bMocap)
        bDrivenByUnreal = true;

    // 睡眠策略 (MuJoCo 3.4+): "never" | "allowed" | "init"
    FString SleepAttr;
    if (MjXmlUtils::ReadAttrString(Node, TEXT("sleep"), SleepAttr))
    {
        SleepAttr = SleepAttr.ToLower();
        if      (SleepAttr == TEXT("never"))   SleepPolicy = EMjBodySleepPolicy::Never;
        else if (SleepAttr == TEXT("allowed")) SleepPolicy = EMjBodySleepPolicy::Allowed;
        else if (SleepAttr == TEXT("init"))    SleepPolicy = EMjBodySleepPolicy::InitAsleep;
    }

    // 名称属性 → 存储在 MjName 中以便显式覆盖
    MjXmlUtils::ReadAttrString(Node, TEXT("name"), MjName);
}

void UMjBody::Bind(mjModel* Model, mjData* Data, const FString& Prefix)
{
    Super::Bind(Model, Data, Prefix);

	if (Model && Data)
    {
        m_BodyView = BindToView<BodyView>(Prefix);

        if (m_BodyView.id != -1)
        {
            m_ID = m_BodyView.id;
            m_IsSetup = true;
            SetComponentTickEnabled(true);
        }
        else
        {
            UE_LOG(LogURLabBind, Warning, TEXT("MjBody::Bind() - FAILED to find body '%s'"), *GetName());
            m_IsSetup = false;
            SetComponentTickEnabled(false);
        }

        if (bDrivenByUnreal && m_ID >= 0)
        {
            int mocapid = Model->body_mocapid[m_ID];
            if (mocapid >= 0)
            {
                 m_MocapPos = Data->mocap_pos + 3 * mocapid;
                 m_MocapQuat = Data->mocap_quat + 4 * mocapid;
            }
        }
    }

	TArray<USceneComponent*> AllChildren;
	GetChildrenComponents(true, AllChildren);
	for (USceneComponent* Child : AllChildren)
	{
		if (UStaticMeshComponent* SMC = Cast<UStaticMeshComponent>(Child))
		{
			UStaticMesh* Mesh = SMC->GetStaticMesh();
			if (Mesh)
			{
				UBodySetup* BodySetup = Mesh->BodySetup;
				if (BodySetup)
				{
					FVector LocalCenter = BodySetup->AggGeom.CalcAABB(FTransform::Identity).GetCenter();
					m_MeshPivotOffset = LocalCenter;
					break;
				}
			}
		}
	}

    // 子组件绑定由 PostSetup 对所有组件进行扁平化迭代来处理。
    // 如果在这里也调用 Bind()，会导致每个子组件被绑定两次。
    // for (const auto& SpecElem : m_SpecElements)
    // {
    //     if (SpecElem)
    //     {
    //         SpecElem->Bind(Model, Data, Prefix);
    //     }
    // }
}

BodyView UMjBody::GetBodyView() const
{
	return m_BodyView;
}

FVector UMjBody::GetWorldPosition() const
{
    if (m_BodyView.id < 0 || !m_BodyView.xpos) return FVector::ZeroVector;
    return MjUtils::MjToUEPosition(m_BodyView.xpos);
}

FQuat UMjBody::GetWorldRotation() const
{
    if (m_BodyView.id < 0 || !m_BodyView.xquat) return FQuat::Identity;
    return MjUtils::MjToUERotation(m_BodyView.xquat);
}

FMuJoCoSpatialVelocity UMjBody::GetSpatialVelocity() const
{
    FMuJoCoSpatialVelocity Result;
    if (m_BodyView.id < 0 || !m_BodyView.cvel) return Result;

    // MuJoCo cvel: [ang_x, ang_y, ang_z, lin_x, lin_y, lin_z] (MuJoCo 帧, m/s 和 rad/s)
    // Unreal 帧: X -> X, Y -> -Y, Z -> Z
    
    // 线速度 (m/s -> cm/s)
    Result.Linear.X = (float)m_BodyView.cvel[3] * 100.0f;
    Result.Linear.Y = -(float)m_BodyView.cvel[4] * 100.0f;
    Result.Linear.Z = (float)m_BodyView.cvel[5] * 100.0f;

    // 角速度 (rad/s -> deg/s)
    Result.Angular.X = FMath::RadiansToDegrees((float)m_BodyView.cvel[0]);
    Result.Angular.Y = -FMath::RadiansToDegrees((float)m_BodyView.cvel[1]);
    Result.Angular.Z = FMath::RadiansToDegrees((float)m_BodyView.cvel[2]);

    return Result;
}

void UMjBody::ApplyForce(FVector Force, FVector Torque)
{
    if (m_BodyView.id < 0 || !m_BodyView.xfrc_applied) return;
    // xfrc_applied 布局: 在 Mujoco 帧中 [torque_x, torque_y, torque_z, force_x, force_y, force_z]
    // 转换 UE (厘米, Y 轴翻转) -> MuJoCo (米，右手系)
    const float InvScale = 0.01f; // cm -> m
    mjtNum* xfrc = m_BodyView.xfrc_applied;
    // 力矩：UE X -> Mj X, UE Y -> -Mj Y, UE Z -> Mj Z
    xfrc[0] = (mjtNum)(Torque.X);
    xfrc[1] = (mjtNum)(-Torque.Y);
    xfrc[2] = (mjtNum)(Torque.Z);
    // 力：相同的约定
    xfrc[3] = (mjtNum)(Force.X * InvScale);
    xfrc[4] = (mjtNum)(-Force.Y * InvScale);
    xfrc[5] = (mjtNum)(Force.Z * InvScale);
}

void UMjBody::ClearForce()
{
    if (m_BodyView.id < 0 || !m_BodyView.xfrc_applied) return;
    for (int i = 0; i < 6; ++i)
        m_BodyView.xfrc_applied[i] = 0.0;
}

bool UMjBody::IsAwake() const
{
    // body_awake: mjtSleepState — mjS_ASLEEP=0, mjS_AWAKE=1
    if (m_BodyView.id < 0 || !m_BodyView._d) return true;  // unbound → treat as awake
    // return true;
    return m_BodyView._d->body_awake[m_BodyView.id] != 0;
}

void UMjBody::Wake()
{
    if (m_BodyView.id < 0 || !m_BodyView._d || !m_BodyView._m) return;

    m_BodyView._d->body_awake[m_BodyView.id] = 1;  // mjS_AWAKE

    // 同时唤醒运动学树，以便物理步传播唤醒
    int32 TreeId = m_BodyView._m->body_treeid[m_BodyView.id];
    if (TreeId >= 0 && TreeId < m_BodyView._m->ntree)
    {
        m_BodyView._d->tree_asleep[TreeId] = -1;  // <0 → awake
        m_BodyView._d->tree_awake[TreeId]  = 1;
    }
}

void UMjBody::Sleep()
{
    if (m_BodyView.id < 0 || !m_BodyView._d || !m_BodyView._m) return;

    m_BodyView._d->body_awake[m_BodyView.id] = 0;  // mjS_ASLEEP

    // 同时将运动树标记为休眠状态。
    int32 TreeId = m_BodyView._m->body_treeid[m_BodyView.id];
    if (TreeId >= 0 && TreeId < m_BodyView._m->ntree)
    {
        // tree_asleep >= 0 表示该树处于睡眠状态（值为睡眠周期中的索引）。
         if (m_BodyView._d->tree_asleep[TreeId] < 0)
            m_BodyView._d->tree_asleep[TreeId] = 0;
        m_BodyView._d->tree_awake[TreeId] = 0;
    }
}

void UMjBody::RegisterToSpec(FMujocoSpecWrapper& Wrapper, mjsBody* ParentBody)
{
    // MjBody 通过父 MjBody 中的 Setup() 方法递归处理。
    // 此接口方法是为了满足 IMjSpecElement 规范而提供的，但在标准流程中并不使用。
    // 如果显式调用此方法，我们会发出警告并尝试委托给 Setup 方法，尽管这种做法并不常见。
    UE_LOG(LogURLab, Warning, TEXT("MjBody::RegisterToSpec called for %s. This path is liable to double-create bodies if not careful. Prefer Setup()."), *GetName());
    if (ParentBody && !m_IsSetup)
    {
         Setup(GetAttachParent(), ParentBody, &Wrapper);
    }
}

#if WITH_EDITOR
TArray<FString> UMjBody::GetChildClassOptions() const
{
    return UMjComponent::GetSiblingComponentOptions(this, UMjDefault::StaticClass(), true);
}
#endif
