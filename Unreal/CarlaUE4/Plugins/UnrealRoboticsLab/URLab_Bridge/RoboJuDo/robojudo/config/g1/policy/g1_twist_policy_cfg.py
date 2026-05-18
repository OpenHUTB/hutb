from robojudo.policy.policy_cfgs import TwistPolicyCfg
from robojudo.tools.tool_cfgs import DoFConfig


class G1TwistDoF(DoFConfig):
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
        *["waist_yaw_joint", "waist_roll_joint", "waist_pitch_joint"],
        *["left_shoulder_pitch_joint", "left_shoulder_roll_joint", "left_shoulder_yaw_joint", "left_elbow_joint"],
        *["right_shoulder_pitch_joint", "right_shoulder_roll_joint", "right_shoulder_yaw_joint", "right_elbow_joint"],
    ]

    default_pos: list[float] | None = [
        *[-0.2, 0.0, 0.0, 0.4, -0.2, 0.0],
        *[-0.2, 0.0, 0.0, 0.4, -0.2, 0.0],
        *[0.0, 0.0, 0.0],
        *[0.0, 0.2, 0.0, 1.2],
        *[0.0, -0.2, 0.0, 1.2],
    ]

    stiffness: list[float] | None = [
        *[100, 100, 100, 150, 40, 40],
        *[100, 100, 100, 150, 40, 40],
        *[150, 150, 150],
        *[40, 40, 40, 40],
        *[40, 40, 40, 40],
    ]

    damping: list[float] | None = [
        *[2, 2, 2, 4, 2, 2],
        *[2, 2, 2, 4, 2, 2],
        *[4, 4, 4],
        *[5, 5, 5, 5],
        *[5, 5, 5, 5],
    ]

    torque_limits: list[float] | None = [
        *[88, 139, 88, 139, 50, 50],
        *[88, 139, 88, 139, 50, 50],
        *[88, 50, 50],
        *[25, 25, 25, 25],
        *[25, 25, 25, 25],
    ]


class G1TwistPolicyCfg(TwistPolicyCfg):
    robot: str = "g1"
    policy_name: str = "twist_general_motion_tracker"

    obs_dof: DoFConfig = G1TwistDoF()
    action_dof: DoFConfig = obs_dof

    # ======= POLICY SPECIFIC CONFIGURATION =======
    ankle_idx: list[int] = [4, 5, 10, 11]
    mimic_obs_total_degrees: int = 33
    mimic_obs_wrist_ids: list[int] = [27, 32]
