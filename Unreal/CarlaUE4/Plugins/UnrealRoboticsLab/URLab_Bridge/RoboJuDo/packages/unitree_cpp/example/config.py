from typing import List
from pydantic import BaseModel


class Config(BaseModel):
    def to_dict(self):
        return self.model_dump()

class UnitreeConfig(Config):
    net_if: str = "eth0"
    control_dt: float = 0.02

    msg_type: str = "hg"    # "hg" or "go"
    control_mode: str = "position"
    hand_type: str = "NONE"  # "Dex-3", or "NONE"

    lowcmd_topic: str = "rt/lowcmd"
    lowstate_topic: str = "rt/lowstate"

    enable_odometry: bool = True
    sport_state_topic: str = "rt/odommodestate"

# Config for G1 robot
class RobotConfig(Config):
    unitree: UnitreeConfig = UnitreeConfig()

    odometry_type:str = "UNITREE"


    joint2motor_idx: List[int] = [0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 
                  15, 16, 17, 18, 19, 20, 21, 
                  22, 23, 24, 25, 26, 27, 28]
    
    num_dofs: int = 29
    joint_names: List[str] = [
        "left_hip_pitch_joint", "left_hip_roll_joint", "left_hip_yaw_joint", "left_knee_joint", "left_ankle_pitch_joint", "left_ankle_roll_joint", 
        "right_hip_pitch_joint", "right_hip_roll_joint", "right_hip_yaw_joint", "right_knee_joint", "right_ankle_pitch_joint", "right_ankle_roll_joint", 
        "waist_yaw_joint", "waist_roll_joint", "waist_pitch_joint",
        "left_shoulder_pitch_joint", "left_shoulder_roll_joint", "left_shoulder_yaw_joint", "left_elbow_joint", "left_wrist_roll_joint", "left_wrist_pitch_joint", "left_wrist_yaw_joint",
        "right_shoulder_pitch_joint", "right_shoulder_roll_joint", "right_shoulder_yaw_joint", "right_elbow_joint", "right_wrist_roll_joint", "right_wrist_pitch_joint", "right_wrist_yaw_joint"
    ]
    default_pos: List[float] = [
        -0.1,  0.0,  0.0,  0.3, -0.2, 0.0, 
        -0.1,  0.0,  0.0,  0.3, -0.2, 0.0,
        0, 0, 0,
        0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0
    ]

    stiffness: List[float] = [
        100, 100, 100, 150, 40, 40,
        100, 100, 100, 150, 40, 40,
        200, 200, 200,
        40, 40, 40, 40, 20, 20, 20,
        40, 40, 40, 40, 20, 20, 20
    ]

    damping: List[float] = [
        5, 5, 5, 5, 2, 2,
        5, 5, 5, 5, 2, 2,
        6, 6, 6,
        2, 2, 2, 2, 2, 2, 2,
        2, 2, 2, 2, 2, 2, 2
    ]
