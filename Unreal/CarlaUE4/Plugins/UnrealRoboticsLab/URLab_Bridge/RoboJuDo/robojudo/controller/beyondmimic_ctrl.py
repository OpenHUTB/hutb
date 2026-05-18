from __future__ import annotations

import os
from collections.abc import Sequence

import numpy as np

from robojudo.controller import Controller, ctrl_registry
from robojudo.controller.ctrl_cfgs import BeyondMimicCtrlCfg
from robojudo.environment import Environment
from robojudo.utils.progress import ProgressBar
from robojudo.utils.rotation import TransformAlignment


# From BeyondMimic
class MotionLoader:
    def __init__(self, motion_file: str, body_indexes: Sequence[int], device: str = "cpu"):
        assert os.path.isfile(motion_file), f"Invalid file path: {motion_file}"
        data = np.load(motion_file)
        self.fps = data["fps"]
        self.joint_pos = data["joint_pos"]
        self.joint_vel = data["joint_vel"]
        self._body_pos_w = data["body_pos_w"]
        self._body_quat_w = data["body_quat_w"]
        self._body_lin_vel_w = data["body_lin_vel_w"]
        self._body_ang_vel_w = data["body_ang_vel_w"]
        self._body_indexes = body_indexes
        self.hand_pose = data.get("hand_pose", None)
        # self.hand_pose = np.zeros((self.joint_pos.shape[0], 2, 6))  # TODO: dummy for now
        self.time_step_total = self.joint_pos.shape[0]

    @property
    def body_pos_w(self) -> np.ndarray:
        return self._body_pos_w[:, self._body_indexes]

    @property
    def body_quat_w(self) -> np.ndarray:
        return self._body_quat_w[:, self._body_indexes]

    @property
    def body_lin_vel_w(self) -> np.ndarray:
        return self._body_lin_vel_w[:, self._body_indexes]

    @property
    def body_ang_vel_w(self) -> np.ndarray:
        return self._body_ang_vel_w[:, self._body_indexes]


@ctrl_registry.register
class BeyondMimicCtrl(Controller):
    cfg_ctrl: BeyondMimicCtrlCfg
    env: Environment

    def __init__(self, cfg_ctrl: BeyondMimicCtrlCfg, env, device="cpu"):
        super().__init__(cfg_ctrl=cfg_ctrl, env=env, device=device)
        assert self.env is not None, "Env is required for BeyondMimicCtrl"
        self.override_robot_anchor_pos = self.cfg_ctrl.override_robot_anchor_pos

        motion_file = self.cfg_ctrl.motion_path
        motion_cfg = self.cfg_ctrl.motion_cfg
        body_indexes = [motion_cfg.body_names_all.index(name) for name in motion_cfg.body_names]
        self.motion_anchor_body_index = motion_cfg.body_names.index(motion_cfg.anchor_body_name)

        self.motion = MotionLoader(motion_file, body_indexes, device="cpu")
        self.timestep = 0
        self.playing = False

        self.motion_init_align = TransformAlignment(yaw_only=True, xy_only=True)
        self.reset()

    @property
    def command(self) -> np.ndarray:
        return np.concatenate([self.joint_pos, self.joint_vel], axis=-1)

    @property
    def joint_pos(self) -> np.ndarray:
        return self.motion.joint_pos[self.timestep].copy()

    @property
    def joint_vel(self) -> np.ndarray:
        return self.motion.joint_vel[self.timestep].copy()

    @property
    def anchor_pos_w(self) -> np.ndarray:
        anchor_pos_w_raw = self.motion.body_pos_w[self.timestep, self.motion_anchor_body_index].copy()
        anchor_pos_w = self.motion_init_align.align_pos(anchor_pos_w_raw)
        return anchor_pos_w

    @property
    def anchor_quat_w(self) -> np.ndarray:
        anchor_quat_w_raw = self.motion.body_quat_w[self.timestep, self.motion_anchor_body_index].copy()[[1, 2, 3, 0]]
        return self.motion_init_align.align_quat(anchor_quat_w_raw)

    @property
    def robot_anchor_pos_w(self) -> np.ndarray:
        if self.override_robot_anchor_pos:  # OVERRIDE
            return self.anchor_pos_w
        else:
            base_pos = self.env.torso_pos
            assert base_pos is not None
            return base_pos

    @property
    def robot_anchor_quat_w(self) -> np.ndarray:
        torso_quat = self.env.torso_quat
        assert torso_quat is not None
        return torso_quat

    @property
    def hand_pose(self) -> np.ndarray | None:
        if self.motion.hand_pose is not None:
            hand_pose = self.motion.hand_pose[self.timestep].copy()
            if len(hand_pose.shape) == 1:
                hand_dim = hand_pose.shape[0] // 2
                hand_pose = hand_pose.reshape(2, hand_dim)
            return hand_pose
        else:
            return None

    def reset(self):
        self.timestep = 0
        self.pbar = ProgressBar(f"BeyondmimicCtrl {self.cfg_ctrl.motion_name}", self.motion.time_step_total)

        # align the robot to the motion's starting pose
        init2anchor_pos = self.motion.body_pos_w[0, self.motion_anchor_body_index].copy()
        init2anchor_quat = self.motion.body_quat_w[0, self.motion_anchor_body_index].copy()[[1, 2, 3, 0]]
        # keep yaw only
        self.motion_init_align.set_base(quat=init2anchor_quat, pos=init2anchor_pos)

    def post_step_callback(self, commands: list[str] | None = None):
        self.pbar.set(self.timestep)
        if self.timestep < self.motion.time_step_total - 1:
            if self.playing:
                self.timestep += 1

        for command in commands or []:
            match command:
                case "[MOTION_RESET]":
                    self.reset()
                case "[MOTION_FADE_IN]":
                    self.playing = True
                case "[MOTION_FADE_OUT]":
                    self.playing = False

    def get_data(self):
        ctrl_data = {
            "command": self.command,
            "joint_pos": self.joint_pos,
            # "joint_vel": self.joint_vel,
            "robot_anchor_pos_w": self.robot_anchor_pos_w,
            "robot_anchor_quat_w": self.robot_anchor_quat_w,
            "anchor_pos_w": self.anchor_pos_w,
            "anchor_quat_w": self.anchor_quat_w,
            "timestep": self.timestep,
            "hand_pose": self.hand_pose,
        }
        return ctrl_data


if __name__ == "__main__":
    # Example usage
    from robojudo.config.g1.ctrl.g1_beyondmimic_ctrl_cfg import G1BeyondmimicCtrlCfg
    from robojudo.config.g1.env.g1_mujuco_env_cfg import G1MujocoEnvCfg
    from robojudo.environment.mujoco_env import MujocoEnv

    env = MujocoEnv(cfg_env=G1MujocoEnvCfg())
    ctrl = BeyondMimicCtrl(cfg_ctrl=G1BeyondmimicCtrlCfg(), env=env)
    print(ctrl.get_data())  # This will print the command tensor
