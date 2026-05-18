import mujoco
import numpy as np
from scipy.spatial.transform import Rotation as sRot


class MujocoVisualizer:
    def __init__(self, viewer):
        self.viewer = viewer

    # TODO: reset: clear all markers

    def update_rg_view(self, body_pos, body_rot, humanoid_id):
        if humanoid_id == 0:
            rgba = (1, 0, 0, 1)
        elif humanoid_id == 1:
            rgba = (0, 1, 0, 1)
        else:
            return

        for j in range(body_pos.shape[0]):
            # need to modify mujoco_viewer to support this
            self.viewer.add_marker(
                pos=body_pos[j],
                size=0.05,
                rgba=rgba,
                type=mujoco.mjtGeom.mjGEOM_SPHERE,  # pyright: ignore[reportAttributeAccessIssue]
                label="",
                id=humanoid_id * 1000 + j,
            )

    def draw_arrow(self, origin, root_quat, vec_local, color, scale=1.0, horizontal_only=False, id=0):
        vec_local = np.array(vec_local, dtype=float)
        r = sRot.from_quat(root_quat)

        if horizontal_only:
            yaw = r.as_euler("xyz", degrees=False)[2]
            yaw_rot = sRot.from_euler("z", yaw).as_matrix()
            vec_world = yaw_rot @ vec_local
        else:
            vec_world = r.as_matrix() @ vec_local

        length = np.linalg.norm(vec_world)
        if length > 1e-6:
            dir_world = vec_world / length
            scaled_length = length * scale

            rot, _ = sRot.align_vectors([dir_world], [[0, 0, 1]])
            mat = rot.as_matrix()

            center_pos = origin  # dir_world * scaled_length * 0.5
        else:
            # zero length
            scaled_length = 0
            mat = np.eye(3)
            center_pos = origin

        self.viewer.add_marker(
            pos=center_pos,
            mat=mat,
            size=np.array([0.02, 0.02, scaled_length]),
            rgba=np.array(color),
            type=mujoco.mjtGeom.mjGEOM_ARROW,  # pyright: ignore[reportAttributeAccessIssue]
            id=3000 + id,
        )
