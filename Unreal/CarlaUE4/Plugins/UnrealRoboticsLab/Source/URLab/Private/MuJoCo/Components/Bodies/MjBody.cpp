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

            // Thread safety: xpos/xquat reads are lock-free. On x86-64, aligned double
            // reads are atomic (no tearing). Worst case is a 1-frame position/rotation
            // desync which is visually imperceptible. This matches MuJoCo Simulate's
            // own threading model.
    	    FVector MuJoCoWorldPos = MjUtils::MjToUEPosition(m_BodyView.xpos);
    	    FQuat MuJoCoWorldQuat = MjUtils::MjToUERotation(m_BodyView.xquat);
    	    
            FVector CorrectedPos = MuJoCoWorldPos;
            
            // Apply mesh pivot offset correction ONLY if QuickConverted (Unreal -> MuJoCo flow pivot issues)
            if (bIsQuickConverted)
            {
    	        // MuJoCo's xpos is at body center, but UE mesh may have off-center pivot
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

    // Sleep policy (MuJoCo 3.4+). Only written when non-default so the global option takes effect otherwise.
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

    // mocap="true" marks this body as a kinematic reference driven from outside simulation
    bool bMocap = false;
    bool bDummyMocap = false;
    if (MjXmlUtils::ReadAttrBool(Node, TEXT("mocap"), bMocap, bDummyMocap) && bMocap)
        bDrivenByUnreal = true;

    // sleep policy (MuJoCo 3.4+): "never" | "allowed" | "init"
    FString SleepAttr;
    if (MjXmlUtils::ReadAttrString(Node, TEXT("sleep"), SleepAttr))
    {
        SleepAttr = SleepAttr.ToLower();
        if      (SleepAttr == TEXT("never"))   SleepPolicy = EMjBodySleepPolicy::Never;
        else if (SleepAttr == TEXT("allowed")) SleepPolicy = EMjBodySleepPolicy::Allowed;
        else if (SleepAttr == TEXT("init"))    SleepPolicy = EMjBodySleepPolicy::InitAsleep;
    }

    // name attribute → store in MjName for explicit override
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
				UBodySetup* BodySetup = Mesh->GetBodySetup();
				if (BodySetup)
				{
					FVector LocalCenter = BodySetup->AggGeom.CalcAABB(FTransform::Identity).GetCenter();
					m_MeshPivotOffset = LocalCenter;
					break; 
				}
			}
		}
	}

    // Child binding is handled by PostSetup's flat iteration over all components.
    // Calling Bind() here as well caused each child to be bound twice.
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

    // MuJoCo cvel: [ang_x, ang_y, ang_z, lin_x, lin_y, lin_z] (MuJoCo Frame, m/s and rad/s)
    // Unreal Frame: X -> X, Y -> -Y, Z -> Z
    
    // Linear Velocity (m/s -> cm/s)
    Result.Linear.X = (float)m_BodyView.cvel[3] * 100.0f;
    Result.Linear.Y = -(float)m_BodyView.cvel[4] * 100.0f;
    Result.Linear.Z = (float)m_BodyView.cvel[5] * 100.0f;

    // Angular Velocity (rad/s -> deg/s)
    Result.Angular.X = FMath::RadiansToDegrees((float)m_BodyView.cvel[0]);
    Result.Angular.Y = -FMath::RadiansToDegrees((float)m_BodyView.cvel[1]);
    Result.Angular.Z = FMath::RadiansToDegrees((float)m_BodyView.cvel[2]);

    return Result;
}

void UMjBody::ApplyForce(FVector Force, FVector Torque)
{
    if (m_BodyView.id < 0 || !m_BodyView.xfrc_applied) return;
    // xfrc_applied layout: [torque_x, torque_y, torque_z, force_x, force_y, force_z] in MuJoCo frame
    // Convert UE (cm, Y-flip) -> MuJoCo (m, right-hand)
    const float InvScale = 0.01f; // cm -> m
    mjtNum* xfrc = m_BodyView.xfrc_applied;
    // Torque: UE X -> Mj X, UE Y -> -Mj Y, UE Z -> Mj Z
    xfrc[0] = (mjtNum)(Torque.X);
    xfrc[1] = (mjtNum)(-Torque.Y);
    xfrc[2] = (mjtNum)(Torque.Z);
    // Force: same convention
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
    return m_BodyView._d->body_awake[m_BodyView.id] != 0;
}

void UMjBody::Wake()
{
    if (m_BodyView.id < 0 || !m_BodyView._d || !m_BodyView._m) return;

    m_BodyView._d->body_awake[m_BodyView.id] = 1;  // mjS_AWAKE

    // Also wake the kinematic tree so the physics step propagates the wake.
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

    // Also mark the kinematic tree as sleeping.
    int32 TreeId = m_BodyView._m->body_treeid[m_BodyView.id];
    if (TreeId >= 0 && TreeId < m_BodyView._m->ntree)
    {
        // tree_asleep >= 0 means the tree is sleeping (value is an index in the sleep cycle).
        if (m_BodyView._d->tree_asleep[TreeId] < 0)
            m_BodyView._d->tree_asleep[TreeId] = 0;
        m_BodyView._d->tree_awake[TreeId] = 0;
    }
}

void UMjBody::RegisterToSpec(FMujocoSpecWrapper& Wrapper, mjsBody* ParentBody)
{
    // MjBody is handled recursively via Setup() in the parent MjBody.
    // This interface method is provided to satisfy IMjSpecElement but is not used in the standard flow.
    // If called explicitly, we warn and attempt to delegate to Setup, though this path is unusual.
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
