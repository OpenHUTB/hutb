import numpy as np
from scipy.spatial.transform import Rotation as sRot


def quatToEuler(quat):
    eulerVec = np.zeros(3)
    qx = quat[0]
    qy = quat[1]
    qz = quat[2]
    qw = quat[3]
    # roll (x-axis rotation)
    sinr_cosp = 2 * (qw * qx + qy * qz)
    cosr_cosp = 1 - 2 * (qx * qx + qy * qy)
    eulerVec[0] = np.arctan2(sinr_cosp, cosr_cosp)

    # pitch (y-axis rotation)
    sinp = 2 * (qw * qy - qz * qx)
    if np.abs(sinp) >= 1:
        eulerVec[1] = np.copysign(np.pi / 2, sinp)  # use 90 degrees if out of range
    else:
        eulerVec[1] = np.arcsin(sinp)

    # yaw (z-axis rotation)
    siny_cosp = 2 * (qw * qz + qx * qy)
    cosy_cosp = 1 - 2 * (qy * qy + qz * qz)
    eulerVec[2] = np.arctan2(siny_cosp, cosy_cosp)

    return eulerVec


def eulerToQuat(euler):
    roll, pitch, yaw = euler
    cy = np.cos(yaw * 0.5)
    sy = np.sin(yaw * 0.5)
    cp = np.cos(pitch * 0.5)
    sp = np.sin(pitch * 0.5)
    cr = np.cos(roll * 0.5)
    sr = np.sin(roll * 0.5)

    w = cr * cp * cy + sr * sp * sy
    x = sr * cp * cy - cr * sp * sy
    y = cr * sp * cy + sr * cp * sy
    z = cr * cp * sy - sr * sp * cy

    return np.array([x, y, z, w])


def get_gravity_orientation(quaternion):
    qw = quaternion[3]
    qx = quaternion[0]
    qy = quaternion[1]
    qz = quaternion[2]

    gravity_orientation = np.zeros(3)

    gravity_orientation[0] = 2 * (-qz * qx + qw * qy)
    gravity_orientation[1] = -2 * (qz * qy + qw * qx)
    gravity_orientation[2] = 1 - 2 * (qw * qw + qz * qz)

    return gravity_orientation


# =========================================


def my_quat_rotate_np(quat, vec):
    r = sRot.from_quat(quat)
    return r.apply(vec)


def quat_rotate_inverse_np(quat, vec):
    r = sRot.from_quat(quat)
    r_inv = r.inv()
    return r_inv.apply(vec)


def calc_heading_quat_np(quat):
    rot = sRot.from_quat(quat)
    mat = rot.as_matrix()

    forward = mat[:, 0]
    yaw = np.arctan2(forward[1], forward[0])

    heading_rot = sRot.from_euler("z", yaw)
    return heading_rot.as_quat()


def calc_heading_quat_inv_np(quat):
    heading_quat = calc_heading_quat_np(quat)
    r_inv = sRot.from_quat(heading_quat).inv()
    return r_inv.as_quat()


# =========================================
def command_remap(command, new_range, old_range=None):
    if not isinstance(command, np.ndarray):
        command = np.array(command, dtype=np.float32)
    if old_range is None:
        old_range = [-1.0, 0.0, 1.0]

    old_min, old_mid, old_max = old_range
    new_min, new_mid, new_max = new_range
    if abs((command - old_mid) / (old_max - old_min)) < 0.02:
        return np.full_like(command, new_mid, dtype=np.float32)

    scale_neg = (new_mid - new_min) / (old_mid - old_min)
    scale_pos = (new_max - new_mid) / (old_max - old_mid)

    return np.where(
        command < old_mid, new_mid + (command - old_mid) * scale_neg, new_mid + (command - old_mid) * scale_pos
    )


# =========================================
# From isaaclab.util.math
# refact using numpy istead of torch


def matrix_from_quat(quat: np.ndarray) -> np.ndarray:
    """Convert quaternion to rotation matrix.
    Args:
        quat: (4,) [x,y,z,w]
    Returns:
        mat: (3,3)
    """
    r = sRot.from_quat(quat)
    return r.as_matrix()


def subtract_frame_transforms(
    t01: np.ndarray, q01: np.ndarray, t02: np.ndarray | None = None, q02: np.ndarray | None = None
) -> tuple[np.ndarray, np.ndarray]:
    """
    Compute T12 = T01^{-1} * T02 using scipy Rotation.
    t01, q01: (N,3), (N,4) [x,y,z,w]
    t02, q02: optional, same shape
    Returns: t12 (N,3), q12 (N,4)
    """
    # Rotation inverse
    r01 = sRot.from_quat(q01)
    r10 = r01.inv()

    if q02 is not None:
        r02 = sRot.from_quat(q02)
        r12 = r10 * r02
    else:
        r12 = r10

    # Translation
    if t02 is not None:
        t_rel = t02 - t01
    else:
        t_rel = -t01
    t12 = r10.apply(t_rel)

    q12 = r12.as_quat()
    return t12, q12
