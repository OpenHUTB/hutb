import logging
import time
from multiprocessing import Queue

from robojudo.controller import Controller, ctrl_registry
from robojudo.controller.ctrl_cfgs import UnitreeCtrlCfg
from robojudo.controller.joystick_ctrl import JoystickCtrl
from robojudo.controller.utils.joystick import unitreeRemoteController

logger = logging.getLogger(__name__)


@ctrl_registry.register
class UnitreeCtrl(JoystickCtrl):
    cfg_ctrl: UnitreeCtrlCfg

    def __init__(self, cfg_ctrl: UnitreeCtrlCfg, env=None, device="cpu"):
        # Skip JoystickCtrl initialization
        Controller.__init__(self, cfg_ctrl=cfg_ctrl, env=env, device=device)
        self.unitree_env = env

        self.state_queue = Queue(maxsize=2)  # for axes
        self.event_queue = Queue(maxsize=100)  # for button/dpad events
        self.unitree_remote_controller = unitreeRemoteController(self.state_queue, self.event_queue)

        self.axes_names = ["LeftX", "LeftY", "RightX", "RightY"]
        self.reset()

        if self.unitree_env is not None:
            self.unitree_env.RemoteControllerHandler = self.unitree_remote_controller.parse
        else:
            logger.warning("No Unitree env, controller not working.")


if __name__ == "__main__":
    from robojudo.config.g1.env.g1_real_env_cfg import G1RealEnvCfg
    from robojudo.environment.unitree_cpp_env import UnitreeCppEnv

    env = UnitreeCppEnv(G1RealEnvCfg())
    ctrl = UnitreeCtrl(cfg_ctrl=UnitreeCtrlCfg(), env=env)

    while True:
        env.update()
        print(ctrl.get_data())
        print("================================")
        time.sleep(0.1)  # Simulate a control loop
