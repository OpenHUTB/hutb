from robojudo.policy.policy_cfgs import SmoothPolicyCfg
from robojudo.tools.tool_cfgs import DoFConfig


class G1SmoothDoF(DoFConfig):
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
        *["waist_yaw_joint"],
        *["left_shoulder_pitch_joint", "left_shoulder_roll_joint", "left_shoulder_yaw_joint", "left_elbow_joint"],
        *["right_shoulder_pitch_joint", "right_shoulder_roll_joint", "right_shoulder_yaw_joint", "right_elbow_joint"],
    ]

    default_pos: list[float] | None = [
        *[-0.4, 0.0, 0.0, 0.8, -0.35, 0.0],
        *[-0.4, 0.0, 0.0, 0.8, -0.35, 0.0],
        *[0],
        *[0, 0, 0, 0],
        *[0, 0, 0, 0],
    ]

    stiffness: list[float] | None = [
        *[200, 150, 150, 200, 20, 20],
        *[200, 150, 150, 200, 20, 20],
        *[200],
        *[40, 40, 40, 40],
        *[40, 40, 40, 40],
    ]

    damping: list[float] | None = [
        *[5, 5, 5, 5, 4, 4],
        *[5, 5, 5, 5, 4, 4],
        *[5],
        *[10, 10, 10, 10],
        *[10, 10, 10, 10],
    ]

    torque_limits: list[float] | None = [
        *[88, 88, 88, 139, 50, 50],
        *[88, 88, 88, 139, 50, 50],
        *[88],
        *[25, 25, 25, 25],
        *[25, 25, 25, 25],
    ]


class G1SmoothPolicyCfg(SmoothPolicyCfg):
    robot: str = "g1"
    policy_name: str = "for_test-2000-jit"

    obs_dof: DoFConfig = G1SmoothDoF()
    action_dof: DoFConfig = obs_dof

    cycle_time: float = 0.64
    commands_map: list[list[float]] = [
        [-1.5, -0.5, 0.5],
        [0.25, 0.0, -0.25],
        [0.5, -0.5, -1.5],
    ]
