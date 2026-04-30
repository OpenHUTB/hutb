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

#include "MuJoCo/Components/Sensors/MjSensor.h"
#include "MuJoCo/Utils/MjXmlUtils.h"
#include "MuJoCo/Utils/MjUtils.h"
#include "XmlNode.h"
#include "Utils/URLabLogging.h"
#include "MuJoCo/Components/Defaults/MjDefault.h"
#include "MuJoCo/Core/Spec/MjSpecWrapper.h"
#include "MuJoCo/Components/Bodies/MjBody.h"
#include "MuJoCo/Components/Joints/MjJoint.h"
#include "MuJoCo/Components/Geometry/MjGeom.h"
#include "MuJoCo/Components/Geometry/MjSite.h"
#include "MuJoCo/Components/Tendons/MjTendon.h"
#include "MuJoCo/Components/Actuators/MjActuator.h"

UMjSensor::UMjSensor()
{
	PrimaryComponentTick.bCanEverTick = false;

    Type = EMjSensorType::Accelerometer;
    ObjType = EMjObjType::Site;
    RefType = EMjObjType::Unknown;
    Dim = 3;
    Noise = 0.0f;
    Cutoff = 0.0f;
}

static EMjObjType MjObjToEnum(int obj) {
    switch(obj) {
        case mjOBJ_BODY: return EMjObjType::Body;
        case mjOBJ_XBODY: return EMjObjType::XBody;
        case mjOBJ_JOINT: return EMjObjType::Joint;
        case mjOBJ_DOF: return EMjObjType::DoF;
        case mjOBJ_GEOM: return EMjObjType::Geom;
        case mjOBJ_SITE: return EMjObjType::Site;
        case mjOBJ_CAMERA: return EMjObjType::Camera;
        case mjOBJ_LIGHT: return EMjObjType::Light;
        case mjOBJ_MESH: return EMjObjType::Mesh;
        case mjOBJ_HFIELD: return EMjObjType::HField;
        case mjOBJ_TEXTURE: return EMjObjType::Texture;
        case mjOBJ_MATERIAL: return EMjObjType::Material;
        case mjOBJ_PAIR: return EMjObjType::Pair;
        case mjOBJ_EXCLUDE: return EMjObjType::Exclude;
        case mjOBJ_EQUALITY: return EMjObjType::Equality;
        case mjOBJ_TENDON: return EMjObjType::Tendon;
        case mjOBJ_ACTUATOR: return EMjObjType::Actuator;
        case mjOBJ_SENSOR: return EMjObjType::Sensor;
        case mjOBJ_NUMERIC: return EMjObjType::Numeric;
        case mjOBJ_TEXT: return EMjObjType::Text;
        case mjOBJ_TUPLE: return EMjObjType::Tuple;
        case mjOBJ_KEY: return EMjObjType::Key;
        case mjOBJ_PLUGIN: return EMjObjType::Plugin;
        default: return EMjObjType::Unknown;
    }
}

static int EnumToMjObj(EMjObjType Type) {
    switch(Type) {
        case EMjObjType::Body:      return mjOBJ_BODY;
        case EMjObjType::XBody:     return mjOBJ_XBODY;
        case EMjObjType::Joint:     return mjOBJ_JOINT;
        case EMjObjType::DoF:       return mjOBJ_DOF;
        case EMjObjType::Geom:      return mjOBJ_GEOM;
        case EMjObjType::Site:      return mjOBJ_SITE;
        case EMjObjType::Camera:    return mjOBJ_CAMERA;
        case EMjObjType::Light:     return mjOBJ_LIGHT;
        case EMjObjType::Mesh:      return mjOBJ_MESH;
        case EMjObjType::HField:    return mjOBJ_HFIELD;
        case EMjObjType::Texture:   return mjOBJ_TEXTURE;
        case EMjObjType::Material:  return mjOBJ_MATERIAL;
        case EMjObjType::Pair:      return mjOBJ_PAIR;
        case EMjObjType::Exclude:   return mjOBJ_EXCLUDE;
        case EMjObjType::Equality:  return mjOBJ_EQUALITY;
        case EMjObjType::Tendon:    return mjOBJ_TENDON;
        case EMjObjType::Actuator:  return mjOBJ_ACTUATOR;
        case EMjObjType::Sensor:    return mjOBJ_SENSOR;
        case EMjObjType::Numeric:   return mjOBJ_NUMERIC;
        case EMjObjType::Text:      return mjOBJ_TEXT;
        case EMjObjType::Tuple:     return mjOBJ_TUPLE;
        case EMjObjType::Key:       return mjOBJ_KEY;
        case EMjObjType::Plugin:    return mjOBJ_PLUGIN;
        default:                    return mjOBJ_UNKNOWN;
    }
}


void UMjSensor::ExportTo(mjsSensor* Sensor, mjsDefault* Default)
{
    if (!Sensor) return;

    if (!TargetName.IsEmpty()) mjs_setString(Sensor->objname, TCHAR_TO_UTF8(*TargetName));
    if (!ReferenceName.IsEmpty()) mjs_setString(Sensor->refname, TCHAR_TO_UTF8(*ReferenceName));

    Sensor->dim = Dim;
    
    if (bOverride_Noise) Sensor->noise = Noise;
    if (bOverride_Cutoff) Sensor->cutoff = Cutoff;
    // UserAdr is read-only runtime data (mjModel::sensor_adr), not writable via mjsSensor.

    for(int i=0; i<IntParams.Num() && i<3; i++) Sensor->intprm[i] = IntParams[i];
    
    switch(Type)
    {
        case EMjSensorType::Touch:          Sensor->type = mjSENS_TOUCH;         Sensor->objtype = mjOBJ_SITE; break;
        case EMjSensorType::Accelerometer:  Sensor->type = mjSENS_ACCELEROMETER; Sensor->objtype = mjOBJ_SITE; break;
        case EMjSensorType::Velocimeter:    Sensor->type = mjSENS_VELOCIMETER;   Sensor->objtype = mjOBJ_SITE; break;
        case EMjSensorType::Gyro:           Sensor->type = mjSENS_GYRO;          Sensor->objtype = mjOBJ_SITE; break;
        case EMjSensorType::Force:          Sensor->type = mjSENS_FORCE;         Sensor->objtype = mjOBJ_SITE; break;
        case EMjSensorType::Torque:         Sensor->type = mjSENS_TORQUE;        Sensor->objtype = mjOBJ_SITE; break;
        case EMjSensorType::Magnetometer:   Sensor->type = mjSENS_MAGNETOMETER;  Sensor->objtype = mjOBJ_SITE; break;

        case EMjSensorType::CamProjection:
            Sensor->type = mjSENS_CAMPROJECTION;
            Sensor->objtype = mjOBJ_SITE;
            Sensor->reftype = mjOBJ_CAMERA;
            break;

        case EMjSensorType::RangeFinder:
            Sensor->type = mjSENS_RANGEFINDER;
            Sensor->objtype = (ObjType == EMjObjType::Camera) ? mjOBJ_CAMERA : mjOBJ_SITE;
            break;

        case EMjSensorType::JointPos:        Sensor->type = mjSENS_JOINTPOS;      Sensor->objtype = mjOBJ_JOINT; break;
        case EMjSensorType::JointVel:        Sensor->type = mjSENS_JOINTVEL;      Sensor->objtype = mjOBJ_JOINT; break;
        case EMjSensorType::JointActFrc:     Sensor->type = mjSENS_JOINTACTFRC;   Sensor->objtype = mjOBJ_JOINT; break;
        case EMjSensorType::BallQuat:        Sensor->type = mjSENS_BALLQUAT;      Sensor->objtype = mjOBJ_JOINT; break;
        case EMjSensorType::BallAngVel:      Sensor->type = mjSENS_BALLANGVEL;    Sensor->objtype = mjOBJ_JOINT; break;
        case EMjSensorType::JointLimitPos:   Sensor->type = mjSENS_JOINTLIMITPOS; Sensor->objtype = mjOBJ_JOINT; break;
        case EMjSensorType::JointLimitVel:   Sensor->type = mjSENS_JOINTLIMITVEL; Sensor->objtype = mjOBJ_JOINT; break;
        case EMjSensorType::JointLimitFrc:   Sensor->type = mjSENS_JOINTLIMITFRC; Sensor->objtype = mjOBJ_JOINT; break;

        case EMjSensorType::TendonPos:       Sensor->type = mjSENS_TENDONPOS;      Sensor->objtype = mjOBJ_TENDON; break;
        case EMjSensorType::TendonVel:       Sensor->type = mjSENS_TENDONVEL;      Sensor->objtype = mjOBJ_TENDON; break;
        case EMjSensorType::TendonActFrc:    Sensor->type = mjSENS_TENDONACTFRC;   Sensor->objtype = mjOBJ_TENDON; break;
        case EMjSensorType::TendonLimitPos:  Sensor->type = mjSENS_TENDONLIMITPOS; Sensor->objtype = mjOBJ_TENDON; break;
        case EMjSensorType::TendonLimitVel:  Sensor->type = mjSENS_TENDONLIMITVEL; Sensor->objtype = mjOBJ_TENDON; break;
        case EMjSensorType::TendonLimitFrc:  Sensor->type = mjSENS_TENDONLIMITFRC; Sensor->objtype = mjOBJ_TENDON; break;

        case EMjSensorType::ActuatorPos:     Sensor->type = mjSENS_ACTUATORPOS; Sensor->objtype = mjOBJ_ACTUATOR; break;
        case EMjSensorType::ActuatorVel:     Sensor->type = mjSENS_ACTUATORVEL; Sensor->objtype = mjOBJ_ACTUATOR; break;
        case EMjSensorType::ActuatorFrc:     Sensor->type = mjSENS_ACTUATORFRC; Sensor->objtype = mjOBJ_ACTUATOR; break;

        case EMjSensorType::FramePos:       Sensor->type = mjSENS_FRAMEPOS;    break;
        case EMjSensorType::FrameQuat:      Sensor->type = mjSENS_FRAMEQUAT;   break;
        case EMjSensorType::FrameXAxis:     Sensor->type = mjSENS_FRAMEXAXIS;  break;
        case EMjSensorType::FrameYAxis:     Sensor->type = mjSENS_FRAMEYAXIS;  break;
        case EMjSensorType::FrameZAxis:     Sensor->type = mjSENS_FRAMEZAXIS;  break;
        case EMjSensorType::FrameLinVel:    Sensor->type = mjSENS_FRAMELINVEL; break;
        case EMjSensorType::FrameAngVel:    Sensor->type = mjSENS_FRAMEANGVEL; break;
        case EMjSensorType::FrameLinAcc:    Sensor->type = mjSENS_FRAMELINACC; break;
        case EMjSensorType::FrameAngAcc:    Sensor->type = mjSENS_FRAMEANGACC; break;

        case EMjSensorType::InsideSite:     Sensor->type = mjSENS_INSIDESITE;  Sensor->objtype = (mjtObj)EnumToMjObj(ObjType); Sensor->reftype = mjOBJ_SITE; break;
        case EMjSensorType::SubtreeCom:     Sensor->type = mjSENS_SUBTREECOM;  Sensor->objtype = mjOBJ_BODY; break;
        case EMjSensorType::SubtreeLinVel:  Sensor->type = mjSENS_SUBTREELINVEL; Sensor->objtype = mjOBJ_BODY; break;
        case EMjSensorType::SubtreeAngMom:  Sensor->type = mjSENS_SUBTREEANGMOM; Sensor->objtype = mjOBJ_BODY; break;

        case EMjSensorType::GeomDist:       Sensor->type = mjSENS_GEOMDIST;   break;
        case EMjSensorType::GeomNormal:     Sensor->type = mjSENS_GEOMNORMAL; break;
        case EMjSensorType::GeomFromTo:     Sensor->type = mjSENS_GEOMFROMTO; break;

        case EMjSensorType::Contact:        Sensor->type = mjSENS_CONTACT; break;
        case EMjSensorType::EPotential:     Sensor->type = mjSENS_E_POTENTIAL; break;
        case EMjSensorType::EKinetic:      Sensor->type = mjSENS_E_KINETIC;   break;
        case EMjSensorType::Clock:         Sensor->type = mjSENS_CLOCK;       break;

        case EMjSensorType::Tactile:        Sensor->type = mjSENS_TACTILE; Sensor->objtype = mjOBJ_MESH; Sensor->reftype = mjOBJ_GEOM; break;
        case EMjSensorType::Plugin:         Sensor->type = mjSENS_PLUGIN; break;
        case EMjSensorType::User:           Sensor->type = mjSENS_USER;   break;

        default:                            Sensor->type = mjSENS_ACCELEROMETER; Sensor->objtype = mjOBJ_SITE; break;
    }

    if (Sensor->type >= mjSENS_FRAMEPOS && Sensor->type <= mjSENS_FRAMEANGACC)
    {
        Sensor->objtype = (mjtObj)EnumToMjObj(ObjType);
        if (RefType != EMjObjType::Unknown) Sensor->reftype = (mjtObj)EnumToMjObj(RefType);
    }
    else if (Type == EMjSensorType::GeomDist || Type == EMjSensorType::GeomNormal || Type == EMjSensorType::GeomFromTo || Type == EMjSensorType::Contact || Type == EMjSensorType::Plugin)
    {
        Sensor->objtype = (mjtObj)EnumToMjObj(ObjType);
        Sensor->reftype = (mjtObj)EnumToMjObj(RefType);
    }
    else if (Type == EMjSensorType::User)
    {
        Sensor->objtype = (mjtObj)EnumToMjObj(ObjType);
    }
}

void UMjSensor::ImportFromXml(const FXmlNode* Node)
{
    if (!Node) return;

    // Determine sensor type from the XML tag name
    static const TMap<FString, EMjSensorType> TagToType = {
        {TEXT("touch"),           EMjSensorType::Touch},
        {TEXT("accelerometer"),   EMjSensorType::Accelerometer},
        {TEXT("velocimeter"),     EMjSensorType::Velocimeter},
        {TEXT("gyro"),            EMjSensorType::Gyro},
        {TEXT("force"),           EMjSensorType::Force},
        {TEXT("torque"),          EMjSensorType::Torque},
        {TEXT("magnetometer"),    EMjSensorType::Magnetometer},
        {TEXT("rangefinder"),     EMjSensorType::RangeFinder},
        {TEXT("camprojection"),   EMjSensorType::CamProjection},
        {TEXT("jointpos"),        EMjSensorType::JointPos},
        {TEXT("jointvel"),        EMjSensorType::JointVel},
        {TEXT("jointactfrc"),     EMjSensorType::JointActFrc},
        {TEXT("tendonpos"),       EMjSensorType::TendonPos},
        {TEXT("tendonvel"),       EMjSensorType::TendonVel},
        {TEXT("tendonactfrc"),    EMjSensorType::TendonActFrc},
        {TEXT("actuatorpos"),     EMjSensorType::ActuatorPos},
        {TEXT("actuatorvel"),     EMjSensorType::ActuatorVel},
        {TEXT("actuatorfrc"),     EMjSensorType::ActuatorFrc},
        {TEXT("ballquat"),        EMjSensorType::BallQuat},
        {TEXT("ballangvel"),      EMjSensorType::BallAngVel},
        {TEXT("jointlimitpos"),   EMjSensorType::JointLimitPos},
        {TEXT("jointlimitvel"),   EMjSensorType::JointLimitVel},
        {TEXT("jointlimitfrc"),   EMjSensorType::JointLimitFrc},
        {TEXT("tendonlimitpos"),  EMjSensorType::TendonLimitPos},
        {TEXT("tendonlimitvel"),  EMjSensorType::TendonLimitVel},
        {TEXT("tendonlimitfrc"),  EMjSensorType::TendonLimitFrc},
        {TEXT("framepos"),        EMjSensorType::FramePos},
        {TEXT("framequat"),       EMjSensorType::FrameQuat},
        {TEXT("framexaxis"),      EMjSensorType::FrameXAxis},
        {TEXT("frameyaxis"),      EMjSensorType::FrameYAxis},
        {TEXT("framezaxis"),      EMjSensorType::FrameZAxis},
        {TEXT("framelinvel"),     EMjSensorType::FrameLinVel},
        {TEXT("frameangvel"),     EMjSensorType::FrameAngVel},
        {TEXT("framelinacc"),     EMjSensorType::FrameLinAcc},
        {TEXT("frameangacc"),     EMjSensorType::FrameAngAcc},
        {TEXT("subtreecom"),      EMjSensorType::SubtreeCom},
        {TEXT("subtreelinvel"),   EMjSensorType::SubtreeLinVel},
        {TEXT("subtreeangmom"),   EMjSensorType::SubtreeAngMom},
        {TEXT("insidesite"),      EMjSensorType::InsideSite},
        {TEXT("geomdist"),        EMjSensorType::GeomDist},
        {TEXT("geomnormal"),      EMjSensorType::GeomNormal},
        {TEXT("geomfromto"),      EMjSensorType::GeomFromTo},
        {TEXT("contact"),         EMjSensorType::Contact},
        {TEXT("e_potential"),     EMjSensorType::EPotential},
        {TEXT("e_kinetic"),       EMjSensorType::EKinetic},
        {TEXT("clock"),           EMjSensorType::Clock},
        {TEXT("tactile"),         EMjSensorType::Tactile},
        {TEXT("plugin"),          EMjSensorType::Plugin},
        {TEXT("user"),            EMjSensorType::User},
    };
    const FString Tag = Node->GetTag().ToLower();
    const EMjSensorType* Found = TagToType.Find(Tag);
    if (Found) Type = *Found;

    MjXmlUtils::ReadAttrFloat(Node, TEXT("noise"), Noise, bOverride_Noise);
    MjXmlUtils::ReadAttrFloat(Node, TEXT("cutoff"), Cutoff, bOverride_Cutoff);

    // Object name: try each attribute in priority order
    TargetName = Node->GetAttribute(TEXT("site"));
    if (TargetName.IsEmpty()) TargetName = Node->GetAttribute(TEXT("joint"));
    if (TargetName.IsEmpty()) TargetName = Node->GetAttribute(TEXT("tendon"));
    if (TargetName.IsEmpty()) TargetName = Node->GetAttribute(TEXT("actuator"));
    if (TargetName.IsEmpty()) TargetName = Node->GetAttribute(TEXT("body"));
    if (TargetName.IsEmpty()) TargetName = Node->GetAttribute(TEXT("geom"));
    if (TargetName.IsEmpty()) TargetName = Node->GetAttribute(TEXT("objname"));

    // Reference name (for frame sensors, camprojection, etc.)
    ReferenceName = Node->GetAttribute(TEXT("refname"));

    // objtype / reftype — string attributes specifying object/reference type for frame,
    // geomfromto, contact, user, and camprojection sensors.
    // Map from MJCF string → EMjObjType so ExportTo() can write the correct mjtObj enum.
    static const TMap<FString, EMjObjType> ObjTypeStrMap = {
        {TEXT("body"),      EMjObjType::Body},
        {TEXT("xbody"),     EMjObjType::XBody},
        {TEXT("joint"),     EMjObjType::Joint},
        {TEXT("dof"),       EMjObjType::DoF},
        {TEXT("geom"),      EMjObjType::Geom},
        {TEXT("site"),      EMjObjType::Site},
        {TEXT("camera"),    EMjObjType::Camera},
        {TEXT("light"),     EMjObjType::Light},
        {TEXT("mesh"),      EMjObjType::Mesh},
        {TEXT("hfield"),    EMjObjType::HField},
        {TEXT("texture"),   EMjObjType::Texture},
        {TEXT("material"),  EMjObjType::Material},
        {TEXT("pair"),      EMjObjType::Pair},
        {TEXT("exclude"),   EMjObjType::Exclude},
        {TEXT("equality"),  EMjObjType::Equality},
        {TEXT("tendon"),    EMjObjType::Tendon},
        {TEXT("actuator"),  EMjObjType::Actuator},
    };

    FString ObjTypeStr = Node->GetAttribute(TEXT("objtype")).ToLower();
    if (!ObjTypeStr.IsEmpty())
    {
        const EMjObjType* FoundObjType = ObjTypeStrMap.Find(ObjTypeStr);
        if (FoundObjType) ObjType = *FoundObjType;
    }

    FString RefTypeStr = Node->GetAttribute(TEXT("reftype")).ToLower();
    if (!RefTypeStr.IsEmpty())
    {
        const EMjObjType* FoundRefType = ObjTypeStrMap.Find(RefTypeStr);
        if (FoundRefType) RefType = *FoundRefType;
    }

    // dim override — used by user sensors where the dimension is not fixed by type
    FString DimStr = Node->GetAttribute(TEXT("dim"));
    if (!DimStr.IsEmpty())
    {
        int32 ParsedDim = FCString::Atoi(*DimStr);
        if (ParsedDim > 0) Dim = ParsedDim;
    }

    // adr — output address override for user sensors (written to mjsSensor::adr)
    {
        bool bAdrOverride = false;
        MjXmlUtils::ReadAttrInt(Node, TEXT("adr"), UserAdr, bAdrOverride);
    }

    MjXmlUtils::ReadAttrString(Node, TEXT("class"), MjClassName);
}

void UMjSensor::Bind(mjModel* Model, mjData* Data, const FString& Prefix)
{
    Super::Bind(Model, Data, Prefix);
    m_SensorView = BindToView<SensorView>(Prefix);
    
    if (m_SensorView.id != -1)
    {
        m_ID = m_SensorView.id;
    }
    else
    {
        UE_LOG(LogURLabBind, Warning, TEXT("[MjSensor] Sensor '%s' could not bind! Prefix: %s"), *GetName(), *Prefix);
    }
}

// Apply the MuJoCo → UE coordinate transform appropriate for each sensor type.
// Rules:
//   Position outputs (meters):    scale ×100, negate Y
//   Direction vector outputs:     negate Y only
//   3-D vector quantities (vel/acc/force/torque/angular): negate Y only
//   Quaternion outputs (w,x,y,z): reorder to UE (x,y,z,w) with handedness fix
//   Scalar outputs:               no transform
static void TransformSensorReading(TArray<float>& R, EMjSensorType Type)
{
    if (R.Num() == 0) return;

    switch (Type)
    {
    // --- Position outputs (scale ×100 cm, negate Y) ---
    case EMjSensorType::FramePos:
    case EMjSensorType::SubtreeCom:
        if (R.Num() >= 3)
        {
            R[0] *=  100.0f;
            R[1] *= -100.0f;
            R[2] *=  100.0f;
        }
        break;

    // --- Direction vectors (negate Y only) ---
    case EMjSensorType::FrameXAxis:
    case EMjSensorType::FrameYAxis:
    case EMjSensorType::FrameZAxis:
    case EMjSensorType::GeomNormal:
        if (R.Num() >= 3) R[1] = -R[1];
        break;

    // --- 3-D vector quantities: velocity, acceleration, force, torque, angular (negate Y only) ---
    case EMjSensorType::Accelerometer:
    case EMjSensorType::Velocimeter:
    case EMjSensorType::Gyro:
    case EMjSensorType::Force:
    case EMjSensorType::Torque:
    case EMjSensorType::Magnetometer:
    case EMjSensorType::BallAngVel:
    case EMjSensorType::FrameLinVel:
    case EMjSensorType::FrameAngVel:
    case EMjSensorType::FrameLinAcc:
    case EMjSensorType::FrameAngAcc:
    case EMjSensorType::SubtreeLinVel:
    case EMjSensorType::SubtreeAngMom:
        if (R.Num() >= 3) R[1] = -R[1];
        break;

    // --- Quaternion (w,x,y,z) → UE (X,Y,Z,W) with handedness fix: X=-mjX, Y=mjY, Z=-mjZ, W=mjW ---
    case EMjSensorType::BallQuat:
    case EMjSensorType::FrameQuat:
        if (R.Num() >= 4)
        {
            const float mj_w = R[0], mj_x = R[1], mj_y = R[2], mj_z = R[3];
            R[0] = -mj_x;
            R[1] =  mj_y;
            R[2] = -mj_z;
            R[3] =  mj_w;
        }
        break;

    // --- GeomFromTo: two 3D positions concatenated (scale ×100, negate Y each) ---
    case EMjSensorType::GeomFromTo:
        if (R.Num() >= 6)
        {
            R[0] *= 100.0f; R[1] *= -100.0f; R[2] *= 100.0f;
            R[3] *= 100.0f; R[4] *= -100.0f; R[5] *= 100.0f;
        }
        break;

    // --- Scalars and types with no coordinate meaning: no transform ---
    default:
        break;
    }
}

TArray<float> UMjSensor::GetReading() const
{
    TArray<float> Result;
    if (m_SensorView.id != -1 && m_SensorView.data)
    {
        for (int i = 0; i < m_SensorView.dim; ++i)
            Result.Add((float)m_SensorView.data[i]);
        TransformSensorReading(Result, Type);
    }
    return Result;
}

float UMjSensor::GetScalarReading() const
{
    if (m_SensorView.id != -1 && m_SensorView.data && m_SensorView.dim > 0)
    {
        return (float)m_SensorView.data[0];
    }
    return 0.0f;
}

int UMjSensor::GetDimension() const
{
    if (m_SensorView.id != -1)
        return m_SensorView.dim;
    return 0;
}

FString UMjSensor::GetMjName() const
{
    if (m_SensorView.id < 0 || !m_SensorView.name) return FString();
    return MjUtils::MjToString(m_SensorView.name);
}

void UMjSensor::RegisterToSpec(FMujocoSpecWrapper& Wrapper, mjsBody* ParentBody)
{
    mjsDefault* effectiveDefault = ResolveDefault(Wrapper.Spec, MjClassName);

    mjsSensor* sensor = mjs_addSensor(Wrapper.Spec);
    m_SpecElement = sensor->element;
    SetSpecElementName(Wrapper, sensor->element, mjOBJ_SENSOR);

    ExportTo(sensor, effectiveDefault);
}

void UMjSensor::BuildBinaryPayload(FBufferArchive& OutBuffer) const
{
    int32 SensorID = m_ID;
    OutBuffer << SensorID;
    
    int32 NumElements = GetDimension();
    OutBuffer << NumElements;

    if (m_SensorView.id != -1 && m_SensorView.data && NumElements > 0)
    {
        for (int i = 0; i < NumElements; ++i)
        {
            float Val = (float)m_SensorView.data[i];
            OutBuffer << Val;
        }
    }
}

FString UMjSensor::GetTelemetryTopicName() const
{
    return FString::Printf(TEXT("sensor/%s"), *GetName());
}

#if WITH_EDITOR
namespace
{
    UClass* GetClassForObjType(EMjObjType ObjType)
    {
        switch (ObjType)
        {
            case EMjObjType::Body:    return UMjBody::StaticClass();
            case EMjObjType::Joint:   return UMjJoint::StaticClass();
            case EMjObjType::Geom:    return UMjGeom::StaticClass();
            case EMjObjType::Site:    return UMjSite::StaticClass();
            case EMjObjType::Tendon:  return UMjTendon::StaticClass();
            case EMjObjType::Actuator:return UMjActuator::StaticClass();
            case EMjObjType::Sensor:  return UMjSensor::StaticClass();
            default:                  return UMjComponent::StaticClass();
        }
    }
}

TArray<FString> UMjSensor::GetTargetNameOptions() const
{
    return UMjComponent::GetSiblingComponentOptions(this, GetClassForObjType(ObjType));
}

TArray<FString> UMjSensor::GetReferenceNameOptions() const
{
    if (RefType == EMjObjType::Unknown) return {TEXT("")};
    return UMjComponent::GetSiblingComponentOptions(this, GetClassForObjType(RefType));
}

TArray<FString> UMjSensor::GetDefaultClassOptions() const
{
    return UMjComponent::GetSiblingComponentOptions(this, UMjDefault::StaticClass(), true);
}
#endif
