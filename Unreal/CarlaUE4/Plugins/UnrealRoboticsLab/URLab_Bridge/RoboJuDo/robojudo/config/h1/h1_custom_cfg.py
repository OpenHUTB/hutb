from robojudo.config import cfg_registry
from robojudo.controller.ctrl_cfgs import (
    JoystickCtrlCfg,  # noqa: F401
    KeyboardCtrlCfg,  # noqa: F401
    UnitreeCtrlCfg,  # noqa: F401
)
from robojudo.pipeline.pipeline_cfgs import (
    RlMultiPolicyPipelineCfg,  # noqa: F401
    RlPipelineCfg,  # noqa: F401
)

from .ctrl.h1_motion_ctrl_cfg import H1MotionH2HCtrlCfg  # noqa: F401
from .env.h1_dummy_env_cfg import H1DummyEnvCfg  # noqa: F401
from .env.h1_mujuco_env_cfg import H1MujocoEnvCfg  # noqa: F401
from .env.h1_real_env_cfg import H1RealEnvCfg  # noqa: F401
from .policy.h1_h2h_policy_cfg import H1H2HPolicyCfg  # noqa: F401
from .policy.h1_smooth_policy_cfg import H1SmoothPolicyCfg  # noqa: F401
from .policy.h1_unitree_policy_cfg import H1UnitreePolicyCfg  # noqa: F401

# ======================== Custom Configs ======================== #
"""
Add your custom config here.
"""


@cfg_registry.register
class h1_dev(RlPipelineCfg):
    robot: str = "h1"
    env: H1MujocoEnvCfg = H1MujocoEnvCfg()

    ctrl: list[JoystickCtrlCfg | KeyboardCtrlCfg] = [
        JoystickCtrlCfg(),
        KeyboardCtrlCfg(),
    ]

    policy: H1SmoothPolicyCfg = H1SmoothPolicyCfg()
