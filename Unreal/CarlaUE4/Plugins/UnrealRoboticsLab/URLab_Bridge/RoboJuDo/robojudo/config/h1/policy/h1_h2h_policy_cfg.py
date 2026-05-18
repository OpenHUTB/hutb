from robojudo.policy.policy_cfgs import H2HPolicyCfg
from robojudo.tools.tool_cfgs import DoFConfig


class H1H2HDoF(DoFConfig):
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
        *[150, 150, 150, 200, 40],
        *[150, 150, 150, 200, 40],
        *[300],
        *[100, 100, 100, 50],
        *[100, 100, 100, 50],
    ]

    damping: list[float] | None = [
        *[2, 2, 2, 4, 2],
        *[2, 2, 2, 4, 2],
        *[3],
        *[2, 2, 2, 2],
        *[2, 2, 2, 2],
    ]


class H1H2HPolicyCfg(H2HPolicyCfg):
    robot: str = "h1"
    policy_name: str = "h2h_release"

    obs_dof: DoFConfig = H1H2HDoF()
    action_dof: DoFConfig = obs_dof

    # ======= POLICY SPECIFIC CONFIGURATION =======
    use_imu_torso: bool = True
