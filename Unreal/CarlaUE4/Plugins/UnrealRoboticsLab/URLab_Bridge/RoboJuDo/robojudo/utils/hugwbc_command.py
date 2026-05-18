from dataclasses import asdict, dataclass
from enum import Enum


class MotionMode(Enum):
    WALK = 0
    STAND = 1
    HOP = 2
    INTERRUPT = 3


@dataclass
class ControllData:
    lin_vel_x: float = 0.0  # yes
    lin_vel_y: float = 0.0  # yes
    ang_vel_yaw: float = 0.0  # yes

    gait_freq: float = 0.0  # yes
    gait_phase: float = 0.0
    gait_duration: float = 0.0
    foot_swing_height: float = 0.0  # yes
    body_height: float = 0.0  # yes
    body_pitch: float = 0.0  # yes
    waist_roll: float = 0.0  # yes
    interrupt: bool = False

    def asdict(self):
        return asdict(self)
