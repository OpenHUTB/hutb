from robojudo.config import ASSETS_DIR
from robojudo.environment.env_cfgs import EnvCfg
from robojudo.tools.tool_cfgs import DoFConfig, ForwardKinematicCfg


class G1_29DoF(DoFConfig):
    # num_dofs as 29
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
        *[0, 0, 0],
        *[0, 0, 0, 0, 0, 0, 0],
        *[0, 0, 0, 0, 0, 0, 0],
    ]

    stiffness: list[float] | None = [
        *[100, 100, 100, 150, 40, 40],
        *[100, 100, 100, 150, 40, 40],
        *[200, 200, 200],
        *[40, 40, 40, 40, 20, 20, 20],
        *[40, 40, 40, 40, 20, 20, 20],
    ]

    damping: list[float] | None = [
        *[5, 5, 5, 5, 2, 2],
        *[5, 5, 5, 5, 2, 2],
        *[6, 6, 6],
        *[2, 2, 2, 2, 2, 2, 2],
        *[2, 2, 2, 2, 2, 2, 2],
    ]

    torque_limits: list[float] | None = [
        *[200, 200, 200, 300, 40, 40],
        *[200, 200, 200, 300, 40, 40],
        *[200, 200, 200],
        *[40, 40, 18, 18, 10, 10, 10],
        *[40, 40, 18, 18, 10, 10, 10],
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


class G1_23DoF(G1_29DoF):
    # num_dofs as 23
    _subset: bool = True  # if True, simplely inheritance & pick

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
        *["waist_yaw_joint"],
        *[
            "left_shoulder_pitch_joint",
            "left_shoulder_roll_joint",
            "left_shoulder_yaw_joint",
            "left_elbow_joint",
            "left_wrist_roll_joint",
        ],
        *[
            "right_shoulder_pitch_joint",
            "right_shoulder_roll_joint",
            "right_shoulder_yaw_joint",
            "right_elbow_joint",
            "right_wrist_roll_joint",
        ],
    ]


class G1_12DoF(G1_29DoF):
    # num_dofs as 12
    _subset: bool = True  # if True, simplely inheritance & pick

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


class G1EnvCfg(EnvCfg):
    xml: str = (ASSETS_DIR / "robots/g1/g1_29dof_rev_1_0.xml").as_posix()

    dof: DoFConfig = G1_29DoF()

    forward_kinematic: ForwardKinematicCfg | None = ForwardKinematicCfg(
        xml_path=xml,
        debug_viz=False,
        kinematic_joint_names=dof.joint_names,
    )
    update_with_fk: bool = True
    torso_name: str = "torso_link"


class G1_23EnvCfg(EnvCfg):
    xml: str = (ASSETS_DIR / "robots/g1/g1_23dof_rev_1_0.xml").as_posix()

    dof: DoFConfig = G1_23DoF()

    forward_kinematic: ForwardKinematicCfg | None = ForwardKinematicCfg(
        xml_path=xml,
        debug_viz=False,
        kinematic_joint_names=dof.joint_names,
    )
    update_with_fk: bool = True
    torso_name: str = "torso_link"


class G1_12EnvCfg(EnvCfg):
    xml: str = (ASSETS_DIR / "robots/g1/g1_12dof.xml").as_posix()

    dof: DoFConfig = G1_12DoF()

    forward_kinematic: ForwardKinematicCfg | None = ForwardKinematicCfg(
        xml_path=xml,
        debug_viz=False,
        kinematic_joint_names=dof.joint_names,
    )
    update_with_fk: bool = False
    torso_name: str = "pelvis"  # no torso in 12dof model
