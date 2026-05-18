# from .motion_lib import MotionLibReal
import logging
import time

import numpy as np

from robojudo.controller import ctrl_registry
from robojudo.controller.ctrl_cfgs import MotionH2HCtrlCfg
from robojudo.controller.motion_ctrl import MotionCtrl
from robojudo.environment import Environment
from robojudo.utils.util_func import my_quat_rotate_np

logger = logging.getLogger(__name__)


@ctrl_registry.register
class MotionH2HCtrl(MotionCtrl):
    cfg_ctrl: MotionH2HCtrlCfg
    env: Environment

    def __init__(self, cfg_ctrl, env, device="cpu"):
        super().__init__(cfg_ctrl=cfg_ctrl, env=env, device=device)
        assert self.env is not None, "Env is required for MotionCtrl"
        self.extra_motion_data = self.cfg_ctrl.extra_motion_data

        # ========== PHC config ==========
        phc_robot_config = self.cfg_ctrl.phc.robot_config
        extend_config = phc_robot_config["extend_config"]

        # ========== robot body keypoints ==========
        assert self.env.kinematics is not None, "Env Kinematics model is required for MotionCtrl"
        robot_body_names = self.env.kinematics.body_names

        robot_track_bodies_id = [robot_body_names.index(body_name) for body_name in self.cfg_ctrl.track_keypoints_names]

        self.extend_body_parent_names = []
        self.extend_body_parent_ids = []
        extend_body_pos_list = []
        for cfg in extend_config:
            parent_name, pos = cfg["parent_name"], cfg["pos"]
            extend_body_parent_id = robot_body_names.index(parent_name)
            self.extend_body_parent_names.append(parent_name)
            self.extend_body_parent_ids.append(extend_body_parent_id)
            extend_body_pos_list.append(pos)
        self.extend_body_pos = np.asarray(extend_body_pos_list).reshape(-1, 3)

        self.robot_track_bodies_extend_id = robot_track_bodies_id + list(
            range(len(robot_body_names), len(robot_body_names) + len(extend_config))
        )

        self.play_speed_ratio = 0.0
        self.speed_target = 1 / 2

    def get_robot_state(self):
        fk_info = self.env.fk_info
        assert fk_info is not None, "Env fk_info is required for MotionCtrl"

        body_pos = np.array([body_info["pos"] for body_info in fk_info.values()])
        body_rot = np.array([body_info["quat"] for body_info in fk_info.values()])

        extend_curr_pos = (
            my_quat_rotate_np(
                body_rot[self.extend_body_parent_ids].reshape(-1, 4), self.extend_body_pos.reshape(-1, 3)
            ).reshape(-1, 3)
            + body_pos[self.extend_body_parent_ids]
        )
        body_pos_extend = np.concatenate([body_pos, extend_curr_pos], axis=0)
        body_pos_subset = body_pos_extend[self.robot_track_bodies_extend_id, :]

        return body_pos_extend, body_pos_subset

    def get_data(self):
        motion_res = self.get_motion()
        ref_body_pos_extend = motion_res["rg_pos_t"].cpu().numpy().squeeze().copy()
        ref_body_vel_extend = motion_res["body_vel_t"].cpu().numpy().squeeze().copy() * self.play_speed_ratio
        ref_body_pos_subset = ref_body_pos_extend[self.motion_track_bodies_extend_id]
        ref_body_vel_subset = ref_body_vel_extend[self.motion_track_bodies_extend_id]

        body_pos_extend, body_pos_subset = self.get_robot_state()

        ctrl_data = {
            "ref_body_pos_subset": ref_body_pos_subset,
            "ref_body_vel_subset": ref_body_vel_subset,
            "robot_body_pos_subset": body_pos_subset,
            "dof_pos": motion_res["dof_pos"].cpu().numpy().squeeze().copy(),
        }

        if (hand_pose := motion_res.get("hand_pose", None)) is not None:
            ctrl_data["hand_pose"] = hand_pose.cpu().numpy().squeeze().copy().reshape(2, -1)

        if self.extra_motion_data:
            ctrl_data.update(
                {
                    "_motion_track_bodies_extend_id": self.motion_track_bodies_extend_id,
                    "_robot_track_bodies_extend_id": self.robot_track_bodies_extend_id,
                    "rg_pos_t": ref_body_pos_extend,
                    "body_vel_t": ref_body_vel_extend,
                    # extra for motion recognition
                    "root_pos": motion_res["root_pos"].cpu().numpy().squeeze().copy(),
                    "root_rot": motion_res["root_rot"].cpu().numpy().squeeze().copy(),
                    "root_vel": motion_res["root_vel"].cpu().numpy().squeeze().copy() * self.play_speed_ratio,
                    "root_ang_vel": motion_res["root_ang_vel"].cpu().numpy().squeeze().copy() * self.play_speed_ratio,
                    "freq": motion_res["freq"].cpu().numpy().squeeze().copy(),
                    "phase": motion_res["phase"].cpu().numpy().squeeze().copy(),
                }
            )

        return ctrl_data


if __name__ == "__main__":
    from pprint import pprint

    from robojudo.config.g1.ctrl.g1_motion_ctrl_cfg import G1MotionH2HCtrlCfg
    from robojudo.config.g1.env.g1_mujuco_env_cfg import G1MujocoEnvCfg
    from robojudo.environment.mujoco_env import MujocoEnv

    env = MujocoEnv(cfg_env=G1MujocoEnvCfg(xml="assets/robots/g1/g1_29dof_rev_1_0.xml"))
    env.reset()
    motion_ctrl = MotionH2HCtrl(
        cfg_ctrl=G1MotionH2HCtrlCfg(motion_name="singles/0-KIT_6_WalkInCounterClockwiseCircle05_1_poses"),
        env=env,
    )

    while True:
        ctrl_data = motion_ctrl.get_data()
        _, commands = motion_ctrl.process_triggers(ctrl_data)
        pprint(ctrl_data)
        motion_ctrl.post_step_callback(commands)
        pprint(commands)
        time.sleep(0.1)
