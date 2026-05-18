import time
import numpy as np

from unitree_cpp import UnitreeController, RobotState, SportState  # type: ignore
from config import RobotConfig

class UnitreeCppEnv():
    def __init__(self, cfg: RobotConfig):
        self.num_dofs = cfg.num_dofs
        self.joint_names = cfg.joint_names
        self.stiffness = cfg.stiffness
        self.damping = cfg.damping
        self.default_pos = cfg.default_pos
        self.dof_idx = cfg.joint2motor_idx

        cfg_unitree = cfg.unitree.to_dict()
        cfg_unitree["num_dofs"] = self.num_dofs
        cfg_unitree["stiffness"] = self.stiffness
        cfg_unitree["damping"] = self.damping
        self.unitree = UnitreeController(cfg_unitree)

        self.msg_type = cfg_unitree["msg_type"]
        self.enable_odometry = cfg_unitree["enable_odometry"]

        self.robot_state: RobotState = None
        self.sport_state: SportState = None

        # Feedback variables, all in radian
        self._joint_positions = np.zeros(self.num_dofs)
        self._joint_velocities = np.zeros(self.num_dofs)
        self._imu_angles = np.zeros(3)
        self._imu_quaternion = np.array([0.0, 0.0, 0.0, 1.0]) # [x, y, z, w]
        self._imu_angular_velocity = np.zeros(3)
        self._imu_linear_velocity = np.zeros(3)
        self._base_pos = np.array([0.0, 0.0, 0.9])

        self.self_check()

    def self_check(self):
        for _ in range(30):
            time.sleep(0.1)
            if self.unitree.self_check():
                print("UnitreeCppEnv self check passed!")
                break
        if not self.unitree.self_check():
            print("UnitreeCppEnv self check failed!")
            return False
        return True

    def update(self):
        self.robot_state = self.unitree.get_robot_state()

        if self.msg_type == "hg":
            self._joint_positions = np.asarray(
                [self.robot_state.motor_state.q[self.dof_idx[i]] for i in range(len(self.dof_idx))]
            )
            self._joint_velocities = np.asarray(
                [self.robot_state.motor_state.dq[self.dof_idx[i]] for i in range(len(self.dof_idx))]
            )
            self._joint_efforts = np.asarray(
                [self.robot_state.motor_state.tau_est[self.dof_idx[i]] for i in range(len(self.dof_idx))]
            )

            quat = np.asarray(self.robot_state.imu_state.quaternion)
            ang_vel = np.array(self.robot_state.imu_state.gyroscope, dtype=np.float32)

            self._imu_quaternion = quat[[1, 2, 3, 0]]
            self._imu_angular_velocity = ang_vel
            self._imu_angles = (
                self.robot_state.imu_state.rpy
            )

        elif self.msg_type == "go":
            raise NotImplementedError("msg_type 'go' not implemented in this example.")
        
        if self.enable_odometry:
            self.sport_state = self.unitree.get_sport_state()
            self._base_pos = np.asarray(self.sport_state.position)
            self._imu_linear_velocity = np.asarray(self.sport_state.velocity)

    def step(self, pd_target):
        assert len(pd_target) == self.num_dofs, "pd_target len should be num_dofs of env"
        self.unitree.step(pd_target.tolist())

    def shutdown(self):
        self.unitree.shutdown()

    def set_gains(self, stiffness, damping):
        self.unitree.set_gains(stiffness, damping)


if __name__ == "__main__":
    cfg = RobotConfig()
    env = UnitreeCppEnv(cfg)

    env.update()
    joint_pos_init = env._joint_positions.copy()
    joint_pos_default = np.array(env.default_pos)
    print("Initial joint pos: ", joint_pos_init)
    
    TOTAL_STEPS = 200
    for t in range(TOTAL_STEPS):
        alpha = t / TOTAL_STEPS
        joint_pos_target = (1 - alpha) * joint_pos_init + alpha * joint_pos_default
        env.step(joint_pos_target)
        env.update()
        print(f"{t}={env._joint_positions}")
        time.sleep(0.02)

    env.shutdown()
    print("Shutdown successfully!")

