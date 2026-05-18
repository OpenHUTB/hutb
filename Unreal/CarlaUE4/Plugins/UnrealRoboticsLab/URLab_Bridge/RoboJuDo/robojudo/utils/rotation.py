import logging

import numpy as np
from scipy.spatial.transform import Rotation as sRot

logger = logging.getLogger(__name__)


class TransformAlignment:
    """Align rotation and/or translation relative to a base reference."""

    def __init__(self, quat=None, pos=None, yaw_only=False, xy_only=False):
        """
        quat: (4,) [x,y,z,w], default identity
        pos: (3,), default zero
        """
        self.yaw_only = yaw_only
        self.xy_only = xy_only
        self.set_base(quat, pos)

    def set_base(self, quat=None, pos=None):
        if quat is None:
            self.R_base = sRot.identity()
        else:
            R_base = sRot.from_quat(quat)
            if self.yaw_only:
                euler = R_base.as_euler("xyz")
                euler[:2] = 0.0
                R_base = sRot.from_euler("xyz", euler)
            self.R_base = R_base
        if pos is None:
            self.p_base = np.zeros(3)
        else:
            p_base = np.asarray(pos, dtype=float)
            if self.xy_only:
                p_base[2] = 0.0
            self.p_base = p_base

        logger.info(f"base set to pos: {self.p_base}, quat: {self.R_base.as_quat()}")

    def align_quat(self, quat):
        """Align rotation relative to base. Input/Output: [x,y,z,w]"""
        R_cur = sRot.from_quat(quat)
        R_rel = self.R_base.inv() * R_cur
        return R_rel.as_quat()

    def align_xyz(self, xyz):
        """Align a vector ignoring translation"""
        xyz = np.asarray(xyz)
        return self.R_base.inv().apply(xyz)

    def align_pos(self, pos):
        """Align position relative to base. Input: (3,) or (N,3), Output: same shape"""
        pos = np.asarray(pos)
        return self.align_xyz(pos - self.p_base)

    def align_transform(self, quat, pos):
        """Full SE3 alignment, returns (quat, pos)"""
        quat_aligned = self.align_quat(quat)
        pos_aligned = self.align_pos(pos)
        return quat_aligned, pos_aligned


if __name__ == "__main__":
    # base: 90° yaw, pos [1,0,0]
    from scipy.spatial.transform import Rotation as R

    base_quat = R.from_euler("xyz", [0, 0, np.pi / 2]).as_quat()
    base_pos = np.array([1.0, 0.0, 0.0])

    aligner = TransformAlignment(base_quat, base_pos)

    quat = R.from_euler("xyz", [0, 0, np.pi / 4]).as_quat()
    pos = np.array([2.0, 0.0, 0.0])

    aligned_quat, aligned_pos = aligner.align_transform(quat, pos)
    print("Original quat:", R.from_quat(quat).as_euler("xyz", degrees=True))
    print("Aligned quat:", R.from_quat(aligned_quat).as_euler("xyz", degrees=True))
    print("Original pos:", pos)
    print("Aligned pos:", aligned_pos)
