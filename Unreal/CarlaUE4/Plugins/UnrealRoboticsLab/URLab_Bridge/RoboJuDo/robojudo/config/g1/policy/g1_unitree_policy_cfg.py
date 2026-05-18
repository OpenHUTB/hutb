from robojudo.policy.policy_cfgs import UnitreePolicyCfg, UnitreeWoGaitPolicyCfg
from robojudo.tools.tool_cfgs import DoFConfig


class G1UnitreeDoF(DoFConfig):
    joint_names: list[str] = [
        *[
            "left_hip_pitch_joint",
            "left_hip_roll_joint",
            "left_hip_yaw_joint",
            "left_knee_joint",
            "left_ankle_pitch_joint",
            "left_ankle_roll_joint",
        ],
        *[
            "right_hip_pitch_joint",
            "right_hip_roll_joint",
            "right_hip_yaw_joint",
            "right_knee_joint",
            "right_ankle_pitch_joint",
            "right_ankle_roll_joint",
        ],
    ]

    default_pos: list[float] | None = [
        *[-0.1, 0.0, 0.0, 0.3, -0.2, 0.0],
        *[-0.1, 0.0, 0.0, 0.3, -0.2, 0.0],
    ]

    stiffness: list[float] | None = [
        *[100, 100, 100, 150, 40, 40],
        *[100, 100, 100, 150, 40, 40],
    ]

    damping: list[float] | None = [
        *[2, 2, 2, 4, 2, 2],
        *[2, 2, 2, 4, 2, 2],
    ]

    torque_limits: list[float] | None = [
        *[88, 88, 88, 139, 50, 50],
        *[88, 88, 88, 139, 50, 50],
    ]


class G1UnitreeWoGaitDoF(DoFConfig):
    joint_names: list[str] = [
        *[
            "left_hip_pitch_joint",
            "right_hip_pitch_joint",
            "waist_yaw_joint",
            "left_hip_roll_joint",
            "right_hip_roll_joint",
            "waist_roll_joint",
            "left_hip_yaw_joint",
            "right_hip_yaw_joint",
            "waist_pitch_joint",
            "left_knee_joint",
            "right_knee_joint",
        ],
        *[
            "left_shoulder_pitch_joint",
            "right_shoulder_pitch_joint",
            "left_ankle_pitch_joint",
            "right_ankle_pitch_joint",
            "left_shoulder_roll_joint",
            "right_shoulder_roll_joint",
            "left_ankle_roll_joint",
            "right_ankle_roll_joint",
            "left_shoulder_yaw_joint",
            "right_shoulder_yaw_joint",
        ],
        *[
            "left_elbow_joint",
            "right_elbow_joint",
            "left_wrist_roll_joint",
            "right_wrist_roll_joint",
            "left_wrist_pitch_joint",
            "right_wrist_pitch_joint",
            "left_wrist_yaw_joint",
            "right_wrist_yaw_joint",
        ],
    ]

    default_pos: list[float] | None = [
        *[-0.1, -0.1, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.3, 0.3],
        *[0.3, 0.3, -0.2, -0.2, 0.25, -0.25, 0.0, 0.0, 0.0, 0.0],
        *[0.97, 0.97, 0.15, -0.15, 0.0, 0.0, 0.0, 0.0],
    ]

    stiffness: list[float] | None = [
        *[100.0, 100.0, 200.0, 100.0, 100.0, 40.0, 100.0, 100.0, 40.0, 150.0, 150.0],
        *[40.0, 40.0, 40.0, 40.0, 40.0, 40.0, 40.0, 40.0, 40.0, 40.0],
        *[40.0, 40.0, 40.0, 40.0, 40.0, 40.0, 40.0, 40.0],
    ]

    damping: list[float] | None = [
        *[2.0, 2.0, 5.0, 2.0, 2.0, 5.0, 2.0, 2.0, 5.0, 4.0, 4.0],
        *[10.0, 10.0, 2.0, 2.0, 10.0, 10.0, 2.0, 2.0, 10.0, 10.0],
        *[10.0, 10.0, 10.0, 10.0, 10.0, 10.0, 10.0, 10.0],
    ]


class G1UnitreePolicyCfg(UnitreePolicyCfg):
    robot: str = "g1"
    policy_name: str = "policy_lstm_1"

    obs_dof: DoFConfig = G1UnitreeDoF()
    action_dof: DoFConfig = obs_dof


class G1UnitreeWoGaitPolicyCfg(UnitreeWoGaitPolicyCfg):
    robot: str = "g1"
    policy_name: str = "policy_wo_gait"

    obs_dof: DoFConfig = G1UnitreeWoGaitDoF()
    action_dof: DoFConfig = obs_dof

    history_obs_dims: dict[str, int] = {
        "ang_vel": 3,
        "gravity": 3,
        "commands": 3,
        "dof_pos": obs_dof.num_dofs,
        "dof_vel": obs_dof.num_dofs,
        "actions": action_dof.num_dofs,
    }
