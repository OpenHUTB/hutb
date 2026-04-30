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

#include "MuJoCo/Components/Joints/MjFreeJoint.h"
#include "Serialization/BufferArchive.h"

UMjFreeJoint::UMjFreeJoint()
{
    bOverride_Type = true;
    Type = EMjJointType::Free;
}

void UMjFreeJoint::BuildBinaryPayload(FBufferArchive& OutBuffer) const
{
    // Free joint: qpos has 7 values (pos[3] + quat[4]), qvel has 6 values (linvel[3] + angvel[3])
    if (!m_JointView._d || !m_JointView.qpos || !m_JointView.qvel)
    {
        return;
    }

    // pos[3]
    for (int i = 0; i < 3; ++i)
    {
        float Val = (float)m_JointView.qpos[i];
        OutBuffer << Val;
    }
    // quat[4] — MuJoCo stores as (w,x,y,z), send as (x,y,z,w)
    float qx = (float)m_JointView.qpos[4];
    float qy = (float)m_JointView.qpos[5];
    float qz = (float)m_JointView.qpos[6];
    float qw = (float)m_JointView.qpos[3];
    OutBuffer << qx;
    OutBuffer << qy;
    OutBuffer << qz;
    OutBuffer << qw;
    // linvel[3]
    for (int i = 0; i < 3; ++i)
    {
        float Val = (float)m_JointView.qvel[i];
        OutBuffer << Val;
    }
    // angvel[3]
    for (int i = 3; i < 6; ++i)
    {
        float Val = (float)m_JointView.qvel[i];
        OutBuffer << Val;
    }
}

FString UMjFreeJoint::GetTelemetryTopicName() const
{
    return FString::Printf(TEXT("base_state/%s"), *GetName());
}
