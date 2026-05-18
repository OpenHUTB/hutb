import logging
import time

import numpy as np

from robojudo.environment import Environment, env_registry
from robojudo.environment.env_cfgs import RobotEnvCfg

logger = logging.getLogger(__name__)


@env_registry.register
class DummyEnv(Environment):
    cfg_env: RobotEnvCfg

    def __init__(self, cfg_env: RobotEnvCfg, device="cpu"):
        super().__init__(cfg_env=cfg_env, device=device)

        self.odometry_type = cfg_env.odometry_type
        if self.odometry_type == "ZED":
            assert cfg_env.zed_cfg is not None, "zed_cfg must be set if odometry_type is 'ZED'"
            from robojudo.tools.zed_odometry import ZedOdometry

            self.zed_odometry = ZedOdometry(cfg_env.zed_cfg)

        self.set_gains(self.stiffness, self.damping)
        # Initiate interface for robot state and actions
        self.self_check()

        logger.info("[DummyEnv] initialized!")

    def reset(self):
        logger.info("[DummyEnv] on reset")
        if self.born_place_align:
            if self.odometry_type == "ZED":
                self.zed_odometry.set_zreo()

    def self_check(self):
        logger.info("[DummyEnv] self check")

    def set_gains(self, stiffness, damping):
        self.stiffness = stiffness
        self.damping = damping
        logger.info(f"[DummyEnv] set gains: stiffness={stiffness}, damping={damping}")

    def update(self):
        logger.info("[DummyEnv] update")
        if self.odometry_type == "ZED":
            self.zed_odometry.update()
            if self.zed_odometry.is_valid:
                self._base_pos = self.zed_odometry.pos
                self._base_lin_vel = self.zed_odometry.lin_vel

        if self.update_with_fk:
            self._fk_info = self.fk()

    def step(self, pd_target, hand_pose=None):
        assert len(pd_target) == self.num_dofs, "pd_target len should be num_dofs of env"
        logger.info(f"[DummyEnv] step with pd_target: {pd_target}, hand_pose: {hand_pose}")

    def shutdown(self):
        logger.info("[DummyEnv] shutdown")


if __name__ == "__main__":
    # Example usage
    from robojudo.config.g1.env.g1_dummy_env_cfg import G1DummyEnvCfg

    cfg_env = G1DummyEnvCfg()

    robot_env = DummyEnv(cfg_env=cfg_env)
    while True:
        robot_env.update()
        robot_env.step(np.random.rand(robot_env.num_dofs))
        time.sleep(0.02)
