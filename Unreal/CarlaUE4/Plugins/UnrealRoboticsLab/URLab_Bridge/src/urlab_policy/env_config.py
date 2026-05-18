# Copyright (c) 2026 Jonathan Embley-Riches. All rights reserved.
# Licensed under the Apache License, Version 2.0.

"""Configuration for UnrealEnv."""

from dataclasses import dataclass, field
from pathlib import Path

try:
    from robojudo.environment.env_cfgs import EnvCfg
    from robojudo.tools.tool_cfgs import DoFConfig, ForwardKinematicCfg
    HAS_ROBOJUDO = True
except ImportError:
    HAS_ROBOJUDO = False

# ─── G1 12-DOF joint spec ───

G1_12DOF_JOINT_NAMES = [
    "left_hip_pitch_joint", "left_hip_roll_joint", "left_hip_yaw_joint",
    "left_knee_joint", "left_ankle_pitch_joint", "left_ankle_roll_joint",
    "right_hip_pitch_joint", "right_hip_roll_joint", "right_hip_yaw_joint",
    "right_knee_joint", "right_ankle_pitch_joint", "right_ankle_roll_joint",
]

G1_12DOF_DEFAULT_POS = [-0.1, 0.0, 0.0, 0.3, -0.2, 0.0] * 2

G1_12DOF_STIFFNESS = [100, 100, 100, 150, 40, 40] * 2
G1_12DOF_DAMPING = [2, 2, 2, 4, 2, 2] * 2
G1_12DOF_TORQUE_LIMITS = [88, 88, 88, 139, 50, 50] * 2
G1_12DOF_POSITION_LIMITS = [
    [-2.5307, 2.8798], [-0.5236, 2.9671], [-2.7576, 2.7576],
    [-0.087267, 2.8798], [-0.87267, 0.5236], [-0.2618, 0.2618],
    [-2.5307, 2.8798], [-2.9671, 0.5236], [-2.7576, 2.7576],
    [-0.087267, 2.8798], [-0.87267, 0.5236], [-0.2618, 0.2618],
]

# ─── G1 29-DOF joint spec ───

G1_29DOF_JOINT_NAMES = [
    "left_hip_pitch_joint", "left_hip_roll_joint", "left_hip_yaw_joint",
    "left_knee_joint", "left_ankle_pitch_joint", "left_ankle_roll_joint",
    "right_hip_pitch_joint", "right_hip_roll_joint", "right_hip_yaw_joint",
    "right_knee_joint", "right_ankle_pitch_joint", "right_ankle_roll_joint",
    "waist_yaw_joint", "waist_roll_joint", "waist_pitch_joint",
    "left_shoulder_pitch_joint", "left_shoulder_roll_joint", "left_shoulder_yaw_joint",
    "left_elbow_joint", "left_wrist_roll_joint", "left_wrist_pitch_joint", "left_wrist_yaw_joint",
    "right_shoulder_pitch_joint", "right_shoulder_roll_joint", "right_shoulder_yaw_joint",
    "right_elbow_joint", "right_wrist_roll_joint", "right_wrist_pitch_joint", "right_wrist_yaw_joint",
]

G1_29DOF_DEFAULT_POS = [
    *[-0.1, 0.0, 0.0, 0.3, -0.2, 0.0],
    *[-0.1, 0.0, 0.0, 0.3, -0.2, 0.0],
    *[0, 0, 0],
    *[0, 0, 0, 0, 0, 0, 0],
    *[0, 0, 0, 0, 0, 0, 0],
]

# Gains matching BeyondMimic policy (most common 29DOF policy)
# Order matches G1_29DOF_JOINT_NAMES (XML joint order)
G1_29DOF_STIFFNESS = [
    # left leg                          right leg
    40.179, 99.098, 40.179, 99.098, 28.501, 28.501,
    40.179, 99.098, 40.179, 99.098, 28.501, 28.501,
    # waist
    40.179, 28.501, 28.501,
    # left arm                                             right arm
    14.251, 14.251, 14.251, 14.251, 14.251, 16.778, 16.778,
    14.251, 14.251, 14.251, 14.251, 14.251, 16.778, 16.778,
]

G1_29DOF_DAMPING = [
    # left leg                        right leg
    2.558, 6.309, 2.558, 6.309, 1.814, 1.814,
    2.558, 6.309, 2.558, 6.309, 1.814, 1.814,
    # waist
    2.558, 1.814, 1.814,
    # left arm                                          right arm
    0.907, 0.907, 0.907, 0.907, 0.907, 1.068, 1.068,
    0.907, 0.907, 0.907, 0.907, 0.907, 1.068, 1.068,
]

G1_29DOF_TORQUE_LIMITS = [
    *[200, 200, 200, 300, 40, 40],
    *[200, 200, 200, 300, 40, 40],
    *[200, 200, 200],
    *[40, 40, 18, 18, 10, 10, 10],
    *[40, 40, 18, 18, 10, 10, 10],
]

# ─── Go2 12-DOF joint spec (walk-these-ways order: FL, FR, RL, RR) ───

GO2_12DOF_JOINT_NAMES = [
    "FL_hip_joint", "FL_thigh_joint", "FL_calf_joint",
    "FR_hip_joint", "FR_thigh_joint", "FR_calf_joint",
    "RL_hip_joint", "RL_thigh_joint", "RL_calf_joint",
    "RR_hip_joint", "RR_thigh_joint", "RR_calf_joint",
]

GO2_12DOF_DEFAULT_POS = [
    0.1, 0.8, -1.5,   # FL
    -0.1, 0.8, -1.5,  # FR
    0.1, 1.0, -1.5,   # RL
    -0.1, 1.0, -1.5,  # RR
]

GO2_12DOF_STIFFNESS = [25.0] * 12
GO2_12DOF_DAMPING = [0.6] * 12
GO2_12DOF_TORQUE_LIMITS = [45.0] * 12

G1_12DOF = None
G1_29DOF = None
GO2_12DOF = None

if HAS_ROBOJUDO:
    GO2_12DOF = DoFConfig(
        joint_names=GO2_12DOF_JOINT_NAMES,
        default_pos=GO2_12DOF_DEFAULT_POS,
        stiffness=GO2_12DOF_STIFFNESS,
        damping=GO2_12DOF_DAMPING,
        torque_limits=GO2_12DOF_TORQUE_LIMITS,
    )

    G1_12DOF = DoFConfig(
        joint_names=G1_12DOF_JOINT_NAMES,
        default_pos=G1_12DOF_DEFAULT_POS,
        stiffness=G1_12DOF_STIFFNESS,
        damping=G1_12DOF_DAMPING,
        torque_limits=G1_12DOF_TORQUE_LIMITS,
        position_limits=G1_12DOF_POSITION_LIMITS,
    )

    G1_29DOF = DoFConfig(
        joint_names=G1_29DOF_JOINT_NAMES,
        default_pos=G1_29DOF_DEFAULT_POS,
        stiffness=G1_29DOF_STIFFNESS,
        damping=G1_29DOF_DAMPING,
        torque_limits=G1_29DOF_TORQUE_LIMITS,
    )

    class UnrealEnvCfg(EnvCfg):
        """RoboJuDo-compatible config for UnrealEnv."""

        env_type: str = "UnrealEnv"
        is_sim: bool = True
        xml: str = ""  # Not used — Unreal manages the MuJoCo model

        # ZMQ endpoints
        state_endpoint: str = "tcp://127.0.0.1:5555"
        control_endpoint: str = "tcp://127.0.0.1:5556"

        # Which articulation to target (auto-detected if empty)
        articulation_prefix: str = ""

        # Timing: must match Unreal's MuJoCo timestep
        sim_dt: float = 0.002
        sim_decimation: int = 10

    class G1UnrealEnvCfg(UnrealEnvCfg):
        """G1 12-DOF for Unitree locomotion policy."""

        articulation_prefix: str = "g1"
        dof: DoFConfig = G1_12DOF
        forward_kinematic: ForwardKinematicCfg | None = None
        update_with_fk: bool = False
        torso_name: str = "pelvis"

    # Path to the G1 29DOF XML for forward kinematics
    _G1_29_XML = (Path(__file__).parent.parent.parent / "RoboJuDo" / "assets" / "robots" / "g1" / "g1_29dof_rev_1_0.xml").as_posix()

    class G1_29UnrealEnvCfg(UnrealEnvCfg):
        """G1 29-DOF for full-body policies (BeyondMimic, H2H, AMO, etc.)."""

        articulation_prefix: str = "g1"
        dof: DoFConfig = G1_29DOF
        forward_kinematic: ForwardKinematicCfg = ForwardKinematicCfg(
            xml_path=_G1_29_XML,
            debug_viz=False,
            kinematic_joint_names=G1_29DOF_JOINT_NAMES,
        )
        update_with_fk: bool = True
        torso_name: str = "torso_link"

    class Go2UnrealEnvCfg(UnrealEnvCfg):
        """Go2 12-DOF for walk-these-ways locomotion policy."""

        articulation_prefix: str = "go2"
        dof: DoFConfig = GO2_12DOF
        forward_kinematic: ForwardKinematicCfg | None = None
        update_with_fk: bool = False
        torso_name: str = "base"

        # Match WTW IsaacGym training: 200Hz physics, decimation 4 → 50Hz policy
        sim_dt: float = 0.005
        sim_decimation: int = 4

else:
    @dataclass
    class UnrealEnvCfg:
        state_endpoint: str = "tcp://127.0.0.1:5555"
        control_endpoint: str = "tcp://127.0.0.1:5556"
        articulation_prefix: str = ""
        joint_names: list[str] = field(default_factory=list)

    @dataclass
    class G1UnrealEnvCfg(UnrealEnvCfg):
        joint_names: list[str] = field(default_factory=lambda: G1_12DOF_JOINT_NAMES)
