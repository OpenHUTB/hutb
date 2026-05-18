import importlib
from typing import Any

from robojudo.pipeline.pipeline_cfgs import PipelineCfg

# TODO: not ready yet

CFG_REGISTRY: dict[str, tuple[str, str]] = {
    # env
    "g1_mujoco_env": (".env.g1_mujuco_env_cfg", "G1MujocoEnvCfg"),
    "g1_23_mujoco_env": (".env.g1_mujuco_env_cfg", "G1_23MujocoEnvCfg"),
    "g1_dummy_env": (".env.g1_dummy_env_cfg", "G1DummyEnvCfg"),
    "g1_real_env": (".env.g1_real_env_cfg", "G1RealEnvCfg"),
    # policy
    "g1_unitree_policy": (".policy.g1_unitree_policy_cfg", "G1UnitreePolicyCfg"),
    "g1_smooth_policy": (".policy.g1_smooth_policy_cfg", "G1SmoothPolicyCfg"),
    "g1_amo_policy": (".policy.g1_amo_policy_cfg", "G1AmoPolicyCfg"),
    "g1_beyondmimic_policy": (".policy.g1_beyondmimic_policy_cfg", "G1BeyondMimicPolicyCfg"),
    # ctrl
    "keyboard_ctrl": ("robojudo.controller.keyboard_ctrl", "KeyboardCtrlCfg"),
    "joystick_ctrl": ("robojudo.controller.joystick_ctrl", "JoystickCtrlCfg"),
    "unitree_ctrl": ("robojudo.controller.unitree_ctrl", "UnitreeCtrlCfg"),
    "beyondmimic_ctrl": ("robojudo.controller.beyondmimic_ctrl", "BeyondMimicCtrlCfg"),
    # pipeline
    "rl_pipeline": ("robojudo.pipeline.rl_pipeline", "RlPipelineCfg"),
    "rl_multi_policy_pipeline": ("robojudo.pipeline.rl_multi_policy_pipeline", "RlMultiPolicyPipelineCfg"),
}


def load_class(key: str) -> Any:
    module, cls_name = CFG_REGISTRY[key]
    return getattr(importlib.import_module(module, package="robojudo.config.g1"), cls_name)


def make_pipeline_cfg(pipeline_id: str, pipeline_cfg: dict, pipeline_cfg_abs: dict[str, str | list]) -> PipelineCfg:
    def parse_cfg_abs(cfg_abs: str | list) -> tuple[Any, Any]:
        if isinstance(cfg_abs, str):
            cls = load_class(cfg_abs)
            return cls(), cls
        elif isinstance(cfg_abs, list):
            if not cfg_abs:
                return [], list[Any]

            inst_list = []
            class_list = []
            for v in cfg_abs:
                inst, ann = parse_cfg_abs(v)
                inst_list.append(inst)
                class_list.append(ann)
            ann_list = list[tuple(class_list)]
            return inst_list, ann_list
        else:
            raise TypeError(f"Unsupported type in cfg_abs: {type(cfg_abs)}")

    pipeline_class = load_class(pipeline_id)
    namespace = {"__annotations__": {}}
    for cfg_key, cfg_class_id in pipeline_cfg_abs.items():
        inst, ann = parse_cfg_abs(cfg_class_id)
        namespace[cfg_key] = inst
        namespace["__annotations__"][cfg_key] = ann

    for cfg_key, cfg_value in pipeline_cfg.items():
        namespace[cfg_key] = cfg_value
        namespace["__annotations__"][cfg_key] = type(cfg_value)

    pipeline_cfg_class = type("DynamicPipelineCfg", (pipeline_class,), namespace)
    pipeline_cfg = pipeline_cfg_class()

    return pipeline_cfg


if __name__ == "__main__":
    from pprint import pprint

    pipeline_id = "rl_pipeline"
    pipeline_cfg = {
        "robot": "g1",
        "test": {
            "main": "keyboard_ctrl",
            "secondary": "joystick_ctrl",
        },
    }

    pipeline_cfg_abs = {
        "env": "g1_mujoco_env",
        "policy": "g1_amo_policy",
        "ctrl": ["keyboard_ctrl", "unitree_ctrl"],
    }
    cfg = make_pipeline_cfg(pipeline_id, pipeline_cfg, pipeline_cfg_abs)
    pprint(cfg)
