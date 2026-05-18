from robojudo.utils.module_registry import Registry

from .base_ctrl import Controller, ControllerHook
from .ctrl_cfgs import CtrlCfg
from .ctrl_manager import CtrlManager

ctrl_registry = Registry(package="robojudo.controller", base_class=Controller)

__all__ = [
    "Controller",
    "ControllerHook",
    "CtrlCfg",
    "CtrlManager",
    "ctrl_registry",
]


def __getattr__(name: str) -> type[Controller]:
    try:
        ctrl_class = ctrl_registry.get(name)
    except Exception as e:
        raise AttributeError(f"module {__name__} has no attribute {name}") from e
    print(f"[Controller] Dynamic import of controller: {name}")
    globals()[name] = ctrl_class
    return ctrl_class


# ===== Declare all your custom controllers here =====
ctrl_registry.add("JoystickCtrl", ".joystick_ctrl")
ctrl_registry.add("UnitreeCtrl", ".unitree_ctrl")
ctrl_registry.add("KeyboardCtrl", ".keyboard_ctrl")
ctrl_registry.add("BeyondMimicCtrl", ".beyondmimic_ctrl")
ctrl_registry.add("MotionCtrl", ".motion_ctrl")
ctrl_registry.add("MotionH2HCtrl", ".motion_h2h_ctrl")
ctrl_registry.add("MotionKungfuBotCtrl", ".motion_kungfubot_ctrl")
ctrl_registry.add("MotionTwistCtrl", ".motion_twist_ctrl")
ctrl_registry.add("TwistRedisCtrl", ".twist_redis_ctrl")
