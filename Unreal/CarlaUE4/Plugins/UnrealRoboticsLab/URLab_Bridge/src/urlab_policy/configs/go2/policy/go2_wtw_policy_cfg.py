# Copyright (c) 2026 Jonathan Embley-Riches. All rights reserved.
# Licensed under the Apache License, Version 2.0.

from urlab_policy.configs.wtw_policy_cfg import WalkTheseWaysPolicyCfg
from robojudo.tools.tool_cfgs import DoFConfig


class Go2WtwDoF(DoFConfig):
    """Go2 12-DOF in walk-these-ways joint order: FL, FR, RL, RR."""

    joint_names: list[str] = [
        "FL_hip_joint", "FL_thigh_joint", "FL_calf_joint",
        "FR_hip_joint", "FR_thigh_joint", "FR_calf_joint",
        "RL_hip_joint", "RL_thigh_joint", "RL_calf_joint",
        "RR_hip_joint", "RR_thigh_joint", "RR_calf_joint",
    ]

    default_pos: list[float] | None = [
        0.1, 0.8, -1.5,   # FL
        -0.1, 0.8, -1.5,  # FR
        0.1, 1.0, -1.5,   # RL
        -0.1, 1.0, -1.5,  # RR
    ]

    stiffness: list[float] | None = [25.0] * 12
    damping: list[float] | None = [0.6] * 12
    torque_limits: list[float] | None = [45.0] * 12


class Go2WtwPolicyCfg(WalkTheseWaysPolicyCfg):
    robot: str = "go2"

    obs_dof: DoFConfig = Go2WtwDoF()
    action_dof: DoFConfig = obs_dof
