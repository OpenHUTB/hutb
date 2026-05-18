from abc import ABC, abstractmethod

from robojudo.environment import Environment

from .ctrl_cfgs import CtrlCfg


class Controller(ABC):
    """
    Base Controller Module
    """

    def __init__(self, cfg_ctrl: CtrlCfg, env: Environment | None = None, device: str = "cpu"):
        self.cfg_ctrl = cfg_ctrl
        self.env = env  # type: ignore
        self.device = device

        self.triggers: dict = cfg_ctrl.triggers
        self.triggers.update(cfg_ctrl.triggers_extra)  # merge extra triggers

    @abstractmethod
    def get_data(self):
        raise NotImplementedError

    @abstractmethod
    def reset(self):
        pass

    # @abstractmethod # TODO
    def post_step_callback(self, commands: list[str] | None = None):
        return

    # @abstractmethod
    def process_triggers(self, ctrl_data):
        commands = []  # TODO
        if len(self.triggers) == 0:
            return ctrl_data, commands
        return ctrl_data, commands


class ControllerHook(Controller):
    """
    Base Controller Hook Module
    """

    def __init__(self, cfg_ctrl=None, env=None, device="cpu"):
        super().__init__(cfg_ctrl=cfg_ctrl, env=env, device=device)

    @abstractmethod
    def get_data_with_hook(self, prior_ctrl_data: dict, env_data: dict):
        raise NotImplementedError

    def get_data(self):
        raise NotImplementedError("Use get_data_with_hook instead of get_data in ControllerHook")
