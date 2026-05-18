"""Configuration for ProtoMotions tracker policy.

DOF config (joint names, stiffness, damping) is loaded from the ONNX YAML
metadata at policy init time.  ``default_pos`` is set to zeros because the
ONNX model outputs absolute PD targets.
"""

from robojudo.config import ASSETS_DIR
from robojudo.policy.policy_cfgs import PolicyCfg
from robojudo.tools.tool_cfgs import DoFConfig


class ProtoMotionsTrackerPolicyCfg(PolicyCfg):
    """Config for :class:`ProtoMotionsTrackerPolicy`.

    ``obs_dof`` and ``action_dof`` are placeholder DOF configs here.
    The real values are loaded from the ONNX YAML metadata at runtime and
    injected into the policy's ``cfg_policy`` before ``super().__init__``.
    """

    policy_type: str = "ProtoMotionsTrackerPolicy"
    robot: str = "g1"
    disable_autoload: bool = True

    # Paths -- override these in the g1_cfg entry or via CLI.
    # If onnx_path is set (absolute), it takes precedence over onnx_name.
    onnx_name: str = "unified_pipeline"
    onnx_path: str | None = None
    motion_path: str = ""
    motion_index: int = 0
    """Index of the motion clip within a multi-motion .pt library."""

    @property
    def policy_file(self) -> str:
        if self.onnx_path is not None:
            return self.onnx_path
        return (ASSETS_DIR / f"models/{self.robot}/protomotions_tracker/{self.onnx_name}.onnx").as_posix()

    # Disable base-class action post-processing (we do it ourselves)
    action_scale: float = 1.0
    action_clip: float | None = None
    action_beta: float = 1.0

    # Placeholder DOF config (overridden from YAML metadata at runtime)
    obs_dof: DoFConfig = DoFConfig(joint_names=["placeholder"], default_pos=[0.0])
    action_dof: DoFConfig = DoFConfig(joint_names=["placeholder"], default_pos=[0.0])
