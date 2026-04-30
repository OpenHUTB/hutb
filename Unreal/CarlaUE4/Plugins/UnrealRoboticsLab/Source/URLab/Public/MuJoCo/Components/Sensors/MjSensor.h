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

#pragma once

#include <mujoco/mjspec.h>

#include "CoreMinimal.h"
#include "mujoco/mujoco.h"
#include "MuJoCo/Core/Spec/MjSpecElement.h"
#include "MuJoCo/Components/MjComponent.h"
#include "MuJoCo/Components/Defaults/MjDefault.h"
#include "MuJoCo/Utils/MjBind.h"
#include "MjSensor.generated.h"

/**
 * @enum EMjSensorType
 * @brief Defines the type of sensor.
 */
UENUM(BlueprintType)
enum class EMjSensorType : uint8
{
    // Common robotic sensors (attached to site)
    Touch,
    Accelerometer,
    Velocimeter,
    Gyro,
    Force,
    Torque,
    Magnetometer,
    RangeFinder,
    CamProjection,

    // Scalar joints, tendons, actuators
    JointPos,
    JointVel,
    TendonPos,
    TendonVel,
    ActuatorPos,
    ActuatorVel,
    ActuatorFrc,
    JointActFrc,
    TendonActFrc,

    // Ball joints
    BallQuat,
    BallAngVel,

    // Joint and Tendon limits
    JointLimitPos,
    JointLimitVel,
    JointLimitFrc,
    TendonLimitPos,
    TendonLimitVel,
    TendonLimitFrc,

    // Frame sensors (attached to body, geom, site, camera)
    FramePos,
    FrameQuat,
    FrameXAxis,
    FrameYAxis,
    FrameZAxis,
    FrameLinVel,
    FrameAngVel,
    FrameLinAcc,
    FrameAngAcc,

    // Subtree sensors (attached to body)
    SubtreeCom,
    SubtreeLinVel,
    SubtreeAngMom,

    // Geometric relationships
    InsideSite,
    GeomDist,
    GeomNormal,
    GeomFromTo,

    // Contact
    Contact,

    // Global
    EPotential,
    EKinetic,
    Clock,

    // SDF
    Tactile,

    // Plugin/User
    Plugin,
    User
};

/**
 * @enum EMjObjType
 * @brief Mirror of mjtObj for specifying sensor attachment/reference types.
 */
UENUM(BlueprintType)
enum class EMjObjType : uint8
{
    Unknown,
    Body,
    XBody,
    Joint,
    DoF,
    Geom,
    Site,
    Camera,
    Light,
    Mesh,
    HField,
    Texture,
    Material,
    Pair,
    Exclude,
    Equality,
    Tendon,
    Actuator,
    Sensor,
    Numeric,
    Text,
    Tuple,
    Key,
    Plugin,
};

/**
 * @class UMjSensor
 * @brief Component representing a sensor in the MuJoCo model.
 * 
 * Sensors provide observation data from the simulation.
 * This component mirrors the `sensor` element in MuJoCo XML.
 */
UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class URLAB_API UMjSensor : public UMjComponent
{
	GENERATED_BODY()

public:	
	UMjSensor();

    /** @brief The type of sensor (Touch, Accelerometer, JointPos, etc.). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MuJoCo|Sensor")
    EMjSensorType Type;

    /** @brief Name of the object this sensor is attached to or referencing (e.g. site name, joint name). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MuJoCo|Sensor", meta=(GetOptions="GetTargetNameOptions"))
    FString TargetName;

    /** @brief Optional: Referenced object name (e.g. for reftype/refname pairs in MuJoCo). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MuJoCo|Sensor", meta=(GetOptions="GetReferenceNameOptions"))
    FString ReferenceName;

    /** @brief Dimension of the sensor output. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MuJoCo|Sensor")
    int Dim;

    /** @brief Override toggle for Noise. */
    UPROPERTY(EditAnywhere, Category = "MuJoCo|Sensor", meta=(InlineEditConditionToggle))
    bool bOverride_Noise = false;

    /** @brief Noise standard deviation added to the sensor reading. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MuJoCo|Sensor", meta=(EditCondition="bOverride_Noise"))
    float Noise = 0.0f;

    /** @brief Override toggle for Cutoff. */
    UPROPERTY(EditAnywhere, Category = "MuJoCo|Sensor", meta=(InlineEditConditionToggle))
    bool bOverride_Cutoff = false;

    /** @brief Cutoff frequency for the sensor filter. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MuJoCo|Sensor", meta=(EditCondition="bOverride_Cutoff"))
    float Cutoff = 0.0f;

    /** @brief Output address override for user sensors (mjsSensor::adr). -1 = let MuJoCo assign. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MuJoCo|Sensor")
    int32 UserAdr = -1;

    /** @brief Optional MuJoCo class name to inherit defaults from (string fallback). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MuJoCo|Sensor", meta=(GetOptions="GetDefaultClassOptions"))
    FString MjClassName;

    /** @brief Reference to a UMjDefault component for default class inheritance. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MuJoCo|Sensor")
    UMjDefault* DefaultClass = nullptr;

    virtual FString GetMjClassName() const override
    {
        return MjClassName;
    }

    /** @brief Type of object this sensor is attached to (e.g. Site, Joint, Body). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MuJoCo|Sensor")
    EMjObjType ObjType;

    /** @brief Type of reference object (e.g. Camera for camprojection, Body/Geom for distance). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MuJoCo|Sensor")
    EMjObjType RefType;

    /** @brief Integer parameters for sensor configuration (e.g. rangefinder flags, contact flags). Maps to sensor->intprm. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MuJoCo|Sensor")
    TArray<int32> IntParams;

    /** @brief User data for custom sensors. Maps to sensor->userdata. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MuJoCo|Sensor")
    TArray<double> UserParams;

public:

    void ExportTo(mjsSensor* Sensor, mjsDefault* Default = nullptr);
    
    /**
     * @brief Imports properties and override flags directly from the raw XML node.
     * @param Node Pointer to the corresponding FXmlNode.
     */
    void ImportFromXml(const class FXmlNode* Node);

    /**
     * @brief Registers this sensor to the MuJoCo spec.
     * @param Wrapper The spec wrapper instance.
     */
    virtual void RegisterToSpec(class FMujocoSpecWrapper& Wrapper, mjsBody* ParentBody = nullptr) override;

    // --- Runtime Binding ---
    virtual void Bind(mjModel* Model, mjData* Data, const FString& Prefix = TEXT("")) override;

    /** @brief Gets the full array reading. */
    UFUNCTION(BlueprintCallable, Category = "MuJoCo|Runtime")
    TArray<float> GetReading() const;

    virtual void BuildBinaryPayload(FBufferArchive& OutBuffer) const override;
    virtual FString GetTelemetryTopicName() const override;

    /** @brief Gets the first scalar reading (index 0). */
    UFUNCTION(BlueprintCallable, Category = "MuJoCo|Runtime")
    float GetScalarReading() const;

    /** @brief Gets the sensor output dimension (number of values in GetReading()). */
    UFUNCTION(BlueprintCallable, Category = "MuJoCo|Runtime")
    int GetDimension() const;

    /** @brief Gets the full prefixed name of this sensor as it appears in the compiled MuJoCo model. */
    virtual FString GetMjName() const override;

#if WITH_EDITOR
    UFUNCTION()
    TArray<FString> GetTargetNameOptions() const;
    UFUNCTION()
    TArray<FString> GetReferenceNameOptions() const;
    UFUNCTION()
    TArray<FString> GetDefaultClassOptions() const;
#endif

    /** @brief The runtime view of the MuJoCo sensor. */
    SensorView m_SensorView;

    /** @brief Semantic accessor for raw MuJoCo data and helper methods. */
    SensorView& GetMj() { return m_SensorView; }
    const SensorView& GetMj() const { return m_SensorView; }

};
