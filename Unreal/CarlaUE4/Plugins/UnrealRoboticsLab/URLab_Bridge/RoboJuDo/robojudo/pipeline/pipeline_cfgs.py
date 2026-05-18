from typing import Any

from pydantic import model_validator

from robojudo.config import Config
from robojudo.controller import CtrlCfg
from robojudo.environment import EnvCfg
from robojudo.policy import PolicyCfg
from robojudo.tools.debug_log import DebugCfg


class PipelineCfg(Config):
    pipeline_type: str  # name of the pipeline class
    # ===== Pipeline Config =====
    device: str = "cpu"

    debug: DebugCfg = DebugCfg()

    run_fullspeed: bool = False
    """If True, run the pipeline at full speed, ignoring the desired frequency"""

    do_safety_check: bool = False
    """
    If True, perform safety check after each step.
    We recommend enabling this, however if motion is very aggressive, you may disable it.
    """


class RlPipelineCfg(PipelineCfg):
    pipeline_type: str = "RlPipeline"

    # ===== Pipeline Config =====
    robot: str  # robot name, e.g. "g1"

    env: EnvCfg | Any
    ctrl: list[CtrlCfg | Any] = []
    policy: PolicyCfg | Any


class RlMultiPolicyPipelineCfg(PipelineCfg):
    pipeline_type: str = "RlMultiPolicyPipeline"

    # ===== Pipeline Config =====
    robot: str  # robot name, e.g. "g1"

    env: EnvCfg | Any
    ctrl: list[CtrlCfg | Any] = []

    policies: list[PolicyCfg | Any] = []
    """First policy as init, rest as extra policies, can be switched to"""


class RlLocoMimicPipelineCfg(PipelineCfg):
    pipeline_type: str = "RlLocoMimicPipeline"

    # ===== Pipeline Config =====
    robot: str  # robot name, e.g. "g1"

    env: EnvCfg | Any
    ctrl: list[CtrlCfg | Any] = []

    loco_policy: PolicyCfg | Any
    """LocoMotion policy, as init"""
    mimic_policies: list[PolicyCfg | Any] = []
    """MotionMimic policies, can be switched to"""

    # ===== Upper body override Config =====
    upper_dof_num: int = 0
    upper_dof_pos_default: list[float] | None = []
    """Default positions of the upper body DOFs"""
    upper_dof_override_indices: list[int] | None = []
    """Indices of the upper body DOFs to be overridden"""

    @model_validator(mode="after")
    def check_upper_dof(self):
        if self.upper_dof_pos_default is not None:
            if len(self.upper_dof_pos_default) != self.upper_dof_num:
                raise ValueError(
                    f"Length of upper_dof_pos_default ({len(self.upper_dof_pos_default)}) "
                    f"must be equal to upper_dof_num ({self.upper_dof_num})"
                )
        if self.upper_dof_override_indices is not None:
            for idx in self.upper_dof_override_indices:
                if idx < -self.upper_dof_num or idx >= 0:
                    raise ValueError(
                        f"upper_dof_override_indices contains invalid index {idx}, "
                        f"must be in [-{self.upper_dof_num}, 0)"
                    )

        return self
