import logging
import os

import numpy as np
import onnxruntime as ort

from robojudo.environment.utils.mujoco_viz import MujocoVisualizer
from robojudo.policy import Policy, policy_registry
from robojudo.policy.asap_policy import AsapPolicy
from robojudo.policy.policy_cfgs import KungfuBotGeneralPolicyCfg
from robojudo.utils.util_func import (
    matrix_from_quat,
    quatToEuler,
    subtract_frame_transforms,
)

logger = logging.getLogger(__name__)

KungfuBotPolicy = AsapPolicy  # backward compatibility


@policy_registry.register
class KungfuBotGeneralPolicy(Policy):
    """From [KungfuBot2-PBHC](https://github.com/TeleHuman/PBHC)"""

    cfg_policy: KungfuBotGeneralPolicyCfg

    def __init__(self, cfg_policy: KungfuBotGeneralPolicyCfg, device):
        if not os.path.isfile(cfg_policy.policy_file):
            raise FileNotFoundError(f"Model file not found at {cfg_policy.policy_file}")

        logger.debug(f"Loading policy '{cfg_policy.policy_name}' from {cfg_policy.policy_file}")
        self.session = ort.InferenceSession(cfg_policy.policy_file)

        self.input_names = [i.name for i in self.session.get_inputs()]
        self.output_names = [o.name for o in self.session.get_outputs()]
        # TODO: move to onnx policy

        super().__init__(cfg_policy=cfg_policy, device=device)

        self.obs_scales = cfg_policy.obs_scales
        self.action_scales = np.asarray(self.cfg_policy.action_scales)
        self.compatibility_old_version = cfg_policy.compatibility_old_version

        self.reset()

    def reset(self):
        self.last_action = np.zeros(self.num_actions)

        self.history_obs_dims = self.cfg_policy.history_obs_dims
        default_history = [np.zeros(dim, dtype=np.float32) for dim in self.history_obs_dims.values()]
        self._init_history(default_history)

    def post_step_callback(self, commands: list[str] | None = None):
        pass

    def _get_obs_history(self):
        history_list = [np.concatenate(items, axis=0) for items in zip(*self.history_buf, strict=True)]
        return np.concatenate(history_list, axis=0)

    def _get_commands(self, ctrl_data):
        target_keys = ["MotionKungfuBotCtrl"]
        motion_res = next((ctrl_data[k] for k in target_keys if k in ctrl_data), None)
        if motion_res is None:
            raise KeyError("No matching motion controller found.")

        future_motion_root_height = motion_res["future_motion_root_height"].copy()
        future_motion_roll_pitch = motion_res["future_motion_roll_pitch"].copy()
        future_motion_base_lin_vel = motion_res["future_motion_base_lin_vel"].copy()
        future_motion_base_yaw_vel = motion_res["future_motion_base_yaw_vel"].copy()
        future_motion_dof_pos = motion_res["future_motion_dof_pos"].copy()
        next_step_ref_motion = motion_res["next_step_ref_motion"].copy()
        ref_frame_anchor = motion_res["ref_frame_anchor"]

        if "hand_pose" in motion_res:
            ref_hand_pose = motion_res["hand_pose"].copy()
        else:
            ref_hand_pose = None

        return (
            future_motion_root_height,
            future_motion_roll_pitch,
            future_motion_base_lin_vel,
            future_motion_base_yaw_vel,
            future_motion_dof_pos,
            next_step_ref_motion,
            ref_frame_anchor,
            ref_hand_pose,
        )

    def get_observation(self, env_data, ctrl_data):  # pyright: ignore[reportIncompatibleMethodOverride] # TODO
        (
            future_motion_root_height,
            future_motion_roll_pitch,
            future_motion_base_lin_vel,
            future_motion_base_yaw_vel,
            future_motion_dof_pos,
            next_step_ref_motion,
            ref_frame_anchor,
            ref_hand_pose,
        ) = self._get_commands(ctrl_data)

        base_quat = env_data.base_quat
        base_ang_vel = env_data.base_ang_vel
        dof_pos = env_data.dof_pos
        dof_vel = env_data.dof_vel

        rpy = quatToEuler(base_quat)
        roll_pitch = rpy[:2]

        dof_pos_minus_default = dof_pos - self.default_dof_pos
        action = self.last_action.copy()

        ref_frame_anchor_pos, ref_frame_anchor_rot = ref_frame_anchor

        _, ori = subtract_frame_transforms(
            np.zeros(3),  # no robot base_pos
            base_quat,
            ref_frame_anchor_pos,
            ref_frame_anchor_rot,
        )
        if self.compatibility_old_version:
            ori = ori[[1, 2, 3, 0]]  # WARN: this is a bug from PBHC repo, https://github.com/TeleHuman/PBHC/issues/68
        mat = matrix_from_quat(ori)
        anchor_ref_rot = mat[:, :2].flatten()

        history = self._get_obs_history()

        # Note: this should be aligned with history_obs_dims
        # IMPORTANT: the order should be SORTED by obs name!!!
        obs_a = [
            action * self.obs_scales.actions,
            base_ang_vel * self.obs_scales.base_ang_vel,
            dof_pos * self.obs_scales.dof_pos,
            dof_vel * self.obs_scales.dof_vel,
            roll_pitch * self.obs_scales.roll_pitch,
        ]
        self.history_buf.appendleft(obs_a)

        # IMPORTANT: the order should be SORTED by obs name!!!
        actor_obs = np.concatenate(
            [
                action * self.obs_scales.actions,
                anchor_ref_rot * self.obs_scales.anchor_ref_rot,
                base_ang_vel * self.obs_scales.base_ang_vel,
                dof_pos_minus_default * self.obs_scales.dof_pos,
                dof_vel * self.obs_scales.dof_vel,
                history * self.obs_scales.history,
                next_step_ref_motion * self.obs_scales.next_step_ref_motion,
                roll_pitch * self.obs_scales.roll_pitch,
            ],
            axis=0,
        )

        # IMPORTANT: the order should be SORTED by obs name!!!
        future_motion_targets = np.concatenate(
            [
                future_motion_base_lin_vel * self.obs_scales.future_motion_base_lin_vel,
                future_motion_base_yaw_vel * self.obs_scales.future_motion_base_yaw_vel,
                future_motion_dof_pos * self.obs_scales.future_motion_dof_pos,
                future_motion_roll_pitch * self.obs_scales.future_motion_roll_pitch,
                future_motion_root_height * self.obs_scales.future_motion_root_height,
            ],
            axis=0,
        )
        prop_history = np.concatenate([history], axis=0)

        obs = [actor_obs, future_motion_targets, prop_history]

        extras = {
            "robot_anchor_pos_w": env_data.base_pos,
            "robot_anchor_quat_w": env_data.base_quat,
            "ref_frame_anchor_pos": ref_frame_anchor_pos,
            "ref_frame_anchor_rot": ref_frame_anchor_rot,
            "ori": ori,
        }
        return obs, extras

    def get_action(self, obs: np.ndarray) -> np.ndarray:
        ort_inputs = {
            "actor_obs": np.expand_dims(obs[0], axis=0).astype(np.float32),
            "future_motion_targets": np.expand_dims(obs[1], axis=0).astype(np.float32),
            "prop_history": np.expand_dims(obs[2], axis=0).astype(np.float32),
        }

        ort_outputs = self.session.run(
            ["action"],
            ort_inputs,
        )
        actions: np.ndarray = np.asarray(ort_outputs[0]).squeeze()

        self.last_action = actions.copy()  # TODO: check all policies after process

        processed_actions = actions
        if self.action_clip is not None:
            processed_actions = np.clip(processed_actions, -self.action_clip, self.action_clip)

        processed_actions = processed_actions * self.action_scales
        return processed_actions

    def get_init_dof_pos(self) -> np.ndarray:
        """
        Return first frame of the reference motion.
        """
        return self.default_dof_pos.copy()

    def debug_viz(self, visualizer: MujocoVisualizer, env_data, ctrl_data, extras):
        robot_anchor_pos_w = extras["robot_anchor_pos_w"]
        robot_anchor_quat_w = extras["robot_anchor_quat_w"]
        anchor_pos_w = extras["ref_frame_anchor_pos"]
        anchor_quat_w = extras["ref_frame_anchor_rot"]

        ori = extras["ori"]

        visualizer.draw_arrow(anchor_pos_w, anchor_quat_w, [0.2, 0, 0], color=[1, 0, 0, 1], scale=2, id=0)
        visualizer.draw_arrow(
            robot_anchor_pos_w,
            robot_anchor_quat_w,
            [0.2, 0, 0],
            color=[0, 1, 0, 1],
            scale=2,
            id=1,
        )
        visualizer.draw_arrow(robot_anchor_pos_w, ori, [0.5, 0, 0], color=[0, 1, 0, 1], scale=2, id=3)
