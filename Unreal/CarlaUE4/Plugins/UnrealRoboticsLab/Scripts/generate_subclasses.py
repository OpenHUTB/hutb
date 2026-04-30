# Copyright (c) 2026 Jonathan Embley-Riches. All rights reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
# --- LEGAL DISCLAIMER ---
# UnrealRoboticsLab is an independent software plugin. It is NOT affiliated with, 
# endorsed by, or sponsored by Epic Games, Inc. "Unreal" and "Unreal Engine" are 
# trademarks or registered trademarks of Epic Games, Inc. in the US and elsewhere.
#
# This plugin incorporates third-party software: MuJoCo (Apache 2.0), 
# CoACD (MIT), and libzmq (MPL 2.0). See ThirdPartyNotices.txt for details.

import os

# Configuration
SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
PLUGIN_ROOT = os.path.abspath(os.path.join(SCRIPT_DIR, '..'))
SOURCE_ROOT = os.path.join(PLUGIN_ROOT, "Source/URLab")
PUBLIC_ROOT = os.path.join(SOURCE_ROOT, "Public/Mujoco/Components")
PRIVATE_ROOT = os.path.join(SOURCE_ROOT, "Private/Mujoco/Components")

# Mappings: ClassName -> (EnumValue, BaseClass)
# BaseClass: "Joint" -> UMjJoint, EMjJointType
#            "Actuator" -> UMjActuator, EMjActuatorType
#            "Sensor" -> UMjSensor, EMjSensorType

DATA = {
    "Joints": {
        "Base": "Joint",
        "Classes": {
            "MjHingeJoint": "Hinge",
            "MjSlideJoint": "Slide",
            "MjBallJoint": "Ball",
            "MjFreeJoint": "Free"
        }
    },
    "Actuators": {
        "Base": "Actuator",
        "Classes": {
            "MjMotorActuator": "Motor",
            "MjPositionActuator": "Position",
            "MjVelocityActuator": "Velocity",
            "MjMuscleActuator": "Muscle"
        }
    },
    "Sensors": {
        "Base": "Sensor",
        "Classes": {
            "MjTouchSensor": "Touch",
            "MjRangeFinderSensor": "RangeFinder",
            "MjJointPosSensor": "JointPos",
            "MjJointVelSensor": "JointVel",
            "MjTendonPosSensor": "TendonPos",
            "MjTendonVelSensor": "TendonVel",
            "MjActuatorPosSensor": "ActuatorPos",
            "MjActuatorVelSensor": "ActuatorVel",
            "MjActuatorFrcSensor": "ActuatorFrc",
            "MjJointActFrcSensor": "JointActFrc",
            "MjTendonActFrcSensor": "TendonActFrc",
            "MjJointLimitPosSensor": "JointLimitPos",
            "MjJointLimitVelSensor": "JointLimitVel",
            "MjJointLimitFrcSensor": "JointLimitFrc",
            "MjTendonLimitPosSensor": "TendonLimitPos",
            "MjTendonLimitVelSensor": "TendonLimitVel",
            "MjTendonLimitFrcSensor": "TendonLimitFrc",
            "MjGeomDistSensor": "GeomDist",
            "MjInsideSiteSensor": "InsideSite",
            "MjEPotentialSensor": "EPotential",
            "MjEKineticSensor": "EKinetic",
            "MjClockSensor": "Clock"
        }
    }
}

HEADER_TEMPLATE = """#pragma once

#include "CoreMinimal.h"
#include "Mujoco/Components/Mj{Base}.h"
#include "{ClassName}.generated.h"

/**
 * @class U{ClassName}
 * @brief Specific {Enum} {Base} component.
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class SYNTHY_API U{ClassName} : public UMj{Base}
{{
	GENERATED_BODY()
public:
	U{ClassName}();
}};
"""

CPP_TEMPLATE = """#include "Mujoco/Components/{Category}/{ClassName}.h"

U{ClassName}::U{ClassName}()
{{
    Type = EMj{Base}Type::{Enum};
}}
"""

def main():
    for category, info in DATA.items():
        base = info["Base"]
        classes = info["Classes"]
        
        pub_dir = os.path.join(PUBLIC_ROOT, category)
        priv_dir = os.path.join(PRIVATE_ROOT, category)
        
        os.makedirs(pub_dir, exist_ok=True)
        os.makedirs(priv_dir, exist_ok=True)
        
        for class_name, enum_val in classes.items():
            # Header
            h_path = os.path.join(pub_dir, f"{class_name}.h")
            h_content = HEADER_TEMPLATE.format(Base=base, ClassName=class_name, Enum=enum_val)
            
            with open(h_path, "w") as f:
                f.write(h_content)
                print(f"Created {h_path}")
            
            # Cpp
            custom_cpp = CPP_TEMPLATE.replace("{Category}", category) # To handle path insertion
            cpp_path = os.path.join(priv_dir, f"{class_name}.cpp")
            cpp_content = custom_cpp.format(Base=base, ClassName=class_name, Enum=enum_val)
            
            with open(cpp_path, "w") as f:
                f.write(cpp_content)
                print(f"Created {cpp_path}")

if __name__ == "__main__":
    main()
