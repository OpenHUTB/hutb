import numpy as np

from robojudo.environment.utils.mujoco_viz import MujocoVisualizer
from robojudo.policy import Policy, policy_registry
from robojudo.policy.policy_cfgs import H2HPolicyCfg
from robojudo.policy.utils.utils_h2h import compute_imitation_observations_teleop_max_np
from robojudo.utils.util_func import quat_rotate_inverse_np


@policy_registry.register
class H2HStudentPolicy(Policy):
    cfg_policy: H2HPolicyCfg

    def __init__(self, cfg_policy, device):
        super().__init__(cfg_policy=cfg_policy, device=device)

        self.forward_vec = np.asarray([1.0, 0.0, 0.0])
        self.gravity_vec = np.asarray([0.0, 0.0, -1.0])

        self.use_imu_torso = self.cfg_policy.use_imu_torso
        self.use_dof_pos_offset = self.cfg_policy.use_dof_pos_offset
        self.scales_ang_vel = np.asarray(self.cfg_policy.obs_scales.ang_vel, dtype=np.float32)
        self.scales_dof_vel = np.asarray(self.cfg_policy.obs_scales.dof_vel, dtype=np.float32)

        # self.ref_dof_pos = None

        self._init_history(np.zeros(self.history_obs_size))

    def _get_commands(self, ctrl_data):
        target_keys = ["MotionH2HCtrl"]
        # ,"MotionCtrlDeltaHook", "MotionExtendCtrl",  "SMPLCtrl", "RemoteCtrl", "SuperCtrl"]
        motion_res = next((ctrl_data[k] for k in target_keys if k in ctrl_data), None)
        if motion_res is None:
            raise KeyError("No matching motion controller found.")

        ref_body_pos_subset = motion_res["ref_body_pos_subset"].copy()
        ref_body_vel_subset = motion_res["ref_body_vel_subset"].copy()
        robot_body_pos_subset = motion_res["robot_body_pos_subset"].copy()

        # self.ref_dof_pos = motion_res["dof_pos"].copy()

        if "hand_pose" in motion_res:
            ref_hand_pose = motion_res["hand_pose"].copy()
        else:
            ref_hand_pose = None

        return ref_body_pos_subset, ref_body_vel_subset, robot_body_pos_subset, ref_hand_pose

    def get_observation(self, env_data, ctrl_data):
        # from "v-teleop-extend-vr-max-nolinvel"

        # Extrat fdb from env_data
        dof_pos = env_data.dof_pos
        dof_pos_offset = dof_pos - self.default_dof_pos
        dof_vel = env_data.dof_vel

        base_quat = env_data.base_quat
        base_pos = env_data.base_pos

        if self.use_imu_torso:
            torso_quat = env_data.torso_quat
            torso_ang_vel = env_data.torso_ang_vel

            base_ang_vel = torso_ang_vel
            base_gravity = quat_rotate_inverse_np(torso_quat, self.gravity_vec)
        else:
            base_ang_vel = env_data.base_ang_vel
            base_gravity = quat_rotate_inverse_np(base_quat, self.gravity_vec)

        ref_rb_pos_subset, ref_body_vel_subset, body_pos_subset, ref_hand_pose = self._get_commands(ctrl_data)

        # TODO: 2 is buggy
        head_index = -1
        ref_head = ref_rb_pos_subset[head_index]
        direction_to_body = base_pos - ref_head
        xy_direction = direction_to_body[:2]
        distance = np.linalg.norm(xy_direction, axis=-1)

        if distance > 1.0:
            direction_to_body_norm = xy_direction / distance
            ref_rb_pos_subset[head_index, :2] = base_pos[:2] - direction_to_body_norm * 1.0

        task_obs = compute_imitation_observations_teleop_max_np(
            base_pos,  # edited from root_pos
            base_quat,  # edited from root_rot
            body_pos_subset,
            ref_rb_pos_subset,
            ref_body_vel_subset,
            ref_vel_in_task_obs=True,
        )

        history_to_be_append = np.array(self.history_buf).flatten()
        obs = np.concatenate(
            [
                dof_pos if not self.use_dof_pos_offset else dof_pos_offset,
                dof_vel * self.scales_dof_vel,
                base_ang_vel * self.scales_ang_vel,
                base_gravity,
                task_obs,
                self.last_action,
                history_to_be_append,
            ],
            axis=0,
        )

        obs_a = np.concatenate(
            [
                dof_pos,
                dof_vel,
                base_ang_vel,
                base_gravity,
                self.last_action,
            ],
            axis=0,
        )
        self.history_buf.appendleft(obs_a)

        extras = {
            "ref_rb_pos_subset": ref_rb_pos_subset,
            "ref_body_vel_subset": ref_body_vel_subset,
            "body_pos_subset": body_pos_subset,
            "hand_pose": ref_hand_pose,
        }

        return obs, extras

    def reset(self):
        pass

    def post_step_callback(self, commands=None):
        pass

    # def get_init_dof_pos(self):
    #     if self.ref_dof_pos is not None:
    #         return self.ref_dof_pos
    #     return self.default_dof_pos

    def debug_viz(self, visualizer: MujocoVisualizer, env_data, ctrl_data, extras):
        visualizer.update_rg_view(
            body_pos=extras["body_pos_subset"],
            body_rot=np.array([[0, 0, 0, 1]] * len(extras["body_pos_subset"])),
            humanoid_id=0,
        )

        visualizer.update_rg_view(
            body_pos=extras["ref_rb_pos_subset"],
            body_rot=np.array([[0, 0, 0, 1]] * len(extras["ref_rb_pos_subset"])),
            humanoid_id=1,
        )


if __name__ == "__main__":
    pass
