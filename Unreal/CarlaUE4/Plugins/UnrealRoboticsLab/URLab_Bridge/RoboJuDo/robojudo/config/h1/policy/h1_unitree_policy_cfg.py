from robojudo.policy.policy_cfgs import UnitreePolicyCfg
from robojudo.tools.tool_cfgs import DoFConfig


class H1UnitreeDoF(DoFConfig):
    joint_names: list[str] = [
        *["left_hip_yaw_joint", "left_hip_roll_joint", "left_hip_pitch_joint", "left_knee_joint", "left_ankle_joint"],
        *[
            "right_hip_yaw_joint",
            "right_hip_roll_joint",
            "right_hip_pitch_joint",
            "right_knee_joint",
            "right_ankle_joint",
        ],
    ]

    default_pos: list[float] | None = [
        *[0, 0.0, -0.1, 0.3, -0.2],
        *[0, 0.0, -0.1, 0.3, -0.2],
    ]

    stiffness: list[float] | None = [
        *[150, 150, 150, 200, 40],
        *[150, 150, 150, 200, 40],
    ]

    damping: list[float] | None = [
        *[2, 2, 2, 4, 2],
        *[2, 2, 2, 4, 2],
    ]


class H1UnitreePolicyCfg(UnitreePolicyCfg):
    robot: str = "h1"
    policy_name: str = "motion"  # from unitree official github repo

    obs_dof: DoFConfig = H1UnitreeDoF()
    action_dof: DoFConfig = obs_dof
