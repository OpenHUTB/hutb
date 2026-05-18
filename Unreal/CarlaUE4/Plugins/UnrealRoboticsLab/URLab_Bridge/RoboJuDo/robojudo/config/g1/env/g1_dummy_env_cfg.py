from typing import Literal

from robojudo.environment.env_cfgs import RobotEnvCfg
from robojudo.tools.tool_cfgs import ZedOdometryCfg  # noqa: F401

# from robojudo.tools.tool_cfgs import ForwardKinematicCfg
from .g1_env_cfg import G1EnvCfg


class G1DummyEnvCfg(G1EnvCfg, RobotEnvCfg):
    env_type: str = RobotEnvCfg.model_fields["env_type"].default
    # ====== ENV CONFIGURATION ======
    odometry_type: Literal["NONE", "DUMMY", "ZED"] = "DUMMY"
    # zed_cfg: ZedOdometryCfg | None = ZedOdometryCfg(
    #     server_ip="127.0.0.1",
    #     pos_offset=[0.0, 0.0, 0.8],
    #     zero_align=True,
    # )

    # forward_kinematic: ForwardKinematicCfg | None = None
