import numpy as np

from robojudo.policy import Policy, policy_registry
from robojudo.policy.policy_cfgs import TwistPolicyCfg
from robojudo.utils.util_func import quatToEuler


@policy_registry.register
class TwistPolicy(Policy):
    """From [TWIST](https://github.com/YanjieZe/TWIST)"""

    cfg_policy: TwistPolicyCfg

    def __init__(self, cfg_policy, device):
        super().__init__(cfg_policy=cfg_policy, device=device)

        self.obs_scales = self.cfg_policy.obs_scales
        self._init_history(np.zeros(self.history_obs_size))

        self.ankle_idx = self.cfg_policy.ankle_idx
        self.mimic_obs_total_degrees = self.cfg_policy.mimic_obs_total_degrees
        self.mimic_obs_wrist_ids = self.cfg_policy.mimic_obs_wrist_ids
        self.mimic_obs_other_ids = self.cfg_policy.mimic_obs_other_ids

        self.reset()

    def reset(self):
        self.timestep: int = 0

    def post_step_callback(self, commands=None):
        self.timestep += 1

    def _extract_mimic_obs_to_body_and_wrist(self, mimic_obs):
        policy_target = mimic_obs[self.mimic_obs_other_ids]
        wrist_dof_pos = mimic_obs[self.mimic_obs_wrist_ids]

        return policy_target, wrist_dof_pos

    def _get_motion_command(self, ctrl_data):
        target_keys = ["TwistRedisCtrl", "MotionTwistCtrl"]
        motion_res = next((ctrl_data[k] for k in target_keys if k in ctrl_data), None)
        if motion_res is None:
            raise KeyError("No matching motion controller found.")

        action_mimic = motion_res["action_mimic"].copy()

        if "hand_pose" in motion_res:
            ref_hand_pose = motion_res["hand_pose"].copy()
        else:
            ref_hand_pose = None

        return action_mimic, ref_hand_pose

    def get_observation(self, env_data, ctrl_data):
        action_mimic, ref_hand_pose = self._get_motion_command(ctrl_data)

        base_quat = env_data.base_quat
        dof_pos = env_data.dof_pos
        dof_vel = env_data.dof_vel
        ang_vel = env_data.base_ang_vel

        rpy = quatToEuler(base_quat)

        obs_dof_vel = dof_vel.copy()
        obs_dof_vel[self.ankle_idx] = 0.0

        obs_proprio = np.concatenate(
            [
                ang_vel * self.obs_scales.ang_vel,
                rpy[:2],
                (dof_pos - self.default_dof_pos) * self.obs_scales.dof_pos,
                obs_dof_vel * self.obs_scales.dof_vel,
                self.last_action,
            ]
        )

        action_mimic, wrist_dof_pos = self._extract_mimic_obs_to_body_and_wrist(action_mimic)
        obs_full = np.concatenate([action_mimic, obs_proprio])

        obs_hist = np.array(self.history_buf).flatten()
        obs_buf = np.concatenate([obs_full, obs_hist])

        self.history_buf.append(obs_full)

        extras = {}
        return obs_buf, extras


if __name__ == "__main__":
    from robojudo.config.g1.policy.g1_twist_policy_cfg import G1TwistPolicyCfg

    cfg_policy = G1TwistPolicyCfg()
    policy = TwistPolicy(cfg_policy=cfg_policy, device="cpu")
