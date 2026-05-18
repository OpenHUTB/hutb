import logging
import time

import mujoco
import numpy as np

from robojudo.environment.utils.mujoco_viz import MujocoVisualizer

from .tool_cfgs import ForwardKinematicCfg

logger = logging.getLogger(__name__)


class MujocoKinematics:
    def __init__(self, cfg: ForwardKinematicCfg):
        self.cfg = cfg
        self.model = mujoco.MjModel.from_xml_path(self.cfg.xml_path)  # pyright: ignore[reportAttributeAccessIssue]
        self.data = mujoco.MjData(self.model)  # pyright: ignore[reportAttributeAccessIssue]
        logger.debug(f"Loaded model from {self.cfg.xml_path}")

        # base joint type
        self.has_free_joint = self.model.jnt_type[0] == mujoco.mjtJoint.mjJNT_FREE  # pyright: ignore[reportAttributeAccessIssue]
        self.qpos_offset = 7 if self.has_free_joint else 0

        # body and joint info
        self.num_bodies = self.model.nbody
        self.body_offset = 1 if self.has_free_joint else 0  # TODO: check base, changes from old version

        body_names = []
        for i in range(self.body_offset, self.num_bodies):
            name = mujoco.mj_id2name(self.model, mujoco.mjtObj.mjOBJ_BODY, i)  # pyright: ignore[reportAttributeAccessIssue]
            body_names.append(name)
        self.body_names = body_names

        self.update_joint_names_subset(self.cfg.kinematic_joint_names)

        # debug viz
        self.debug_viz = self.cfg.debug_viz
        if self.debug_viz:
            import mujoco_viewer

            self.viewer = mujoco_viewer.MujocoViewer(  # TODO: BUGGY with multiple instances
                self.model,
                self.data,
                width=900,
                height=900,
                hide_menus=True,
            )
            self.viewer.cam.distance = 3.0
            self.viewer._render_every_frame = True
            self.debug_viz_last_render_time = 0.0

            self.visualizer = MujocoVisualizer(self.viewer)

    def update_joint_names_subset(self, joint_names_subset: list[str] | None = None):
        if joint_names_subset is not None:
            self.joint_names = joint_names_subset
        else:
            self.joint_names = []
            for i in range(self.model.njnt):
                # skip root joint
                if self.has_free_joint and i == 0:
                    continue
                joint_name = mujoco.mj_id2name(self.model, mujoco.mjtObj.mjOBJ_JOINT, i)  # pyright: ignore[reportAttributeAccessIssue]
                self.joint_names.append(joint_name)

        self.num_joints = len(self.joint_names)

        self.joint_qpos_indices = []
        for joint_name in self.joint_names:
            joint_id = mujoco.mj_name2id(self.model, mujoco.mjtObj.mjOBJ_JOINT, joint_name)  # pyright: ignore[reportAttributeAccessIssue]
            if joint_id == -1:
                raise ValueError(f"Joint {joint_name} not found in the model.")
            qpos_addr = self.model.jnt_qposadr[joint_id]
            self.joint_qpos_indices.append(qpos_addr)

        logger.debug(f"[MujocoKinematics] set fk with {self.num_joints} joints")

    def forward(
        self,
        joint_pos: np.ndarray,
        base_pos: np.ndarray | None = None,
        base_quat: np.ndarray | None = None,
        joint_vel: np.ndarray | None = None,
        base_lin_vel: np.ndarray | None = None,
        base_ang_vel: np.ndarray | None = None,
    ) -> dict:
        assert joint_pos.shape[0] == self.num_joints, (
            f"Expected joint_pos of shape ({self.num_joints},), got {joint_pos.shape}"
        )

        # ---------------- qpos ----------------
        qpos_full = np.zeros(self.model.nq, dtype=np.float64)

        if self.has_free_joint:
            if base_pos is not None:
                assert base_pos.shape == (3,), "base_pos must be of shape (3,)"
                qpos_full[0:3] = base_pos

            if base_quat is not None:
                assert base_quat.shape == (4,), "base_quat must be of shape (4,)"
                qpos_full[3:7] = base_quat[[3, 0, 1, 2]]

        qpos_full[self.joint_qpos_indices] = joint_pos
        self.data.qpos[:] = qpos_full

        # ---------------- qvel ----------------
        if joint_vel is not None or base_lin_vel is not None or base_ang_vel is not None:
            qvel_full = np.zeros(self.model.nv, dtype=np.float64)
            if self.has_free_joint:
                if base_lin_vel is not None:
                    qvel_full[0:3] = base_lin_vel
                if base_ang_vel is not None:
                    qvel_full[3:6] = base_ang_vel
                offset = 6
            else:
                offset = 0
            if joint_vel is not None:
                qvel_full[offset : offset + self.num_joints] = joint_vel
            self.data.qvel[:] = qvel_full

        # ---------------- forward ----------------
        mujoco.mj_forward(self.model, self.data)  # pyright: ignore[reportAttributeAccessIssue]

        # ---------------- body info ----------------
        nbody = self.model.nbody
        offset = 1 if self.has_free_joint else 0  # TODO: check base, changes from old version
        body_info = {}
        for i in range(offset, nbody):
            name = mujoco.mj_id2name(self.model, mujoco.mjtObj.mjOBJ_BODY, i)  # pyright: ignore[reportAttributeAccessIssue]
            pos = self.data.xpos[i].copy()
            quat = self.data.xquat[i].copy()[[1, 2, 3, 0]]  # [x, y, z, w]
            lin_vel = self.data.cvel[i].copy()[3:]  # cvel = [ang, lin]
            ang_vel = self.data.cvel[i].copy()[0:3]
            body_info[name] = dict(
                pos=pos,
                quat=quat,
                lin_vel=lin_vel,
                ang_vel=ang_vel,
            )

        if self.debug_viz:
            if (time.time() - self.debug_viz_last_render_time) >= 0.02 and self.viewer.is_alive:
                self.viewer.cam.lookat = self.data.qpos.astype(np.float32)[:3]
                self.viewer.render()
                self.debug_viz_last_render_time = time.time()

                # add viz for base lin_vel
                self.visualizer.draw_arrow(
                    origin=base_pos if base_pos is not None else np.zeros((3,)),
                    root_quat=base_quat if base_quat is not None else np.array([0, 0, 0, 1]),
                    vec_local=base_lin_vel if base_lin_vel is not None else [0, 0, 0],
                    color=[0, 1, 0, 1],
                    scale=5,
                    horizontal_only=False,
                    id=999,
                )

        return body_info

    def __del__(self):
        try:
            if self.debug_viz:
                self.viewer.close()
        except Exception:
            pass


def test():
    from robojudo.config.g1.ctrl.g1_motion_ctrl_cfg import G1MotionCtrlCfg

    cfg = G1MotionCtrlCfg()
    phc_robot_config = cfg.phc.robot_config
    xml_path = phc_robot_config["asset"]["assetFileName"]
    kine = MujocoKinematics(cfg=ForwardKinematicCfg(xml_path=xml_path, debug_viz=True))

    kine.forward(
        joint_pos=np.zeros((kine.num_joints,)),
        base_pos=np.zeros((3,)),
        base_quat=np.array([0, 0, 0, 1]),
    )
    time.sleep(2)
    pass


if __name__ == "__main__":
    test()
