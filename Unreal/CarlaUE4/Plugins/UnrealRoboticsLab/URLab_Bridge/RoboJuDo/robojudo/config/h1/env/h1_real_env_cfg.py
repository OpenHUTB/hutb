from typing import Literal

from robojudo.environment.env_cfgs import UnitreeEnvCfg

# from robojudo.tools.tool_cfgs import ZedOdometryCfg
from .h1_env_cfg import H1EnvCfg


class H1UnitreeCfg(UnitreeEnvCfg.UnitreeCfg):
    robot: Literal["h1", "g1"] = "h1"

    msg_type: Literal["hg", "go"] = "go"
    hand_type: Literal["Dex-3", "Inspire", "NONE"] = "NONE"

    enable_odometry: bool = False


class H1RealEnvCfg(H1EnvCfg, UnitreeEnvCfg):
    # env_type: str = UnitreeEnvCfg.model_fields["env_type"].default
    env_type: str = "UnitreeEnv"
    # ====== ENV CONFIGURATION ======
    unitree: UnitreeEnvCfg.UnitreeCfg = H1UnitreeCfg(
        net_if="eno2",
    )

    odometry_type: Literal["NONE", "DUMMY", "UNITREE", "ZED"] = "DUMMY"
    # zed_cfg: ZedOdometryCfg | None = ZedOdometryCfg(
    #     server_ip="192.168.123.167",
    #     pos_offset=[0.0, 0.0, 0.9],
    #     zero_align=True,
    # )

    weak_motor: list[int] = [10, 11, 12, 13, 14, 15, 16, 17, 18, 19]
    joint2motor_idx: list[int] | None = [7, 3, 4, 5, 10, 8, 0, 1, 2, 11, 6, 16, 17, 18, 19, 12, 13, 14, 15]

    hand_retarget: None = None  # TODO

    # hand_retarget:
    #     reverse_fingers_order: True
    #     fingers_pose: [0.0, 1.7]
    #     thumb_bending_pose: [-0.1, 0.6]
    #     thumb_rotation_pose: [-0.1, 1.3]
    #     fingers_cmd: [1000, 0.0]
    #     thumb_bending_cmd: [1000, 0.0]
    #     thumb_rotation_cmd: [1000, 0.0]
