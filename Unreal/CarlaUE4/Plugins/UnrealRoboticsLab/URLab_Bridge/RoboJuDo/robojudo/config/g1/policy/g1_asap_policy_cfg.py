from robojudo.policy.policy_cfgs import AsapLocoPolicyCfg, AsapPolicyCfg
from robojudo.tools.tool_cfgs import DoFConfig


class G1_29AsapDoF(DoFConfig):
    """robot_dof as g1_29dof"""

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
        *[
            "left_shoulder_pitch_joint",
            "left_shoulder_roll_joint",
            "left_shoulder_yaw_joint",
            "left_elbow_joint",
            "left_wrist_roll_joint",
            "left_wrist_pitch_joint",
            "left_wrist_yaw_joint",
        ],
        *[
            "right_shoulder_pitch_joint",
            "right_shoulder_roll_joint",
            "right_shoulder_yaw_joint",
            "right_elbow_joint",
            "right_wrist_roll_joint",
            "right_wrist_pitch_joint",
            "right_wrist_yaw_joint",
        ],
    ]

    default_pos: list[float] | None = [
        *[-0.1, 0.0, 0.0, 0.3, -0.2, 0.0],
        *[-0.1, 0.0, 0.0, 0.3, -0.2, 0.0],
        *[0.0, 0.0, 0.0],
        *[0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0],
        *[0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0],
    ]

    stiffness: list[float] | None = [
        *[100, 100, 100, 200, 20, 20],
        *[100, 100, 100, 200, 20, 20],
        *[400, 400, 400],
        *[90, 60, 20, 60, 4, 4, 4],  # original as 0, bug?
        *[90, 60, 20, 60, 4, 4, 4],
    ]

    damping: list[float] | None = [
        *[2.5, 2.5, 2.5, 5, 0.2, 0.1],
        *[2.5, 2.5, 2.5, 5, 0.2, 0.1],
        *[5.0, 5.0, 5.0],
        *[2.0, 1.0, 0.4, 1.0, 0.2, 0.2, 0.2],
        *[2.0, 1.0, 0.4, 1.0, 0.2, 0.2, 0.2],
    ]

    position_limits: list[list[float]] | None = [
        *[
            [-2.5307, 2.8798],
            [-0.5236, 2.9671],
            [-2.7576, 2.7576],
            [-0.087267, 2.8798],
            [-0.87267, 0.5236],
            [-0.2618, 0.2618],
        ],
        *[
            [-2.5307, 2.8798],
            [-2.9671, 0.5236],
            [-2.7576, 2.7576],
            [-0.087267, 2.8798],
            [-0.87267, 0.5236],
            [-0.2618, 0.2618],
        ],
        *[[-2.618, 2.618], [-0.52, 0.52], [-0.52, 0.52]],
        *[
            [-3.0892, 2.6704],
            [-1.5882, 2.2515],
            [-2.618, 2.618],
            [-1.0472, 2.0944],
            [-1.972222054, 1.972222054],
            [-1.614429558, 1.614429558],
            [-1.614429558, 1.614429558],
        ],
        *[
            [-3.0892, 2.6704],
            [-2.2515, 1.5882],
            [-2.618, 2.618],
            [-1.0472, 2.0944],
            [-1.972222054, 1.972222054],
            [-1.614429558, 1.614429558],
            [-1.614429558, 1.614429558],
        ],
    ]

    torque_limits: list[float] | None = [
        *[88.0, 88.0, 88.0, 139.0, 50.0, 50.0],
        *[88.0, 88.0, 88.0, 139.0, 50.0, 50.0],
        *[88.0, 50.0, 50.0],
        *[25.0, 25.0, 25.0, 25.0, 25.0, 5.0, 5.0],
        *[25.0, 25.0, 25.0, 25.0, 25.0, 5.0, 5.0],
    ]


class G1_23AsapDoF(G1_29AsapDoF):
    """robot_dof as g1_29dof_anneal_23dof"""

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


class G1_12AsapDoF(G1_29AsapDoF):
    """robot_dof as g1_29dof_anneal_23dof, for loco lower body 12dof only"""

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
    ]


class G1AsapPolicyCfg(AsapPolicyCfg):
    robot: str = "g1"

    # ======= MOTION CONFIGURATION =======
    policy_name: str = "CR7_level1"
    relative_path: str = "model_191500.onnx"

    motion_length_s: float = 3.967

    start_upper_body_dof_pos: list[float] | None = [
        *[0.19964170455932617, 0.07710712403059006, -0.2882401943206787],
        *[0.21672365069389343, 0.15629297494888306, -0.5167576670646667, 0.5782126784324646],
        *[0.25740593671798706, -0.2504104673862457, 0.22500675916671753, 0.5127624273300171],
    ]

    # ======= POLICY DOF CONFIGURATION =======

    obs_dof: DoFConfig = G1_23AsapDoF()
    action_dof: DoFConfig = obs_dof

    # ======= POLICY SPECIFIC CONFIGURATION =======
    obs_scales: AsapPolicyCfg.ObsScalesCfg = AsapPolicyCfg.ObsScalesCfg(
        # base_lin_vel=2.0,
        base_ang_vel=0.25,
        projected_gravity=1.0,
        # command_lin_vel=1.0,
        # command_ang_vel=1.0,
        # command_stand=1.0,
        # command_base_height=2.0,  # Yuanhang: it's 2, not 1!
        # ref_upper_dof_pos=1.0,
        dof_pos=1.0,
        dof_vel=0.05,
        history=1.0,
        actions=1.0,
        # phase_time=1.0,
        ref_motion_phase=1.0,
        # sin_phase=1.0,
        # cos_phase=1.0,
    )

    history_length: int = 4  # from history_mimic_config
    history_obs_dims: dict[str, int] = {  # from obs_mimic_dims, SORTED by key!!!
        "actions": action_dof.num_dofs,  # full body actions
        "base_ang_vel": 3,
        "dof_pos": obs_dof.num_dofs,
        "dof_vel": obs_dof.num_dofs,
        "projected_gravity": 3,
        "ref_motion_phase": 1,  # mimic motion phase
    }

    USE_HISTORY: bool = True


class G1AsapLocoPolicyCfg(AsapLocoPolicyCfg):
    robot: str = "g1"

    policy_name: str = "20250109_231507-noDR_rand_history_loco_stand_height_noise-decoupled_locomotion-g1_29dof"
    relative_path: str = "model_6600.onnx"

    obs_dof: DoFConfig = G1_29AsapDoF()
    action_dof: DoFConfig = G1_12AsapDoF()

    # ======= POLICY SPECIFIC CONFIGURATION =======
    obs_scales: AsapLocoPolicyCfg.ObsScalesCfg = AsapLocoPolicyCfg.ObsScalesCfg(
        # base_lin_vel=2.0,
        base_ang_vel=0.25,
        projected_gravity=1.0,
        command_lin_vel=1.0,
        command_ang_vel=1.0,
        command_base_height=2.0,  # Yuanhang: it's 2, not 1!
        command_stand=1.0,
        ref_upper_dof_pos=1.0,
        dof_pos=1.0,
        dof_vel=0.05,
        history=1.0,
        actions=1.0,
        # phase_time=1.0,
        ref_motion_phase=1.0,
        sin_phase=1.0,
        cos_phase=1.0,
    )
    NUM_UPPER_BODY_JOINTS: int = 17

    history_length: int = 4  # from history_mimic_config
    history_obs_dims: dict[str, int] = {  # from obs_mimic_dims
        "actions": obs_dof.num_dofs - NUM_UPPER_BODY_JOINTS,  # lower body actions
        "base_ang_vel": 3,
        "command_ang_vel": 1,
        "command_base_height": 1,
        "command_lin_vel": 2,
        "command_stand": 1,
        "cos_phase": 1,
        "dof_pos": obs_dof.num_dofs,
        "dof_vel": obs_dof.num_dofs,
        # "phase_time": 1,
        "projected_gravity": 3,
        "ref_upper_dof_pos": NUM_UPPER_BODY_JOINTS,  # upper body actions
        "sin_phase": 1,
    }

    USE_HISTORY: bool = True
    GAIT_PERIOD: float = 0.8  # 1.25

    # ======= Default Command CONFIGURATION =======
    ref_upper_dof_pos_default: list[float] | None = [
        *[0.0, 0.0, 0.0],
        *[0.0, 0.3, 0.0, 1.0, 0.0, 0.0, 0.0],
        *[0.0, -0.3, 0.0, 1.0, 0.0, 0.0, 0.0],
    ]
    command_base_height_default: float = 0.78
