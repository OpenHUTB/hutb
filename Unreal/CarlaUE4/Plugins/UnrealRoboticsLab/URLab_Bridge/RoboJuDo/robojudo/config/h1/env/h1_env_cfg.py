from robojudo.config import ASSETS_DIR
from robojudo.environment.env_cfgs import EnvCfg
from robojudo.tools.tool_cfgs import DoFConfig, ForwardKinematicCfg


class H1DoF(DoFConfig):
    # num_dofs as 19
    joint_names: list[str] = [
        *["left_hip_yaw_joint", "left_hip_roll_joint", "left_hip_pitch_joint", "left_knee_joint", "left_ankle_joint"],
        *[
            "right_hip_yaw_joint",
            "right_hip_roll_joint",
            "right_hip_pitch_joint",
            "right_knee_joint",
            "right_ankle_joint",
        ],
        *["torso_joint"],
        *["left_shoulder_pitch_joint", "left_shoulder_roll_joint", "left_shoulder_yaw_joint", "left_elbow_joint"],
        *["right_shoulder_pitch_joint", "right_shoulder_roll_joint", "right_shoulder_yaw_joint", "right_elbow_joint"],
    ]
    default_pos: list[float] | None = [
        *[0.0, 0.0, -0.4, 0.8, -0.4],
        *[0.0, 0.0, -0.4, 0.8, -0.4],
        *[0.0],
        *[0.0, 0.0, 0.0, 0.0],
        *[0.0, 0.0, 0.0, 0.0],
    ]

    stiffness: list[float] | None = [
        *[200, 200, 200, 300, 40],
        *[200, 200, 200, 300, 40],
        *[300],
        *[100, 100, 100, 100],
        *[100, 100, 100, 100],
    ]

    damping: list[float] | None = [
        *[5, 5, 5, 6, 2],
        *[5, 5, 5, 6, 2],
        *[6],
        *[2, 2, 2, 2],
        *[2, 2, 2, 2],
    ]

    torque_limits: list[float] | None = [
        *[200, 200, 200, 300, 40],
        *[200, 200, 200, 300, 40],
        *[200],
        *[40, 40, 18, 18],
        *[40, 40, 18, 18],
    ]

    position_limits: list[list[float]] | None = [
        *[[-0.43, 0.43], [-0.43, 0.43], [-3.14, 2.53], [-0.26, 2.05], [-0.87, 0.52]],
        *[[-0.43, 0.43], [-0.43, 0.43], [-3.14, 2.53], [-0.26, 2.05], [-0.87, 0.52]],
        *[[-2.35, 2.35]],
        *[[-2.87, 2.87], [-3.11, 0.34], [-4.45, 1.3], [-1.25, 2.61]],
        *[[-2.87, 2.87], [-0.34, 3.11], [-1.3, 4.45], [-1.25, 2.61]],
    ]


class H1EnvCfg(EnvCfg):
    xml: str = (ASSETS_DIR / "robots/h1/h1.xml").as_posix()

    dof: DoFConfig = H1DoF()

    forward_kinematic: ForwardKinematicCfg | None = ForwardKinematicCfg(
        xml_path=xml,
        debug_viz=False,
        kinematic_joint_names=dof.joint_names,
    )
    update_with_fk: bool = True

    torso_name: str = "torso_link"
