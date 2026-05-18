"""ProtoMotions tracker policy for RoboJuDo.

Runs a unified ONNX model exported by
``deployment/export_bm_tracker_onnx.py`` with cached 50 fps motion from
``deployment/motion_utils.MotionPlayer``.

Key inputs:

- ``historical.processed_actions`` — action history feedback (previous PD
  targets are fed back as an ONNX input)
- ``mimic.future_anchor_rot`` — anchor-body-only rotation references

Heading alignment
-----------------
Yaw-only offset computed on first step to align motion heading with robot heading.

Sensor requirements (real G1)
-----------------------------
- ``env_data.dof_pos`` / ``env_data.dof_vel`` -- joint encoders
- ``env_data.base_quat`` (xyzw) -- pelvis IMU
- ``env_data.base_ang_vel`` -- pelvis IMU gyro (body-local frame)
- ``env_data.torso_quat`` (xyzw) -- FK-computed (requires ``update_with_fk=True``)
"""

import importlib
import logging
import sys
from pathlib import Path

import numpy as np
import onnxruntime as ort
import yaml

from robojudo.policy import Policy, policy_registry
from robojudo.policy.policy_cfgs import PolicyCfg
from robojudo.tools.tool_cfgs import DoFConfig

logger = logging.getLogger(__name__)

# Add the protomotions repo root to sys.path so deployment.* is importable.
# robojudo/robojudo/policy/this_file.py -> parents[2] = robojudo repo root
# protomotions is expected as a sibling: ../protomotions
_PROTO_ROOT = str(Path(__file__).resolve().parents[2].parent / "protomotions")
if _PROTO_ROOT not in sys.path:
    sys.path.insert(0, _PROTO_ROOT)


try:
    importlib.import_module("deployment")
except ModuleNotFoundError:
    logger.error(
        "Missing ProtoMotions source repository: cannot import 'deployment'. "
        "Please clone the original 'protomotions' repo as a sibling directory "
        f"next to this repo (expected path: {_PROTO_ROOT})."
    )
    raise RuntimeError("Cannot import 'deployment' from ProtoMotions repo!") from None

from deployment.motion_utils import MotionPlayer  # noqa: E402
from deployment.state_utils import (  # noqa: E402
    apply_heading_offset_np,
    compute_yaw_offset_np,
)


@policy_registry.register
class ProtoMotionsTrackerPolicy(Policy):
    """Policy that drives a ProtoMotions tracker via unified ONNX model.

    The ONNX model bakes in: obs computation -> actor MLP -> action processing.
    Inputs are raw context tensors; outputs are absolute PD position targets.
    """

    cfg_policy: PolicyCfg

    def __init__(self, cfg_policy: PolicyCfg, device: str = "cpu"):
        # Load YAML metadata BEFORE calling super().__init__ so we can
        # build the DOF config from it.
        onnx_path = cfg_policy.policy_file
        yaml_path = onnx_path.replace(".onnx", ".yaml")

        with open(yaml_path) as f:
            self._meta = yaml.safe_load(f)

        robot_meta = self._meta["robot"]
        control_meta = self._meta["control"]
        motion_meta = self._meta["motion"]
        runtime = self._meta["_runtime"]

        joint_names = robot_meta["joint_names"]
        num_dofs = robot_meta["num_dofs"]
        stiffness = control_meta["stiffness"]
        damping = control_meta["damping"]
        effort_limits = control_meta.get("effort_limits")

        # Build DOF config from YAML metadata.
        dof_cfg = DoFConfig(
            joint_names=joint_names,
            default_pos=[0.0] * num_dofs,
            stiffness=stiffness,
            damping=damping,
            torque_limits=effort_limits,
        )
        cfg_policy_updated = cfg_policy.model_copy()
        cfg_policy_updated.obs_dof = dof_cfg
        cfg_policy_updated.action_dof = dof_cfg

        super().__init__(cfg_policy=cfg_policy_updated, device="cpu")

        # ONNX session
        logger.info(f"[TrackerPolicy] Loading ONNX: {onnx_path}")
        self._session = ort.InferenceSession(
            onnx_path, providers=["CPUExecutionProvider"]
        )
        self._onnx_in_names = [inp.name for inp in self._session.get_inputs()]
        self._onnx_out_names = [out.name for out in self._session.get_outputs()]
        self._onnx_name_to_key = runtime["onnx_name_to_in_key"]

        # Motion player (cached mode -- no protomotions import)
        motion_path = getattr(cfg_policy, "motion_path", None)
        if motion_path is None:
            raise ValueError("ProtoMotionsTrackerPolicyCfg must set motion_path")
        motion_index = getattr(cfg_policy, "motion_index", 0)
        timing = self._meta["timing"]
        self._player = MotionPlayer(
            motion_path, motion_index=motion_index, control_dt=timing["control_dt"]
        )

        # ONNX input config
        self._anchor_idx = robot_meta["anchor_body_index"]
        self._root_idx = robot_meta["root_body_index"]
        self._future_step_indices = motion_meta["future_step_indices"]

        # Determine how to read the anchor body rotation from env_data.
        # Use body name to look up in fk_info; pelvis uses base_quat directly.
        self._anchor_body_name = robot_meta.get("anchor_body_name")
        logger.info(
            f"[TrackerPolicy] anchor body: "
            f"{self._anchor_body_name or 'pelvis'} (idx={self._anchor_idx})"
        )

        # Action post-processing config
        self._pd_target_max_accel = control_meta.get("pd_target_max_accel")
        self._action_ema_alpha = control_meta.get("action_ema_alpha", 1.0)

        logger.info(
            f"[TrackerPolicy] {num_dofs} DOFs, "
            f"{self._player.total_frames} motion frames, "
            f"anchor_idx={self._anchor_idx}, root_idx={self._root_idx}"
        )

        self._heading_offset = None
        self.reset()

    def reset(self):
        self._frame = 0
        self._prev_pd = None
        self._prev_prev_pd = None
        self._ema_prev = None
        self._stashed_pd_targets = np.zeros(self.num_actions, dtype=np.float32)
        self._prev_actions = np.zeros(self.num_actions, dtype=np.float32)
        self._motion_done = False
        self._paused = False

    def reset_alignment(self):
        self._heading_offset = None

    def post_step_callback(self, commands=None):
        if not self._paused:
            self._frame += 1
            if self._frame >= self._player.total_frames:
                self._frame = self._player.total_frames - 1
                self._motion_done = True
        for cmd in commands or []:
            if cmd in ("[MOTION_RESET]", "[MOTION_FADE_IN]"):
                self.reset()

    def get_observation(self, env_data, ctrl_data):
        # -- Heading alignment (first step after reset) --
        if self._heading_offset is None:
            motion_anchor_rot = self._player.get_state_at_frame(0)["body_rot"][
                self._anchor_idx
            ]
            robot_anchor_rot = self._get_anchor_quat(env_data)
            self._heading_offset = compute_yaw_offset_np(
                robot_anchor_rot, motion_anchor_rot
            )

        # -- State from env_data (already xyzw) --
        anchor_rot = self._get_anchor_quat(env_data)
        dof_pos = np.asarray(env_data.dof_pos, dtype=np.float32)
        dof_vel = np.asarray(env_data.dof_vel, dtype=np.float32)
        # env_data.base_ang_vel comes from MuJoCo qvel[3:6] which is ALREADY
        # in the pelvis local frame (not world frame).  On the real G1, the
        # IMU gyroscope also reads in body-local frame.  So we use it directly
        # as root_local_ang_vel -- NO quat_rotate_inverse needed.
        root_local_ang_vel = np.asarray(env_data.base_ang_vel, dtype=np.float32)

        # -- Future motion references with heading alignment --
        # Clamp each future step so it never exceeds the last valid frame.
        # This repeats the last frame's references at end-of-motion instead
        # of going out of bounds.
        last_frame = self._player.total_frames - 1
        clamped_steps = [
            min(self._frame + step, last_frame) - self._frame
            for step in self._future_step_indices
        ]
        future_refs = self._player.get_future_references(
            self._frame, clamped_steps
        )
        future_body_rot = apply_heading_offset_np(
            self._heading_offset, future_refs["body_rot"]
        )
        # Anchor-body-only rotation: [num_steps, 4]
        future_anchor_rot = future_body_rot[:, self._anchor_idx, :]

        # -- Build ONNX inputs --
        key_to_array = {
            "current.dof_pos": dof_pos[None],
            "current.dof_vel": dof_vel[None],
            "current.anchor_rot": anchor_rot[None],
            "current.root_local_ang_vel": root_local_ang_vel[None],
            "mimic.future_anchor_rot": future_anchor_rot[None],
            "mimic.future_dof_pos": future_refs["dof_pos"][None],
            "mimic.future_dof_vel": future_refs["dof_vel"][None],
            "historical.processed_actions": self._prev_actions[None, None],
        }
        onnx_inputs = {}
        for onnx_name in self._onnx_in_names:
            sem_key = self._onnx_name_to_key.get(onnx_name)
            if sem_key and sem_key in key_to_array:
                onnx_inputs[onnx_name] = key_to_array[sem_key].astype(np.float32)

        # -- ONNX inference --
        ort_out = self._session.run(self._onnx_out_names, onnx_inputs)
        pd_targets = ort_out[1].squeeze().copy()

        # -- PD target acceleration clamp --
        if (
            self._pd_target_max_accel is not None
            and self._prev_pd is not None
            and self._prev_prev_pd is not None
        ):
            delta = pd_targets - self._prev_pd
            prev_delta = self._prev_pd - self._prev_prev_pd
            accel = delta - prev_delta
            clamped_accel = np.clip(
                accel, -self._pd_target_max_accel, self._pd_target_max_accel
            )
            pd_targets = self._prev_pd + prev_delta + clamped_accel
        self._prev_prev_pd = self._prev_pd
        self._prev_pd = pd_targets.copy()

        # -- EMA action filter --
        alpha = self._action_ema_alpha
        if alpha < 1.0:
            if self._ema_prev is None:
                self._ema_prev = pd_targets.copy()
            pd_targets = alpha * pd_targets + (1.0 - alpha) * self._ema_prev
            self._ema_prev = pd_targets.copy()

        self._stashed_pd_targets = pd_targets
        self._prev_actions = pd_targets.copy()
        extras = {
            "CALLBACK": ["[MOTION_DONE]"] if self._motion_done else [],
        }
        dummy_obs = np.zeros(1, dtype=np.float32)
        return dummy_obs, extras

    def _get_anchor_quat(self, env_data) -> np.ndarray:
        """Read the anchor body's quaternion from env_data.

        Uses the body name from YAML metadata to look up in fk_info.
        Falls back to base_quat for pelvis (root body).
        """
        name = self._anchor_body_name
        if name is not None and name not in (None, "pelvis"):
            # Named body -- look up in FK info
            fk = env_data.fk_info
            if fk is not None and name in fk:
                return np.asarray(fk[name]["quat"], dtype=np.float32)
            # Fallback: if the env exposes it as torso_quat and name matches
            if name == "torso_link" and env_data.torso_quat is not None:
                return np.asarray(env_data.torso_quat, dtype=np.float32)
        # Pelvis / root body -- always available as base_quat
        return np.asarray(env_data.base_quat, dtype=np.float32)

    def get_action(self, obs):
        return self._stashed_pd_targets

    def get_init_dof_pos(self):
        return self._player.get_state_at_frame(0)["dof_pos"].copy()
