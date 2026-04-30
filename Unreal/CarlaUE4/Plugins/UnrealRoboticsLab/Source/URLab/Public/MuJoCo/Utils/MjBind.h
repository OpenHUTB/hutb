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
 
#include <mujoco/mujoco.h>
#include <string>
#include "MuJoCo/Utils/MjUtils.h"
#include "Utils/URLabLogging.h"

// ==========================================
// 1. The View Structs
// ==========================================

// Helper to format 3D vectors
inline FString FormatVec3(const mjtNum* v) {
    if (!v) return TEXT("NULL");
    return FString::Printf(TEXT("[%.3f, %.3f, %.3f]"), v[0], v[1], v[2]);
}

// Helper to format quaternions
inline FString FormatQuat(const mjtNum* q) {
    if (!q) return TEXT("NULL");
    return FString::Printf(TEXT("[%.3f, %.3f, %.3f, %.3f]"), q[0], q[1], q[2], q[3]);
}

/**
 * @struct GeomView
 * @brief Lightweight wrapper around MuJoCo geom data (model and data).
 * provides easy access to configuration and state.
 */
struct GeomView {
    static constexpr mjtObj obj_type = mjOBJ_GEOM;

    const mjModel* _m;
    mjData* _d;
    int id;
    const char* name;

    // --- Config ---
    int type;
    mjtNum* size;
    mjtNum* pos_offset;
    mjtNum* quat_offset;
    float* rgba;
    int mat_id;
    int body_id;
    int dataid;
    mjtNum* friction;
    mjtNum* solref;
    mjtNum* solimp;
    mjtNum* solmix;
    mjtNum* margin;
    mjtNum* gap;
    int contype;
    int conaffinity;
    int priority;
    mjtNum* fluid_coef;
    mjtNum* user;

    // --- State ---
    mjtNum* xpos;
    mjtNum* xmat;


    GeomView() : _m(nullptr), _d(nullptr), id(-1) {}

    GeomView(const mjModel* m, mjData* d, int id_in) : _m(m), _d(d), id(id_in) {
        check(id >= 0 && id < m->ngeom);
        name = (m->name_geomadr[id] >= 0) ? m->names + m->name_geomadr[id] : nullptr;

        type        = m->geom_type[id];
        size        = m->geom_size + (id * 3);
        pos_offset  = m->geom_pos + (id * 3);
        quat_offset = m->geom_quat + (id * 4);
        rgba        = m->geom_rgba + (id * 4);
        mat_id      = m->geom_matid[id];
        body_id     = m->geom_bodyid[id];
        friction    = m->geom_friction + (id * 3);
        solref      = m->geom_solref + (id * mjNREF);
        solimp      = m->geom_solimp + (id * mjNIMP);
        solmix      = m->geom_solmix + id;
        margin      = m->geom_margin + id;
        gap         = m->geom_gap + id;
        contype     = m->geom_contype[id];
        conaffinity = m->geom_conaffinity[id];
        priority    = m->geom_priority[id];
        fluid_coef  = m->geom_fluid + (id * mjNFLUID);
        user        = m->geom_user + (id * m->nuser_geom);
        dataid     = m->geom_dataid[id];

        xpos = d->geom_xpos + (id * 3);
        xmat = d->geom_xmat + (id * 9);
    }

    /** @brief Sets the friction coefficient (tangential) for this geom. */
    void SetFriction(float Value) {
        if (friction) friction[0] = (mjtNum)Value;
    }

    /** @brief Sets the contact solver reference parameters (time constant, damping ratio). */
    void SetSolRef(float TimeConst, float DampRatio) {
        if (solref) {
            solref[0] = (mjtNum)TimeConst;
            solref[1] = (mjtNum)DampRatio;
        }
    }

    /** @brief Sets the contact solver impedance parameters. */
    void SetSolImp(float Dmin, float Dmax, float Width) {
        if (solimp) {
            solimp[0] = (mjtNum)Dmin;
            solimp[1] = (mjtNum)Dmax;
            solimp[2] = (mjtNum)Width;
        }
    }

    FString ToString() const {
        FString Info = FString::Printf(TEXT("=== Geom ID: %d (%s) ===\n"), id, name ? *MjUtils::MjToString(name) : TEXT("None"));
        Info += FString::Printf(TEXT("    Type: %d | Size: %s\n"), type, *FormatVec3(size));
        Info += FString::Printf(TEXT("    World Pos: %s\n"), *FormatVec3(xpos));
        return Info;
    }
};

/**
 * @struct JointView
 * @brief Lightweight wrapper around MuJoCo joint data.
 */
struct JointView {
    static constexpr mjtObj obj_type = mjOBJ_JOINT;

    const mjModel* _m;
    mjData* _d;
    int id;
    const char* name;
    int type;

    // --- Config ---
    mjtNum* pos_offset;
    mjtNum* axis_local;
    mjtNum* stiffness;
    mjtNum* stiffnesspoly;   // high-order stiffness coefficients (mjNPOLY values)
    mjtNum* range;
    mjtNum* margin;
    mjtNum* solref_limit;
    mjtNum* solimp_limit;
    mjtNum* damping;
    mjtNum* dampingpoly;     // high-order damping coefficients (mjNPOLY values)
    mjtNum* armature;
    mjtNum* frictionloss;
    mjtNum* solref_friction;
    mjtNum* solimp_friction;
    mjtNum* user;

    // --- State ---
    mjtNum* qpos;
    mjtNum* qvel;
    mjtNum* qacc;
    mjtNum* xanchor;
    mjtNum* xaxis;


    JointView() : _m(nullptr), _d(nullptr), id(-1) {}

    JointView(const mjModel* m, mjData* d, int id_in) : _m(m), _d(d), id(id_in) {
        check(id >= 0 && id < m->njnt);
        name = (m->name_jntadr[id] >= 0) ? m->names + m->name_jntadr[id] : nullptr;
        type = m->jnt_type[id];

        int qpos_adr = m->jnt_qposadr[id];
        int dof_adr  = m->jnt_dofadr[id];

        pos_offset  = m->jnt_pos + (id * 3);
        axis_local  = m->jnt_axis + (id * 3);
        stiffness       = m->jnt_stiffness + id;
        stiffnesspoly   = m->jnt_stiffnesspoly + (id * mjNPOLY);
        range           = m->jnt_range + (id * 2);
        margin      = m->jnt_margin + id;
        solref_limit= m->jnt_solref + (id * mjNREF);
        solimp_limit= m->jnt_solimp + (id * mjNIMP);
        user        = m->jnt_user + (id * m->nuser_jnt);

        damping         = m->dof_damping + dof_adr;
        dampingpoly     = m->dof_dampingpoly + (dof_adr * mjNPOLY);
        armature        = m->dof_armature + dof_adr;
        frictionloss    = m->dof_frictionloss + dof_adr;
        solref_friction = m->dof_solref + (dof_adr * mjNREF);
        solimp_friction = m->dof_solimp + (dof_adr * mjNIMP);

        qpos    = d->qpos + qpos_adr;
        qvel    = d->qvel + dof_adr;
        qacc    = d->qacc + dof_adr;
        xanchor = d->xanchor + (id * 3);
        xaxis   = d->xaxis + (id * 3);
    }

    /** @brief Gets the current joint position (radians for hinges, meters for sliders). */
    float GetPosition() const {
        return (qpos) ? (float)qpos[0] : 0.0f;
    }

    /** @brief Directly sets the joint position. Warning: teleports the physics state. */
    void SetPosition(float Value) {
        if (qpos) qpos[0] = (mjtNum)Value;
    }

    FString ToString() const {
        return FString::Printf(TEXT("=== Joint ID: %d (%s) Type: %d ===\n"), id, name ? *MjUtils::MjToString(name) : TEXT("None"), type);
    }
};

/**
 * @struct ActuatorView
 * @brief Lightweight wrapper around MuJoCo actuator data.
 */
struct ActuatorView {
    static constexpr mjtObj obj_type = mjOBJ_ACTUATOR;

    const mjModel* _m;
    mjData* _d;
    int id;
    const char* name;

    // --- Config (mjModel) ---
    int trntype;
    int dyntype;
    int gaintype;
    int biastype;
    mjtNum* gear;
    mjtNum* cranklength;
    mjtNum* acc0;
    mjtNum* length0;
    mjtNum* lengthrange;
    mjtNum* ctrlrange;
    mjtNum* forcerange;
    mjtNum* actrange;
    mjtNum* gainprm;
    mjtNum* biasprm;
    mjtNum* dynprm;
    
    // --- State (mjData) ---
    mjtNum* ctrl;
    mjtNum* force;
    mjtNum* length;
    mjtNum* moment; // output: actuator moment
    mjtNum* velocity;
    mjtNum* act;    // internal state (activation)

    ActuatorView() : _m(nullptr), _d(nullptr), id(-1) {}

    ActuatorView(const mjModel* m, mjData* d, int id_in) : _m(m), _d(d), id(id_in) {
        check(id >= 0 && id < m->nu);
        name = (m->name_actuatoradr[id] >= 0) ? m->names + m->name_actuatoradr[id] : nullptr;

        // Config
        trntype = m->actuator_trntype[id];
        dyntype = m->actuator_dyntype[id];
        gaintype = m->actuator_gaintype[id];
        biastype = m->actuator_biastype[id];
        
        gear = m->actuator_gear + (id * 6);
        cranklength = m->actuator_cranklength + id;
        acc0 = m->actuator_acc0 + id;
        length0 = m->actuator_length0 + id;
        lengthrange = m->actuator_lengthrange + (id * 2);
        ctrlrange = m->actuator_ctrlrange + (id * 2);
        forcerange = m->actuator_forcerange + (id * 2);
        actrange = m->actuator_actrange + (id * 2);
        
        gainprm = m->actuator_gainprm + (id * mjNGAIN);
        biasprm = m->actuator_biasprm + (id * mjNBIAS);
        dynprm = m->actuator_dynprm + (id * mjNDYN);

        // State
        ctrl = d->ctrl + id;
        force = d->actuator_force + id;
        length = d->actuator_length + id;
        moment = d->actuator_moment + (id * m->nv); // nactuator x nv matrix, row for this actuator
        velocity = d->actuator_velocity + id;
        int act_adr = m->actuator_actadr[id];
        if (act_adr >= 0)
            act = d->act + act_adr;
        else
            act = nullptr;
    }
};

/**
 * @struct TendonView
 * @brief Lightweight wrapper around MuJoCo tendon data.
 */
struct TendonView {
    static constexpr mjtObj obj_type = mjOBJ_TENDON;

    const mjModel* _m;
    mjData* _d;
    int id;
    const char* name;

    // --- Config (mjModel) ---
    mjtNum* stiffness;
    mjtNum* stiffnesspoly;   // high-order stiffness coefficients (mjNPOLY values)
    mjtNum* damping;
    mjtNum* dampingpoly;     // high-order damping coefficients (mjNPOLY values)
    mjtNum* frictionloss;
    mjtNum* armature;
    mjtNum* range;
    mjtNum* margin;
    mjtNum* solref_limit;
    mjtNum* solimp_limit;
    mjtNum* solref_friction;
    mjtNum* solimp_friction;

    // --- State (mjData) ---
    mjtNum* length;     // ten_length[id]
    mjtNum* velocity;   // ten_velocity[id]

    TendonView() : _m(nullptr), _d(nullptr), id(-1),
        name(nullptr), stiffness(nullptr), stiffnesspoly(nullptr),
        damping(nullptr), dampingpoly(nullptr),
        frictionloss(nullptr), armature(nullptr), range(nullptr),
        margin(nullptr), solref_limit(nullptr), solimp_limit(nullptr),
        solref_friction(nullptr), solimp_friction(nullptr),
        length(nullptr), velocity(nullptr) {}

    TendonView(const mjModel* m, mjData* d, int id_in) : _m(m), _d(d), id(id_in) {
        check(id >= 0 && id < m->ntendon);
        name = (m->name_tendonadr[id] >= 0) ? m->names + m->name_tendonadr[id] : nullptr;

        // Config
        stiffness       = m->tendon_stiffness + id;
        stiffnesspoly   = m->tendon_stiffnesspoly + (id * mjNPOLY);
        damping         = m->tendon_damping + id;
        dampingpoly     = m->tendon_dampingpoly + (id * mjNPOLY);
        frictionloss    = m->tendon_frictionloss + id;
        armature        = m->tendon_armature + id;
        range           = m->tendon_range + (id * 2);
        margin          = m->tendon_margin + id;
        solref_limit    = m->tendon_solref_lim + (id * mjNREF);
        solimp_limit    = m->tendon_solimp_lim + (id * mjNIMP);
        solref_friction = m->tendon_solref_fri + (id * mjNREF);
        solimp_friction = m->tendon_solimp_fri + (id * mjNIMP);

        // State
        length   = d->ten_length   + id;
        velocity = d->ten_velocity + id;
    }

    /** @brief Gets the current tendon length (meters). */
    float GetLength() const {
        return length ? (float)*length : 0.0f;
    }

    /** @brief Gets the current tendon velocity (m/s). */
    float GetVelocity() const {
        return velocity ? (float)*velocity : 0.0f;
    }
};

/**
 * @struct SensorView
 * @brief Lightweight wrapper around MuJoCo sensor data.
 */
struct SensorView {
    static constexpr mjtObj obj_type = mjOBJ_SENSOR;

    const mjModel* _m;
    mjData* _d;
    int id;
    const char* name;

    // --- Config ---
    int type;
    int objtype;
    int objid;
    int reftype;
    int refid;
    int dim;
    int adr;
    mjtNum* cutoff;
    mjtNum* noise;

    // --- State ---
    mjtNum* data; // Pointer to start of this sensor's data in sensordata

    SensorView() : _m(nullptr), _d(nullptr), id(-1) {}

    SensorView(const mjModel* m, mjData* d, int id_in) : _m(m), _d(d), id(id_in) {
        check(id >= 0 && id < m->nsensor);
        name = (m->name_sensoradr[id] >= 0) ? m->names + m->name_sensoradr[id] : nullptr;

        type = m->sensor_type[id];
        objtype = m->sensor_objtype[id];
        objid = m->sensor_objid[id];
        reftype = m->sensor_reftype[id];
        refid = m->sensor_refid[id];
        dim = m->sensor_dim[id];
        adr = m->sensor_adr[id];
        cutoff = m->sensor_cutoff + id;
        noise = m->sensor_noise + id;

        // State
        if (adr >= 0 && adr < m->nsensordata)
             data = d->sensordata + adr;
        else
             data = nullptr;
    }
};
/**
 * @struct SiteView
 * @brief Lightweight wrapper around MuJoCo site data.
 */
struct SiteView {
    static constexpr mjtObj obj_type = mjOBJ_SITE;

    const mjModel* _m;
    mjData* _d;
    int id;
    const char* name;

    // --- Config (mjModel) ---
    int type;
    mjtNum* size;
    mjtNum* pos_offset;
    mjtNum* quat_offset;
    float* rgba;
    int body_id;
    int group;
    mjtNum* user;

    // --- State (mjData) ---
    mjtNum* xpos;
    mjtNum* xmat;

    SiteView() : _m(nullptr), _d(nullptr), id(-1), name(nullptr),
        type(0), size(nullptr), pos_offset(nullptr), quat_offset(nullptr),
        rgba(nullptr), body_id(-1), group(0), user(nullptr),
        xpos(nullptr), xmat(nullptr) {}

    SiteView(const mjModel* m, mjData* d, int id_in) : _m(m), _d(d), id(id_in) {
        check(id >= 0 && id < m->nsite);
        name = (m->name_siteadr[id] >= 0) ? m->names + m->name_siteadr[id] : nullptr;

        // Config
        type        = m->site_type[id];
        size        = m->site_size + (id * 3);
        pos_offset  = m->site_pos + (id * 3);
        quat_offset = m->site_quat + (id * 4);
        rgba        = m->site_rgba + (id * 4);
        body_id     = m->site_bodyid[id];
        group       = m->site_group[id];
        user        = m->site_user + (id * m->nuser_site);

        // State
        xpos = d->site_xpos + (id * 3);
        xmat = d->site_xmat + (id * 9);
    }

    FVector GetWorldPosition() const {
        return MjUtils::MjToUEPosition(xpos);
    }

    FString ToString() const {
        FString Info = FString::Printf(TEXT("=== Site ID: %d (%s) ===\n"), id, name ? *MjUtils::MjToString(name) : TEXT("None"));
        Info += FString::Printf(TEXT("    Type: %d | Size: %s\n"), type, *FormatVec3(size));
        Info += FString::Printf(TEXT("    World Pos: %s\n"), *FormatVec3(xpos));
        return Info;
    }
};

/**
 * @struct BodyView
 * @brief Lightweight wrapper around MuJoCo body data.
 * Can spawn children views for traversal.
 */
struct BodyView {
    static constexpr mjtObj obj_type = mjOBJ_BODY;

    // We must store these to spawn children views later
    const mjModel* _m;
    mjData* _d;
    int id;

    // --- Identification ---
    const char* name;

    // --- Configuration (mjModel) ---
    mjtNum* mass;
    mjtNum* inertia;
    mjtNum* ipos;
    mjtNum* iquat;
    mjtNum* gravcomp;
    mjtNum* pos_offset;
    mjtNum* quat_offset;
    mjtNum* user;
    int mocap_id;

    // --- State (mjData) ---
    mjtNum* xpos;
    mjtNum* xquat;
    mjtNum* xipos;
    mjtNum* ximat;
    mjtNum* cvel;
    mjtNum* cacc;
    mjtNum* xfrc_applied;
    
    // Initialize critical members to safe defaults so a failed bind doesn't cause a crash later
    BodyView() : _m(nullptr), _d(nullptr), id(-1), name(nullptr),
        mass(nullptr), inertia(nullptr), ipos(nullptr), iquat(nullptr), gravcomp(nullptr),
        pos_offset(nullptr), quat_offset(nullptr), user(nullptr), mocap_id(-1),
        xpos(nullptr), xquat(nullptr), xipos(nullptr), ximat(nullptr),
        cvel(nullptr), cacc(nullptr), xfrc_applied(nullptr) {}

    BodyView(const mjModel* m, mjData* d, int id_in) : _m(m), _d(d), id(id_in) {
        check(id >= 0 && id < m->nbody);
        // Name resolution
        name = (m->name_bodyadr[id] >= 0) ? m->names + m->name_bodyadr[id] : nullptr;

        // Model (Const params)
        mass        = m->body_mass + id;
        inertia     = m->body_inertia + (id * 3);
        ipos        = m->body_ipos + (id * 3);
        iquat       = m->body_iquat + (id * 4);
        gravcomp    = m->body_gravcomp + id;
        pos_offset  = m->body_pos + (id * 3);
        quat_offset = m->body_quat + (id * 4);
        user        = m->body_user + (id * m->nuser_body);
        mocap_id    = m->body_mocapid[id];

        // Data (Dynamic state)
        xpos         = d->xpos + (id * 3);
        xquat        = d->xquat + (id * 4);
        xipos        = d->xipos + (id * 3);
        ximat        = d->ximat + (id * 9);
        cvel         = d->cvel + (id * 6);
        cacc         = d->cacc + (id * 6);
        xfrc_applied = d->xfrc_applied + (id * 6);
    }

    FVector GetWorldPosition() const {
        return MjUtils::MjToUEPosition(xpos);
    }

    FQuat GetWorldRotation() const {
        return MjUtils::MjToUERotation(xquat);
    }

    /** @brief Applies a world-space force (Unreal units/direction) to this body. */
    void ApplyForce(FVector UEForce) {
        if (!xfrc_applied) return;
        mjtNum MjForce[3];
        MjUtils::UEToMjPosition(UEForce, MjForce); // This handles cm->m and Y-flip
        xfrc_applied[3] += MjForce[0];
        xfrc_applied[4] += MjForce[1];
        xfrc_applied[5] += MjForce[2];
    }

    /** @brief Applies a world-space wrench (Force and Torque) to this body. */
    void ApplyWrench(FVector UEForce, FVector UETorque) {
        if (!xfrc_applied) return;
        
        // Force: Convert and add to indices 3, 4, 5
        mjtNum MjForce[3];
        MjUtils::UEToMjPosition(UEForce, MjForce);
        xfrc_applied[3] += MjForce[0];
        xfrc_applied[4] += MjForce[1];
        xfrc_applied[5] += MjForce[2];

        // Torque: Convert and add to indices 0, 1, 2
        // Note: Torque doesn't require the 0.01 scale factor (cm->m) in the same way, 
        // but it does require the Y-flip and coordinate mapping.
        // UEToMjRotation handles the flip/mapping part if treated as a vector.
        // However, torque is often just a vector. 
        // Looking at UMjBody::ApplyForce, it uses Torque.X, -Torque.Y, Torque.Z.
        xfrc_applied[0] += (mjtNum)UETorque.X;
        xfrc_applied[1] += (mjtNum)-UETorque.Y;
        xfrc_applied[2] += (mjtNum)UETorque.Z;
    }

    // Traversal Methods (Declared here, Implemented at the bottom)
    TArray<BodyView> Bodies() const;
    TArray<GeomView> Geoms() const;
    TArray<JointView> Joints() const;

    FString ToString() const {
        FString Info = FString::Printf(TEXT("=== Body ID: %d (%s) ===\n"), id, name ? *MjUtils::MjToString(name) : TEXT("None"));
        
        Info += TEXT("  [Config]\n");
        Info += FString::Printf(TEXT("    Mass: %.4f\n"), *mass);
        Info += FString::Printf(TEXT("    Rel Pos: %s\n"), *FormatVec3(pos_offset));

        Info += TEXT("  [State]\n");
        Info += FString::Printf(TEXT("    World Pos: %s\n"), *FormatVec3(xpos));
        return Info;
    }
};

// --- Body View ---
// Returns a list of geoms attached to this body
inline TArray<GeomView> BodyView::Geoms() const {
    TArray<GeomView> geoms;
    int num = _m->body_geomnum[id];
    int start_adr = _m->body_geomadr[id];
    
    // Check for -1 (no geoms) just in case, though num would be 0
    if (start_adr >= 0) {
        for (int i = 0; i < num; ++i) {
            geoms.Emplace(_m, _d, start_adr + i);
        }
    }
    return geoms;
}
inline TArray<BodyView> BodyView::Bodies() const {
    TArray<BodyView> children;
    // Iterate over all bodies to find those whose parent is the current id
    for (int i = 0; i < _m->nbody; ++i) {
        if (_m->body_parentid[i] == id) {
            children.Emplace(_m, _d, i);
        }
    }
    return children;
}
// Returns a list of joints attached to this body
inline TArray<JointView> BodyView::Joints() const {
    TArray<JointView> joints;
    int num = _m->body_jntnum[id];
    int start_adr = _m->body_jntadr[id];

    if (start_adr >= 0) {
        for (int i = 0; i < num; ++i) {
            joints.Emplace(_m, _d, start_adr + i);
        }
    }
    return joints;
}
// ==========================================
// 2. The Templated Binder Function
// ==========================================

// Recursive function to traverse the hierarchy
inline void LogBodyHierarchy(const BodyView& RootBody, int IndentLevel = 0) {
    // 1. Create an indent string for visual hierarchy
    FString IndentString;
    for (int i = 0; i < IndentLevel; ++i) IndentString += TEXT("  |  ");

    // 2. Log the current Body
    FString BodyLog = FString::Printf(TEXT("%s[Body ID: %d] %s"), 
        *IndentString, 
        RootBody.id, 
        RootBody.name ? *MjUtils::MjToString(RootBody.name) : TEXT("Unnamed"));
    
    UE_LOG(LogURLabBind, Log, TEXT("%s"), *BodyLog);

    // 3. Log all Geoms attached to this Body
    TArray<GeomView> Geoms = RootBody.Geoms();
    for (const GeomView& Geom : Geoms) {
        FString GeomLog = FString::Printf(TEXT("%s  - (Geom ID: %d) Type: %d | Size: %s"), 
            *IndentString, 
            Geom.id, 
            Geom.type, 
            *FormatVec3(Geom.size)); // Uses the FormatVec3 helper defined previously
        
        UE_LOG(LogURLabBind, Verbose, TEXT("%s"), *GeomLog);
    }

    // 4. Recurse into Child Bodies
    TArray<BodyView> Children = RootBody.Bodies();
    for (const BodyView& Child : Children) {
        LogBodyHierarchy(Child, IndentLevel + 1);
    }
}
template <typename T>
T bind(const mjModel* m, mjData* d, const std::string& name) {
    int id = mj_name2id(m, T::obj_type, name.c_str());
    if (id == -1) {
        UE_LOG(LogURLabBind, Warning, TEXT("MuJoCo Bind: Could not find '%hs'"), name.c_str());
        return T();
    }
    return T(m, d, id);
}