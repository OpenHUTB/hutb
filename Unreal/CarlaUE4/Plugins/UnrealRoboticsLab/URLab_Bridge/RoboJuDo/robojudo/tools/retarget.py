import numpy as np


class HandRetarget:
    def __init__(self, cfg=None):
        # fingers parameters
        self.REVERSE_FINGERS_ORDER = True
        self.RANGE_FINGERS_POSE = (0.0, 1.7)
        self.RANGE_THUMB_BENDING_POSE = (-0.1, 0.6)
        self.RANGE_THUMB_ROTATION_POSE = (-0.1, 1.3)

        self.RANGE_FINGERS_CMD = (1.0, 0.0)
        self.RANGE_THUMB_BENDING_CMD = (1.0, 0.0)
        self.RANGE_THUMB_ROTATION_CMD = (1.0, 0.0)

        if cfg is not None:
            self.REVERSE_FINGERS_ORDER = cfg.get("reverse_fingers_order", self.REVERSE_FINGERS_ORDER)
            self.RANGE_FINGERS_POSE = cfg.get("fingers_pose", self.RANGE_FINGERS_POSE)
            self.RANGE_THUMB_BENDING_POSE = cfg.get("thumb_bending_pose", self.RANGE_THUMB_BENDING_POSE)
            self.RANGE_THUMB_ROTATION_POSE = cfg.get("thumb_rotation_pose", self.RANGE_THUMB_ROTATION_POSE)
            self.RANGE_FINGERS_CMD = cfg.get("fingers_cmd", self.RANGE_FINGERS_CMD)
            self.RANGE_THUMB_BENDING_CMD = cfg.get("thumb_bending_cmd", self.RANGE_THUMB_BENDING_CMD)
            self.RANGE_THUMB_ROTATION_CMD = cfg.get("thumb_rotation_cmd", self.RANGE_THUMB_ROTATION_CMD)

    def _retarget(self, angles, limits, target_range):
        limit_min, limit_max = min(limits), max(limits)
        angles = np.clip(angles, limit_min, limit_max)
        # angles = np.interp(angles, limits, target_range)
        angles = (angles - limits[0]) / (limits[1] - limits[0]) * (target_range[1] - target_range[0]) + target_range[0]
        return angles

    def from_pose_to_cmd(self, hand_pose):
        # hand_pose as (2, 6)
        if hand_pose.shape == (12,):
            hand_pose = hand_pose.reshape(2, 6)

        hand_pose_retarged_list = []
        for hand_pose_single in hand_pose:
            if self.REVERSE_FINGERS_ORDER:
                hand_pose_single = hand_pose_single[::-1]

            fingers_pose = hand_pose_single[:4]
            thumb_poses = hand_pose_single[4:]

            four_fingers_cmd = self._retarget(fingers_pose, self.RANGE_FINGERS_POSE, self.RANGE_FINGERS_CMD)
            thumb_bending_cmd = self._retarget(
                thumb_poses[0], self.RANGE_THUMB_BENDING_POSE, self.RANGE_THUMB_BENDING_CMD
            )
            thumb_rotation_cmd = self._retarget(
                thumb_poses[1], self.RANGE_THUMB_ROTATION_POSE, self.RANGE_THUMB_ROTATION_CMD
            )

            hand_pose_single_retarged = np.concatenate([four_fingers_cmd, [thumb_bending_cmd, thumb_rotation_cmd]])
            hand_pose_retarged_list.append(hand_pose_single_retarged)

        hand_pose_retarged = np.array(hand_pose_retarged_list)
        return hand_pose_retarged

    def from_cmd_to_pose(self, hand_cmd):
        # hand_cmd as (2, 6)
        if hand_cmd.shape == (12,):
            hand_cmd = hand_cmd.reshape(2, 6)

        hand_cmd_retarged_list = []
        for hand_cmd_single in hand_cmd:
            fingers_cmd = hand_cmd_single[:4]
            thumb_cmds = hand_cmd_single[4:]

            four_fingers_pose = self._retarget(fingers_cmd, self.RANGE_FINGERS_CMD, self.RANGE_FINGERS_POSE)
            thumb_bending_pose = self._retarget(
                thumb_cmds[0], self.RANGE_THUMB_BENDING_CMD, self.RANGE_THUMB_BENDING_POSE
            )
            thumb_rotation_pose = self._retarget(
                thumb_cmds[1], self.RANGE_THUMB_ROTATION_CMD, self.RANGE_THUMB_ROTATION_POSE
            )

            hand_cmd_single_retarged = np.concatenate([four_fingers_pose, [thumb_bending_pose, thumb_rotation_pose]])
            hand_cmd_retarged_list.append(hand_cmd_single_retarged)

        hand_cmd_retarged = np.array(hand_cmd_retarged_list)
        return hand_cmd_retarged


if __name__ == "__main__":
    from robojudo.config import CFG

    cfg = CFG.env.hand_retarget
    hand_retarget = HandRetarget(cfg)
    hand_pose = np.array([0.0] * 12)
    hand_cmd = hand_retarget.from_pose_to_cmd(hand_pose)
    print("Hand Pose:", hand_pose)
    print("Hand Command:", hand_cmd)
