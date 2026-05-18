from robojudo.policy.policy_cfgs import KungfuBotGeneralPolicyCfg
from robojudo.tools.tool_cfgs import DoFConfig

from .g1_asap_policy_cfg import G1_29AsapDoF, G1AsapPolicyCfg


class G1_29KongfuBotDoF(G1_29AsapDoF):
    """robot_dof as g1_29dof, for KungfuBot PBHC"""

    default_pos: list[float] | None = [
        *[-0.1, 0, 0, 0.3, -0.2, 0],
        *[-0.1, 0, 0, 0.3, -0.2, 0],
        *[0, 0, 0],
        *[0.2, 0.2, 0, 0.9, 0, 0, 0],
        *[0.2, -0.2, 0, 0.9, 0, 0, 0],
    ]

    stiffness: list[float] | None = [
        *[100, 100, 100, 150, 40, 40],
        *[100, 100, 100, 150, 40, 40],
        *[400, 400, 400],
        *[100, 100, 50, 50, 4, 4, 4],
        *[100, 100, 50, 50, 4, 4, 4],
    ]

    damping: list[float] | None = [
        *[2.0, 2.0, 2.0, 4.0, 2.0, 2.0],
        *[2.0, 2.0, 2.0, 4.0, 2.0, 2.0],
        *[5.0, 5.0, 5.0],
        *[2.0, 2.0, 2.0, 2.0, 0.2, 0.2, 0.2],
        *[2.0, 2.0, 2.0, 2.0, 0.2, 0.2, 0.2],
    ]


class G1_23KongfuBotDoF(G1_29KongfuBotDoF):
    """robot_dof as 23dof, for KungfuBot PBHC"""

    _subset: bool = True
    _subset_joint_names: list[str] | None = [
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
        *[
            "left_shoulder_pitch_joint",
            "left_shoulder_roll_joint",
            "left_shoulder_yaw_joint",
            "left_elbow_joint",
            # "left_wrist_roll_joint",
            # "left_wrist_pitch_joint",
            # "left_wrist_yaw_joint",
        ],
        *[
            "right_shoulder_pitch_joint",
            "right_shoulder_roll_joint",
            "right_shoulder_yaw_joint",
            "right_elbow_joint",
            # "right_wrist_roll_joint",
            # "right_wrist_pitch_joint",
            # "right_wrist_yaw_joint",
        ],
    ]


class G1_23KongfuGBotGeneralDoF(DoFConfig):
    """robot_dof as g1_23dof, for PBHC KungfuBot2 Genreral Tracking"""

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
        *[-0.1, 0, 0, 0.3, -0.2, 0],
        *[-0.1, 0, 0, 0.3, -0.2, 0],
        *[0, 0, 0],
        *[0.2, 0.2, 0, 0.9],
        *[0.2, -0.2, 0, 0.9],
    ]

    stiffness: list[float] | None = [
        *[
            40.17923847137318,
            99.09842777666113,
            40.17923847137318,
            99.09842777666113,
            28.50124619574858,
            28.50124619574858,
        ],
        *[
            40.17923847137318,
            99.09842777666113,
            40.17923847137318,
            99.09842777666113,
            28.50124619574858,
            28.50124619574858,
        ],
        *[40.17923847137318, 28.50124619574858, 28.50124619574858],
        *[14.25062309787429, 14.25062309787429, 14.25062309787429, 14.25062309787429],
        *[14.25062309787429, 14.25062309787429, 14.25062309787429, 14.25062309787429],
    ]

    damping: list[float] | None = [
        *[
            2.5578897650279457,
            6.3088018534966395,
            2.5578897650279457,
            6.3088018534966395,
            1.814445686584846,
            1.814445686584846,
        ],
        *[
            2.5578897650279457,
            6.3088018534966395,
            2.5578897650279457,
            6.3088018534966395,
            1.814445686584846,
            1.814445686584846,
        ],
        *[2.5578897650279457, 1.814445686584846, 1.814445686584846],
        *[0.907222843292423, 0.907222843292423, 0.907222843292423, 0.907222843292423],
        *[0.907222843292423, 0.907222843292423, 0.907222843292423, 0.907222843292423],
    ]


class G1KungfuBotPolicyCfg(G1AsapPolicyCfg):
    # ======= POLICY DOF CONFIGURATION =======
    obs_dof: DoFConfig = G1_23KongfuBotDoF()
    action_dof: DoFConfig = obs_dof

    # ======= MOTION CONFIGURATION =======
    policy_name: str = "kongfu"
    relative_path: str = "horse_squat.onnx"

    motion_length_s: float = 6.67

    start_upper_body_dof_pos: list[float] | None = [
        *[-5.0488499e-04, 7.1762636e-04, 2.9380890e-03],
        *[2.0567834e-01, 1.9635735e-01, 4.9880091e-03, 8.9015263e-01],
        *[2.0773219e-01, -1.9702259e-01, -2.3902415e-03, 8.8817328e-01],
    ]


class G1KungfuBotGeneralPolicyCfg(KungfuBotGeneralPolicyCfg):
    robot: str = "g1"

    # ======= MOTION CONFIGURATION =======
    policy_name: str = "horse_test_43000"  # this is a test model trained with only one motion

    # ======= POLICY DOF CONFIGURATION =======

    obs_dof: DoFConfig = G1_23KongfuGBotGeneralDoF()
    action_dof: DoFConfig = obs_dof

    action_scales: list[float] = [
        *[
            0.5475464652142303,
            0.3506614663788243,
            0.5475464652142303,
            0.3506614663788243,
            0.43857731392336724,
            0.43857731392336724,
        ],  # noqa: E501
        *[
            0.5475464652142303,
            0.3506614663788243,
            0.5475464652142303,
            0.3506614663788243,
            0.43857731392336724,
            0.43857731392336724,
        ],  # noqa: E501
        *[0.5475464652142303, 0.43857731392336724, 0.43857731392336724],
        *[0.43857731392336724, 0.43857731392336724, 0.43857731392336724, 0.43857731392336724],
        *[0.43857731392336724, 0.43857731392336724, 0.43857731392336724, 0.43857731392336724],
    ]

    # ======= POLICY SPECIFIC CONFIGURATION =======
    obs_scales: KungfuBotGeneralPolicyCfg.ObsScalesCfg = KungfuBotGeneralPolicyCfg.ObsScalesCfg(
        # base_lin_vel=2.0,
        base_ang_vel=0.25,
        dof_pos=1.0,
        dof_vel=0.05,
        actions=1.0,
        roll_pitch=1.0,
        anchor_ref_rot=1.0,
        next_step_ref_motion=1.0,
        history=1.0,
        future_motion_root_height=1.0,
        future_motion_roll_pitch=1.0,
        future_motion_base_lin_vel=1.0,
        future_motion_base_yaw_vel=1.0,
        future_motion_dof_pos=1.0,
    )

    history_length: int = 10
    history_obs_dims: dict[str, int] = {  # SORTED by key!!!
        "actions": obs_dof.num_dofs,
        "base_ang_vel": 3,
        "dof_pos": obs_dof.num_dofs,
        "dof_vel": obs_dof.num_dofs,
        "roll_pitch": 2,
    }

    compatibility_old_version: bool = False
    """For old version of kungfubot general policy (before 2025-11-13 bugfix #68)"""
