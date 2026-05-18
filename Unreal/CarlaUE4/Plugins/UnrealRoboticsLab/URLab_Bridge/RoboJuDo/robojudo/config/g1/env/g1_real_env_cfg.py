from typing import Literal

from robojudo.environment.env_cfgs import UnitreeEnvCfg

# from robojudo.tools.tool_cfgs import ZedOdometryCfg
from .g1_env_cfg import G1EnvCfg


class G1UnitreeCfg(UnitreeEnvCfg.UnitreeCfg):
    robot: Literal["h1", "g1"] = "g1"

    msg_type: Literal["hg", "go"] = "hg"
    hand_type: Literal["Dex-3", "Inspire", "NONE"] = "NONE"

    enable_odometry: bool = True


class G1RealEnvCfg(G1EnvCfg, UnitreeEnvCfg):
    # env_type: str = UnitreeEnvCfg.model_fields["env_type"].default
    env_type: str = "UnitreeCppEnv"
    # ====== ENV CONFIGURATION ======
    unitree: UnitreeEnvCfg.UnitreeCfg = G1UnitreeCfg(
        net_if="eth0",
    )

    odometry_type: Literal["NONE", "DUMMY", "UNITREE", "ZED"] = "UNITREE"

    joint2motor_idx: list[int] | None = None  # list(range(0, 29))


class G1WithHandRealEnvCfg(G1EnvCfg, UnitreeEnvCfg):
    # env_type: str = UnitreeEnvCfg.model_fields["env_type"].default
    env_type: str = "UnitreeCppEnv"
    # ====== ENV CONFIGURATION ======
    unitree: UnitreeEnvCfg.UnitreeCfg = G1UnitreeCfg(
        net_if="eth0",
        hand_type="Dex-3",
    )

    odometry_type: Literal["NONE", "DUMMY", "UNITREE", "ZED"] = "DUMMY"
    # zed_cfg: ZedOdometryCfg | None = ZedOdometryCfg(
    #     server_ip="192.168.123.167",
    #     pos_offset=[0.0, 0.0, 0.9],
    #     zero_align=True,
    # )

    joint2motor_idx: list[int] | None = None  # list(range(0, 29))
