from typing import Literal

from robojudo.environment.env_cfgs import RobotEnvCfg
from robojudo.tools.tool_cfgs import ZedOdometryCfg  # noqa: F401

# from robojudo.tools.tool_cfgs import ForwardKinematicCfg
from .h1_env_cfg import H1EnvCfg


class H1DummyEnvCfg(H1EnvCfg, RobotEnvCfg):
    env_type: str = RobotEnvCfg.model_fields["env_type"].default
    # ====== ENV CONFIGURATION ======
    odometry_type: Literal["NONE", "DUMMY", "ZED"] = "DUMMY"
    # zed_cfg: ZedOdometryCfg | None = ZedOdometryCfg(
    #     server_ip="192.168.123.167",
    #     pos_offset=[0.0, 0.0, 0.9],
    #     zero_align=True,
    # )

    # forward_kinematic: ForwardKinematicCfg | None = None
