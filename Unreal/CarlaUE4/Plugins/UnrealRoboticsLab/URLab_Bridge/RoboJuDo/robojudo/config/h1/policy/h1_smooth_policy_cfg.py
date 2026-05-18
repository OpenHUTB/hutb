from robojudo.policy.policy_cfgs import SmoothPolicyCfg
from robojudo.tools.tool_cfgs import DoFConfig


class H1SmoothDoF(DoFConfig):
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
        *[0.0, 0.0, -0.6, 1.2, -0.6],  # left leg (5)
        *[0.0, 0.0, -0.6, 1.2, -0.6],  # right leg (5)
        *[0.0],  # waist (1)
        *[0.0, 0.0, 0.0, 0.0],  # left arm (4)
        *[0.0, 0.0, 0.0, 0.0],  # right arm (4)
    ]

    stiffness: list[float] | None = [
        *[200, 200, 200, 200, 40],
        *[200, 200, 200, 200, 40],
        *[300],
        *[40, 40, 40, 40],
        *[40, 40, 40, 40],
    ]

    damping: list[float] | None = [
        *[5, 5, 5, 5, 2],
        *[5, 5, 5, 5, 2],
        *[6],
        *[2.0, 2.0, 2.0, 2.0],
        *[2.0, 2.0, 2.0, 2.0],
    ]

    torque_limits: list[float] | None = [
        *[200, 200, 200, 300, 40],
        *[200, 200, 200, 300, 40],
        *[200],
        *[40, 40, 18, 18],
        *[40, 40, 18, 18],
    ]


class H1SmoothPolicyCfg(SmoothPolicyCfg):
    robot: str = "h1"
    policy_name: str = "train_default-14000-jit"

    obs_dof: DoFConfig = H1SmoothDoF()
    action_dof: DoFConfig = obs_dof

    cycle_time: float = 0.8
    commands_map: list[list[float]] = [
        [-0.5, 0.1, 1.0],
        [0.25, 0.05, -0.25],
        [3.0, 1.5, 0.0],
    ]
