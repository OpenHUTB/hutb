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

#include "Utils/MJHelper.h"
FQuat MJHelper::MJQuatToUE(mjtNum* MjQuat) {

    FQuat quat;
    quat.W = MjQuat[0];
    quat.X = -MjQuat[1];
    quat.Y = MjQuat[2];
    quat.Z = -MjQuat[3];
    return quat;
}
FQuat MJHelper::MJQuatToUE(mjtNum* MjQuat, int Offset) {

    FQuat quat;
    quat.W = MjQuat[Offset + 0];
    quat.X = -MjQuat[Offset + 1];
    quat.Y = MjQuat[Offset + 2];
    quat.Z = -MjQuat[Offset + 3];
    return quat;
}

//I now need the opposite versions
FQuat MJHelper::UEQuatToMJ(FQuat UEQuat) {
    // Inverse of MJQuatToUE: MJ(w,x,y,z) stored in FQuat(X,Y,Z,W).
    // MJQuatToUE maps: W=mj[0], X=-mj[1], Y=mj[2], Z=-mj[3]
    // So inverse: mj[0]=ue.W, mj[1]=-ue.X, mj[2]=ue.Y, mj[3]=-ue.Z
    // FQuat constructor is (X,Y,Z,W), so pass as:
    return FQuat(-UEQuat.X, UEQuat.Y, -UEQuat.Z, UEQuat.W);
}

