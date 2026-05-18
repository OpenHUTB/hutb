from abc import ABC, abstractmethod

import numpy as np
from box import Box

from robojudo.tools.dof import merge_dof_cfgs
from robojudo.tools.kinematics import MujocoKinematics
from robojudo.tools.tool_cfgs import DoFConfig
from robojudo.utils.rotation import TransformAlignment

from .env_cfgs import EnvCfg


class Environment(ABC):
    def __init__(self, cfg_env: EnvCfg, device: str = "cpu"):
        self.cfg_env = cfg_env
        self.device = device

        self.kinematics = None
        if self.cfg_env.forward_kinematic is not None:
            self.kinematics = MujocoKinematics(cfg=self.cfg_env.forward_kinematic)
        self.update_with_fk = self.cfg_env.update_with_fk
        self._torso_name = self.cfg_env.torso_name

        # init env dofs config
        self.update_dof_cfg()

        # Feedback variables, all in radian
        self._dof_pos = np.zeros(self.num_dofs)
        self._dof_vel = np.zeros(self.num_dofs)
        self._base_rpy = np.zeros(3)
        self._base_quat = np.array([0.0, 0.0, 0.0, 1.0])  # as x, y, z, w
        self._base_ang_vel = np.zeros(3)

        # optional variables, may not be available in all envs
        self._base_pos: np.ndarray | None = None
        self._base_lin_vel: np.ndarray | None = None
        self._base_lin_acc: np.ndarray | None = None
        self._torso_pos: np.ndarray | None = None
        self._torso_quat: np.ndarray | None = None
        self._torso_ang_vel: np.ndarray | None = None
        self._fk_info: dict | None = None

        # born place alignment
        self.born_place_align = self.cfg_env.born_place_align
        self.base_align = TransformAlignment(yaw_only=True, xy_only=True)

        self.visualizer = None  # for sim viz debug plot

    def update_dof_cfg(self, override_cfg: DoFConfig | None = None):
        dof_config: DoFConfig = self.cfg_env.dof
        if override_cfg is not None:
            dof_config = merge_dof_cfgs(self.cfg_env.dof, override_cfg)

        self.dof_cfg = dof_config
        self.joint_names = dof_config.joint_names
        self.num_dofs = dof_config.num_dofs
        self.default_pos = np.asarray(dof_config.default_pos)
        self.stiffness = np.asarray(dof_config.stiffness)
        self.damping = np.asarray(dof_config.damping)
        self.torque_limits = np.asarray(dof_config.torque_limits)
        self.position_limits = np.asarray(dof_config.position_limits)

        self.set_gains(self.stiffness, self.damping)  # TODO: temp solution

        # if self.kinematics is not None: # TODO: check usage
        #     self.kinematics.update_joint_names_subset(self.joint_names)

    def set_born_place(self, quat: np.ndarray | None = None, pos: np.ndarray | None = None):
        """Need to be called with real quat and pos from subclass"""
        self.base_align.set_base(quat, pos)

    @abstractmethod
    def self_check(self):
        raise NotImplementedError

    @abstractmethod
    def reset(self):
        raise NotImplementedError

    @abstractmethod
    def update(self):
        raise NotImplementedError

    def fk(self):
        if self.kinematics is None:
            raise ValueError("Kinematics model not initialized.")
        fk_info = self.kinematics.forward(
            joint_pos=self.dof_pos,
            base_pos=self.base_pos,
            base_quat=self.base_quat,
            base_ang_vel=self.base_ang_vel,
            base_lin_vel=self.base_lin_vel,
        )
        return fk_info

    @abstractmethod
    def step(self, pd_target, hand_pose=None):
        assert len(pd_target) == self.num_dofs, "pd_target len should be num_dofs of env"
        raise NotImplementedError

    @abstractmethod
    def shutdown(self):
        raise NotImplementedError

    @abstractmethod
    def set_gains(self, stiffness, damping):
        raise NotImplementedError

    # === Properties ===
    # ALL in radian
    @property
    def dof_pos(self):
        return self._dof_pos.copy()

    @property
    def dof_vel(self):
        return self._dof_vel.copy()

    # @property # TODO: disabled due to conflict with base_quat
    # def base_rpy(self):
    #     return self._base_rpy

    @property
    def base_quat(self):
        return self._base_quat.copy()

    @property
    def base_ang_vel(self):
        return self._base_ang_vel.copy()

    # == Optional Properties ==
    @property
    def base_pos(self):
        return self._base_pos.copy() if self._base_pos is not None else None

    @property
    def base_lin_vel(self):
        return self._base_lin_vel.copy() if self._base_lin_vel is not None else None

    @property
    def base_lin_acc(self):
        return self._base_lin_acc.copy() if self._base_lin_acc is not None else None

    @property
    def torso_pos(self):
        return self._torso_pos.copy() if self._torso_pos is not None else None

    @property
    def torso_quat(self):
        return self._torso_quat.copy() if self._torso_quat is not None else None

    @property
    def torso_ang_vel(self):
        return self._torso_ang_vel.copy() if self._torso_ang_vel is not None else None

    @property
    def fk_info(self):
        return self._fk_info.copy() if self._fk_info is not None else None

    def get_data(self):
        env_data = {
            "dof_pos": self.dof_pos,
            "dof_vel": self.dof_vel,
            # "base_rpy": self.base_rpy,
            "base_quat": self.base_quat,
            "base_ang_vel": self.base_ang_vel,
            "base_lin_acc": self.base_lin_acc,
            "base_pos": self.base_pos,
            "base_lin_vel": self.base_lin_vel,
            "torso_pos": self.torso_pos,
            "torso_quat": self.torso_quat,
            "torso_ang_vel": self.torso_ang_vel,
            "fk_info": self.fk_info,
        }
        return Box(env_data)
