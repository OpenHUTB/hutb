from robojudo.policy.policy_cfgs import AMOPolicyCfg
from robojudo.tools.tool_cfgs import DoFConfig


class G1AmoDoF(DoFConfig):
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
        *[-0.1, 0.0, 0.0, 0.3, -0.2, 0.0],
        *[-0.1, 0.0, 0.0, 0.3, -0.2, 0.0],
        *[0.0, 0.0, 0.0],
        *[0.5, 0.0, 0.2, 0.3],
        *[0.5, 0.0, -0.2, 0.3],
    ]

    stiffness: list[float] | None = [
        *[150, 150, 150, 300, 80, 20],
        *[150, 150, 150, 300, 80, 20],
        *[400, 400, 400],
        *[80, 80, 40, 60],
        *[80, 80, 40, 60],
    ]

    damping: list[float] | None = [
        *[2, 2, 2, 4, 2, 1],
        *[2, 2, 2, 4, 2, 1],
        *[15, 15, 15],
        *[2, 2, 1, 1],
        *[2, 2, 1, 1],
    ]

    torque_limits: list[float] | None = [
        *[88, 139, 88, 139, 50, 50],
        *[88, 139, 88, 139, 50, 50],
        *[88, 50, 50],
        *[25, 25, 25, 25],
        *[25, 25, 25, 25],
    ]


class G1AmoLowerDoF(G1AmoDoF):
    _subset = True
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
    ]


class G1AmoPolicyCfg(AMOPolicyCfg):
    robot: str = "g1"

    obs_dof: DoFConfig = G1AmoDoF()
    action_dof: DoFConfig = G1AmoLowerDoF()

    commands_map: list[list[float]] = [
        [-1.0, 0.0, 1.0],  # vel_y
        [0.2, 0.0, -0.2],  # ang_z
        [0.8, 0.0, -0.8],  # vel_x
        [0.3, 0.75, 0.9],  # height
    ]
