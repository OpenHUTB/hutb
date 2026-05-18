# from .motion_lib import MotionLibReal
import logging
import time

import torch

from robojudo.controller import ctrl_registry
from robojudo.controller.ctrl_cfgs import MotionTwistCtrlCfg
from robojudo.controller.motion_ctrl import MotionCtrl
from robojudo.environment import Environment
from robojudo.utils.util_func_torch import (
    get_euler_xyz_in_tensor,
    quat_rotate_inverse,
)

logger = logging.getLogger(__name__)


# TODO: bug: motion source as 29dof
@ctrl_registry.register
class MotionTwistCtrl(MotionCtrl):
    cfg_ctrl: MotionTwistCtrlCfg
    env: Environment

    def __init__(self, cfg_ctrl, env, device="cpu"):
        super().__init__(cfg_ctrl=cfg_ctrl, env=env, device=device)
        self.robot_type = self.cfg_ctrl.robot

        self._get_future_motion_targets()

    def post_step_callback(self, commands: list[str] | None = None):
        self._get_future_motion_targets()
        super().post_step_callback(commands)

    def _get_future_motion_targets(self):
        num_steps = 1
        motion_res = self.get_motion()  # get one step motion data
        # Retrieve motion frames
        root_pos = motion_res["root_pos"]
        root_rot = motion_res["root_rot"]
        root_vel = motion_res["root_vel"]
        root_ang_vel = motion_res["root_ang_vel"]
        dof_pos = motion_res["dof_pos"]

        flat_root_rot = root_rot.reshape(num_steps, 4)
        flat_root_vel = root_vel.reshape(num_steps, 3)
        flat_root_ang_vel = root_ang_vel.reshape(num_steps, 3)

        rpy = get_euler_xyz_in_tensor(flat_root_rot)
        root_vel = quat_rotate_inverse(flat_root_rot, flat_root_vel, w_last=True).view(num_steps, 3)
        root_ang_vel = quat_rotate_inverse(flat_root_rot, flat_root_ang_vel, w_last=True).view(num_steps, 3)

        if self.robot_type == "g1":
            dof_pos_with_wrist = torch.zeros(25).reshape(1, 25)
            wrist_ids = [19, 24]
            other_ids = [f for f in range(25) if f not in wrist_ids]
            dof_pos_with_wrist[..., other_ids] = dof_pos
            dof_pos = dof_pos_with_wrist

        mimic_obs_buf = torch.cat(
            [
                root_pos[..., 2:3],
                rpy,
                root_vel,
                root_ang_vel[..., 2:3],
                dof_pos,
            ],
            dim=-1,
        )

        self._mimic_obs_buf = mimic_obs_buf.flatten()

        # Hands
        if (hand_pose := motion_res.get("hand_pose", None)) is not None:
            self._hand_pose = hand_pose.cpu().numpy().squeeze().copy().reshape(2, -1)
        else:
            self._hand_pose = None

    def get_data(self):
        ctrl_data = {
            "action_mimic": self._mimic_obs_buf.cpu().numpy().copy(),
        }

        if self._hand_pose is not None:
            ctrl_data["hand_pose"] = self._hand_pose

        return ctrl_data


if __name__ == "__main__":
    from pprint import pprint

    from robojudo.config.g1.ctrl.g1_motion_ctrl_cfg import G1MotionTwistCtrlCfg

    motion_ctrl = MotionTwistCtrl(
        cfg_ctrl=G1MotionTwistCtrlCfg(),
        env=None,
    )
    from robojudo.utils.progress import ProgressBar

    pbar = ProgressBar("test motion ctrl", total=-1)

    while True:
        ctrl_data = motion_ctrl.get_data()
        _, commands = motion_ctrl.process_triggers(ctrl_data)
        motion_ctrl.post_step_callback(commands)
        pprint(ctrl_data)
        pprint(commands)
        pbar.update(1)
        time.sleep(0.1)
