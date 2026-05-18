# from .motion_lib import MotionLibReal
import logging
import os
import time
from queue import Queue
from threading import Lock, Thread, Timer

import numpy as np
import torch
from box import Box
from phc.utils.motion_lib_base import FixHeightMode
from phc.utils.motion_lib_real import MotionLibReal
from poselib.poselib.skeleton.skeleton3d import SkeletonTree

from robojudo.controller import Controller, ctrl_registry
from robojudo.controller.ctrl_cfgs import MotionCtrlCfg
from robojudo.controller.utils.motion_gui import MotionGUI
from robojudo.utils.util_func_torch import to_torch

logger = logging.getLogger(__name__)


# TODO: motion done flag
@ctrl_registry.register
class MotionCtrl(Controller):
    cfg_ctrl: MotionCtrlCfg

    def __init__(self, cfg_ctrl, env, device="cpu"):
        super().__init__(cfg_ctrl=cfg_ctrl, env=env, device=device)
        self.enable_gui = self.cfg_ctrl.motion_ctrl_gui
        self.motion_path = self.cfg_ctrl.motion_path

        assert os.path.exists(self.motion_path), f"Motion file {self.motion_path} not found!"

        # ========== PHC config ==========
        phc_robot_config = self.cfg_ctrl.phc.robot_config
        phc_robot_config["has_upright_start"] = False

        # ========== motion body keypoints ==========
        extend_config = phc_robot_config["extend_config"]

        self.motion_body_names = phc_robot_config["body_names"]
        motion_track_bodies_id = [
            self.motion_body_names.index(body_name) for body_name in self.cfg_ctrl.track_keypoints_names
        ]
        self.motion_track_bodies_extend_id = motion_track_bodies_id + list(
            range(len(self.motion_body_names), len(self.motion_body_names) + len(extend_config))
        )

        # ========== PHC motionlib ==========
        asset_file = phc_robot_config["asset"]["assetFileName"]
        self.skeleton_tree = SkeletonTree.from_mjcf(asset_file)

        motion_lib_cfg = Box(
            {
                "motion_file": self.motion_path,
                # "fix_height": FixHeightMode.full_fix,
                "fix_height": FixHeightMode.no_fix,
                "min_length": -1,
                "max_length": -1,
                "im_eval": False,
                "multi_thread": False,
                "smpl_type": phc_robot_config["humanoid_type"],
                "randomrize_heading": False,
                "device": self.device,
                "robot": phc_robot_config,
            }
        )

        self._motion_lib = MotionLibReal(motion_lib_cfg)
        self.ref_motion_cache = {}

        self.motion_time: float = 0
        self.motion_offset: np.ndarray = np.array([0.0, 0.0, 0.0])

        # ========== Play Control ==========
        self.motion_id = -1
        self.motion_name = "Blank Name"
        self.motion_length = 0.01

        self.motion_target_heading = np.array([0.0, 0.0, 0.0, 1.0])  # Default target heading

        self.play_speed_ratio = 1

        self.fade_step = 0.01
        self.fade_delay = 20  # ms
        self.speed_target = 1
        self.speed_steps = iter(np.arange(0, self.speed_target + self.fade_step, self.fade_step))

        if self.enable_gui:
            self.motion_gui = MotionGUI(self)
            self.play_speed_ratio = 0

        self.lock_motion_load = Lock()
        self.gui_commands = Queue()

        # init
        self.load_motion(0, block=True)

    def load_motion(self, motion_id=None, block=False):
        if (self.motion_id >= 0) and (motion_id == self.motion_id):
            logger.debug(f"[MotionCtrl] motion_id {motion_id} already loaded")
            return

        if self.lock_motion_load.locked():
            logger.debug(f"[MotionCtrl] motion_id {motion_id} already loading")
            return

        def load_motion_thread():
            motion_start_idx = motion_id if motion_id is not None else 0
            with self.lock_motion_load:
                self._motion_lib.load_motions(
                    skeleton_trees=[self.skeleton_tree],
                    gender_betas=[torch.zeros(17)],
                    limb_weights=[np.zeros(10)],
                    random_sample=False,
                    start_idx=motion_start_idx,
                    target_heading=self.motion_target_heading,
                )

                self.motion_id = motion_start_idx
                # self.motion_dt = self._motion_lib._motion_dt
                self.motion_name = self._motion_lib.curr_motion_keys
                self.motion_length = self._motion_lib._motion_lengths[0]
                self.reset()
                # TODO check auto reset

            if self.enable_gui:
                self.motion_gui.update_info(f"{self.motion_id}@{self.motion_name}", self.motion_length)

        if block:
            load_motion_thread()
        else:
            load_thread = Thread(target=load_motion_thread, daemon=True)
            load_thread.start()

    def get_motion(self, motion_ids=None, motion_times=None, offset=None):
        if self.lock_motion_load.locked():
            logger.debug("[MotionCtrl] use cache motion as loading lock")
            return self.ref_motion_cache
        # read motion
        # logger.debug(f"{self.motion_id=}, {self.motion_timestep=}")

        if motion_ids is None:
            motion_ids = to_torch([0], dtype=torch.int32)
        if motion_times is None:
            motion_times = to_torch([self.motion_time], dtype=torch.float32)
        if offset is None:
            offset = to_torch(self.motion_offset[np.newaxis, :], dtype=torch.float32)

        ## Cache the motion + offset
        if (
            offset is None
            or "motion_ids" not in self.ref_motion_cache
            or self.ref_motion_cache["offset"] is None
            or len(self.ref_motion_cache["offset"]) != len(offset)
            or (self.ref_motion_cache["motion_times"] - motion_times).abs().sum()
            + (self.ref_motion_cache["offset"] - offset).abs().sum()
            > 0
        ):
            self.ref_motion_cache["motion_ids"] = motion_ids.clone()  # need to clone; otherwise will be overriden
            self.ref_motion_cache["motion_times"] = motion_times.clone()  # need to clone; otherwise will be overriden
            self.ref_motion_cache["offset"] = offset.clone() if offset is not None else None
        else:
            return self.ref_motion_cache
        motion_res = self._motion_lib.get_motion_state(motion_ids, motion_times, offset=offset)

        self.ref_motion_cache.update(motion_res)

        return self.ref_motion_cache

    def get_data(self):
        motion_res = self.get_motion()
        ref_body_pos_extend = motion_res["rg_pos_t"].cpu().numpy().squeeze().copy()
        ref_body_vel_extend = motion_res["body_vel_t"].cpu().numpy().squeeze().copy() * self.play_speed_ratio
        ref_body_pos_subset = ref_body_pos_extend[self.motion_track_bodies_extend_id]
        ref_body_vel_subset = ref_body_vel_extend[self.motion_track_bodies_extend_id]

        ctrl_data = {
            "ref_body_pos_subset": ref_body_pos_subset,
            "ref_body_vel_subset": ref_body_vel_subset,
            "dof_pos": motion_res["dof_pos"].cpu().numpy().squeeze().copy(),
        }

        if (hand_pose := motion_res.get("hand_pose", None)) is not None:
            ctrl_data["hand_pose"] = hand_pose.cpu().numpy().squeeze().copy().reshape(2, -1)

        return ctrl_data

    def process_triggers(self, ctrl_data):
        commands = []
        while not self.gui_commands.empty():
            command = self.gui_commands.get()
            if command not in commands:
                commands.append(command)

        return ctrl_data, commands

    def post_step_callback(self, commands=None, motion_time_step=0.02):
        if commands is None:
            commands = []

        self.motion_time += motion_time_step * self.play_speed_ratio

        if self.enable_gui:
            self.motion_gui.update_time(self.motion_time)

        for command in commands:
            match command:
                case "[MOTION_FADE_IN]":
                    self.fade_in()
                case "[MOTION_FADE_OUT]":
                    self.fade_out()
                case "[MOTION_RESET]":
                    self.reset()
                case "[MOTION_LOAD_NEXT]":
                    self.load_motion(self.motion_id + 1, block=False)
                case "[MOTION_LOAD_PREV]":
                    if self.motion_id > 0:
                        self.load_motion(self.motion_id - 1, block=False)

    # ========== Play Control ==============
    def reset(self):
        self.motion_time = 0

        motion_init_pos = self._motion_lib.get_root_pos_smpl([0], to_torch([0]))["root_pos"][0].cpu().numpy()
        motion_init_pos[2] = 0.0
        self.motion_offset = -motion_init_pos
        # logger.debug(f"{self.motion_offset=}")

    def _fade_step_apply(self):
        try:
            speed_step = float(next(self.speed_steps))
            speed_step = max(speed_step, 0)
            self.play_speed_ratio = speed_step

            Timer(self.fade_delay / 1000.0, self._fade_step_apply).start()
        except StopIteration:
            logger.info("Fade-step complete")

    def fade_in(self):
        self.speed_steps = iter(np.arange(self.play_speed_ratio, self.speed_target + self.fade_step, self.fade_step))
        self._fade_step_apply()

    def fade_out(self):
        self.speed_steps = iter(np.arange(self.play_speed_ratio, 0 - self.fade_step, -self.fade_step))
        self._fade_step_apply()


if __name__ == "__main__":
    from pprint import pprint

    from robojudo.config.g1.ctrl.g1_motion_ctrl_cfg import G1MotionCtrlCfg

    motion_ctrl = MotionCtrl(
        cfg_ctrl=G1MotionCtrlCfg(motion_name="singles/0-KIT_6_WalkInCounterClockwiseCircle05_1_poses"),
        env=None,
    )

    while True:
        ctrl_data = motion_ctrl.get_data()
        _, commands = motion_ctrl.process_triggers(ctrl_data)
        pprint(ctrl_data)
        motion_ctrl.post_step_callback(commands)
        pprint(commands)
        time.sleep(0.1)
