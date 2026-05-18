from robojudo.controller.ctrl_cfgs import MotionH2HCtrlCfg


class H1MotionH2HCtrlCfg(MotionH2HCtrlCfg):
    # ==== policy specific configs ====
    track_keypoints_names: list[str] = []
    phc: MotionH2HCtrlCfg.PhcCfg = MotionH2HCtrlCfg.PhcCfg(
        robot_config_file="robot/unitree_h1.yaml",
    )

    # ==== motion config ====
    robot: str = "h1"
    # PHC retargeted motion
    # motion_name: str = "dance_sample_h1" # Warning: special heading
    motion_name: str = "singles/0-KIT_572_punch_right01_poses"
