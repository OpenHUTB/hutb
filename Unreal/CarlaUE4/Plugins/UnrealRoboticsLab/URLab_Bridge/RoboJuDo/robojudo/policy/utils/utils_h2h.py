import numpy as np

from robojudo.utils.util_func import calc_heading_quat_inv_np, my_quat_rotate_np


def compute_imitation_observations_teleop_max_np(
    root_pos,  # (3,)
    root_rot,  # (4,)
    body_pos,  # (J, 3)
    ref_body_pos,  # (J, 3)
    ref_body_vel,  # (J, 3)
    ref_vel_in_task_obs=True,
):
    obs = []
    J = body_pos.shape[0]

    # heading rotations
    heading_inv_rot = calc_heading_quat_inv_np(root_rot)  # (4,)
    heading_inv_rot_expand = np.tile(heading_inv_rot, (J, 1))  # (J, 4)

    # --- local body position difference
    diff_global_body_pos = ref_body_pos - body_pos  # (J, 3)
    diff_local_body_pos = my_quat_rotate_np(heading_inv_rot_expand, diff_global_body_pos)  # (J, 3)

    # --- reference body position in local frame
    local_ref_body_pos = ref_body_pos - root_pos[np.newaxis, :]  # (J, 3)
    local_ref_body_pos = my_quat_rotate_np(heading_inv_rot_expand, local_ref_body_pos)  # (J, 3)

    # --- reference body velocity in local frame
    local_ref_body_vel = my_quat_rotate_np(heading_inv_rot_expand, ref_body_vel)  # (J, 3)

    # --- flatten and concatenate
    obs.append(diff_local_body_pos.reshape(-1))  # (J * 3,)
    obs.append(local_ref_body_pos.reshape(-1))  # (J * 3,)
    if ref_vel_in_task_obs:
        obs.append(local_ref_body_vel.reshape(-1))  # (J * 3,)

    return np.concatenate(obs, axis=-1)  # shape: (feature_dim,)
