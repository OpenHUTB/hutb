from robojudo.config import ASSETS_DIR
from robojudo.controller.ctrl_cfgs import MotionCtrlCfg, MotionH2HCtrlCfg, MotionKungfuBotCtrlCfg, MotionTwistCtrlCfg


class G1MotionCtrlCfg(MotionCtrlCfg):
    # ==== policy specific configs ====
    track_keypoints_names: list[str] = []
    phc: MotionCtrlCfg.PhcCfg = MotionCtrlCfg.PhcCfg(
        robot_config_file="robot/unitree_g1_29dof.yaml",
    )

    # ==== motion config ====
    robot: str = "g1"
    # PHC retargeted motion
    # motion_name: str = "dance_sample_g1"
    # motion_name: str = "singles/0-KIT_572_punch_right01_poses"
    motion_name: str = "singles/0-KIT_6_WalkInCounterClockwiseCircle05_1_poses"
    # motion_name: str = "singles/0-Transitions_mocap_mazen_c3d_dance_stand_poses"
    # motion_name: str = "amass_all_phc"

    @property
    def motion_path(self) -> str:
        motion_path = ASSETS_DIR / f"motions/{self.robot}/phc_29/{self.motion_name}.pkl"
        return motion_path.as_posix()


class G1MotionH2HCtrlCfg(G1MotionCtrlCfg, MotionH2HCtrlCfg):
    ctrl_type: str = "MotionH2HCtrl"
    extra_motion_data: bool = False  # extra data for motion recognition


class G1MotionKungfuBotCtrlCfg(G1MotionCtrlCfg, MotionKungfuBotCtrlCfg):
    ctrl_type: str = "MotionKungfuBotCtrl"

    phc: MotionCtrlCfg.PhcCfg = MotionCtrlCfg.PhcCfg(
        robot_config_file="robot/unitree_g1_23dof_KungfuBot.yaml",
    )

    # motion_name: str = "singles/0-KIT_6_WalkInCounterClockwiseCircle05_1_poses"
    motion_name: str = "kungfubot/Horse-stance_pose"

    @property
    def motion_path(self) -> str:
        motion_path = ASSETS_DIR / f"motions/{self.robot}/phc/{self.motion_name}.pkl"
        return motion_path.as_posix()

    key_body_id: list[int] = [4, 6, 10, 12, 19, 23, 24, 25, 26]


class G1MotionTwistCtrlCfg(G1MotionCtrlCfg, MotionTwistCtrlCfg):
    ctrl_type: str = "MotionTwistCtrl"

    phc: MotionCtrlCfg.PhcCfg = MotionCtrlCfg.PhcCfg(
        robot_config_file="robot/unitree_g1_23dof_KungfuBot.yaml",
    )

    # motion_name: str = "singles/0-KIT_6_WalkInCounterClockwiseCircle05_1_poses"
    motion_name: str = "kungfubot/Horse-stance_pose"

    @property
    def motion_path(self) -> str:
        motion_path = ASSETS_DIR / f"motions/{self.robot}/phc/{self.motion_name}.pkl"
        return motion_path.as_posix()
