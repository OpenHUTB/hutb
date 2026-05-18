from robojudo.policy.policy_cfgs import BeyondMimicPolicyCfg
from robojudo.tools.tool_cfgs import DoFConfig


class G1BeyondMimicDoF(DoFConfig):
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
        *[-0.312, -0.312, 0.000, 0.000, 0.000, 0.000, 0.000, 0.000, 0.000, 0.669, 0.669],
        *[0.200, 0.200, -0.363, -0.363, 0.200, -0.200, 0.000, 0.000, 0.000, 0.000],
        *[0.600, 0.600, 0.000, 0.000, 0.000, 0.000, 0.000, 0.000],
    ]

    stiffness: list[float] | None = [
        *[40.179, 40.179, 40.179, 99.098, 99.098, 28.501, 40.179, 40.179, 28.501, 99.098, 99.098],
        *[14.251, 14.251, 28.501, 28.501, 14.251, 14.251, 28.501, 28.501, 14.251, 14.251],
        *[14.251, 14.251, 14.251, 14.251, 16.778, 16.778, 16.778, 16.778],
    ]

    damping: list[float] | None = [
        *[2.558, 2.558, 2.558, 6.309, 6.309, 1.814, 2.558, 2.558, 1.814, 6.309, 6.309],
        *[0.907, 0.907, 1.814, 1.814, 0.907, 0.907, 1.814, 1.814, 0.907, 0.907],
        *[0.907, 0.907, 0.907, 0.907, 1.068, 1.068, 1.068, 1.068],
    ]


class G1BeyondMimicPolicyCfg(BeyondMimicPolicyCfg):
    robot: str = "g1"

    # policy_name: str = "Jump_wose"
    policy_name: str = "Dance_wose"
    # policy_name: str = "Violin"
    # policy_name: str = "Waltz"

    obs_dof: DoFConfig = G1BeyondMimicDoF()
    action_dof: DoFConfig = obs_dof

    action_beta: float = 1.0
    # ======= POLICY SPECIFIC CONFIGURATION =======
    without_state_estimator: bool = True

    action_scales: list[float] = [
        *[0.548, 0.548, 0.548, 0.351, 0.351, 0.439, 0.548, 0.548, 0.439, 0.351, 0.351],
        *[0.439, 0.439, 0.439, 0.439, 0.439, 0.439, 0.439, 0.439, 0.439, 0.439],
        *[0.439, 0.439, 0.439, 0.439, 0.075, 0.075, 0.075, 0.075],
    ]
