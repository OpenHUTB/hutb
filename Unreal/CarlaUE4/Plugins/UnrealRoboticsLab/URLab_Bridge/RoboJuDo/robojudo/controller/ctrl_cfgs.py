from robojudo.config import ASSETS_DIR, Config


class CtrlCfg(Config):
    ctrl_type: str  # name of the controller class

    triggers: dict[str, str] = {}  # trigger conditions
    triggers_extra: dict[str, str] = {}  # extra trigger conditions


class KeyboardCtrlCfg(CtrlCfg):
    ctrl_type: str = "KeyboardCtrl"

    combination_init_buttons: list[str] = ["Key.ctrl_l"]
    """first button in combination, need to be held down to trigger other commands;"""

    triggers: dict[str, str] = {
        "Key.esc": "[SHUTDOWN]",
        # "Key.tab": "[POLICY_TOGGLE]",
        "`": "[SIM_REBORN]",
        "<": "[MOTION_FADE_IN]",  # note: with shift
        ">": "[MOTION_FADE_OUT]",  # note: with shift
        "|": "[MOTION_RESET]",  # note: with shift
        "{": "[MOTION_LOAD_PREV]",  # note: with shift
        "}": "[MOTION_LOAD_NEXT]",  # note: with shift
    }


class JoystickCtrlCfg(CtrlCfg):
    ctrl_type: str = "JoystickCtrl"

    combination_init_buttons: list[str] = ["LB", "RB"]
    """first button in combination, need to be held down to trigger other commands;"""

    # reference for button names in JoystickThread config
    triggers: dict[str, str] = {
        "A": "[SHUTDOWN]",
        "X": "[MOTION_FADE_IN]",
        "B": "[MOTION_FADE_OUT]",
        "Y": "[MOTION_RESET]",
        # "LB": "[MOTION_LOAD_PREV]",
        # "RB": "[MOTION_LOAD_NEXT]",
        # Note: combo keys supported: "LB+RB+A": "[TEST]",
    }


class UnitreeCtrlCfg(JoystickCtrlCfg):
    ctrl_type: str = "UnitreeCtrl"

    combination_init_buttons: list[str] = ["L1", "R1"]
    """first button in combination, need to be held down to trigger other commands;"""

    triggers: dict[str, str] = {
        "A": "[SHUTDOWN]",
        "X": "[MOTION_FADE_IN]",
        "B": "[MOTION_FADE_OUT]",
        "Y": "[MOTION_RESET]",
        # Note: combo keys supported: "L1+R1+A": "[TEST]",
    }


class MotionCtrlCfg(CtrlCfg):
    class PhcCfg(Config):
        robot_config_file: str
        robot_config: dict = {}  # PLACEHOLDER for phc robot config, to be parsed by config manager

        def model_post_init(self, context) -> None:
            import yaml

            from robojudo.config import THIRD_PARTY_DIR

            # parse phc configs
            phc_dir_path = THIRD_PARTY_DIR / "phc"
            phc_robot_config_file = self.robot_config_file
            phc_robot_config_file_path = phc_dir_path / "phc/data/cfg" / phc_robot_config_file
            if phc_robot_config_file_path.exists():
                phc_robot_config_dict = yaml.safe_load(phc_robot_config_file_path.open("r"))
                phc_robot_config_dict["asset"]["assetRoot"] = phc_dir_path.as_posix()
                phc_robot_config_dict["asset"]["assetFileName"] = (
                    phc_dir_path / phc_robot_config_dict["asset"]["assetFileName"]
                ).as_posix()
                # phc_robot_config_dict["asset"]["urdfFileName"] = (
                #     phc_dir_path / phc_robot_config_dict["asset"]["urdfFileName"]
                # ).as_posix()

                self.robot_config = phc_robot_config_dict

    ctrl_type: str = "MotionCtrl"

    motion_ctrl_gui: bool = True

    # ==== policy specific configs ====
    track_keypoints_names: list[str] = []
    phc: PhcCfg

    # ==== motion config ====
    robot: str
    motion_name: str = ""

    @property
    def motion_path(self) -> str:
        motion_path = ASSETS_DIR / f"motions/{self.robot}/phc/{self.motion_name}.pkl"
        return motion_path.as_posix()


class MotionH2HCtrlCfg(MotionCtrlCfg):
    ctrl_type: str = "MotionH2HCtrl"

    extra_motion_data: bool = False  # extra data for motion recognition


class MotionKungfuBotCtrlCfg(MotionCtrlCfg):
    ctrl_type: str = "MotionKungfuBotCtrl"

    future_max_steps: int = 95
    future_num_steps: int = 20

    anchor_index: int = 0  # root
    key_body_id: list[int]


class MotionTwistCtrlCfg(MotionCtrlCfg):
    ctrl_type: str = "MotionTwistCtrl"

    # ==== motion config ====
    robot: str


class BeyondMimicCtrlCfg(CtrlCfg):
    ctrl_type: str = "BeyondMimicCtrl"

    override_robot_anchor_pos: bool = False  # if True, drop pos fdb

    # ==== motion config ====
    robot: str
    motion_name: str

    @property
    def motion_path(self) -> str:
        motion_path = ASSETS_DIR / f"motions/{self.robot}/beyondmimic/{self.motion_name}.npz"
        return motion_path.as_posix()

    # ==== from beyondmimic ====
    class MotionCommandCfg(Config):
        """Configuration for the motion command."""

        anchor_body_name: str
        body_names: list[str]
        body_names_all: list[str]
        """from beyondmimic asset, used for indexing"""

    motion_cfg: MotionCommandCfg


class TwistRedisCtrlCfg(CtrlCfg):
    ctrl_type: str = "TwistRedisCtrl"

    redis_host: str = "localhost"
    redis_port: int = 6379
    redis_db: int = 0
    redis_key: str = "action_mimic_g1"  # key to get command data from redis

    buffer_size: int = 5  # size of the data buffer to store recent commands
