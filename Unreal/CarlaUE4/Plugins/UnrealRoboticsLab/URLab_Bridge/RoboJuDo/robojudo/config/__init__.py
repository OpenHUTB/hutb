from robojudo.utils.module_registry import Registry

from .config_class import Config
from .global_path import ASSETS_DIR, ROOT_DIR, THIRD_PARTY_DIR  # noqa: F401

cfg_registry = Registry(package="robojudo.control", base_class=Config)

__all__ = [
    "ROOT_DIR",
    "ASSETS_DIR",
    "THIRD_PARTY_DIR",
    "cfg_registry",
]


def __getattr__(name: str) -> type[Config]:
    try:
        cfg_class = cfg_registry.get(name)
    except Exception as e:
        raise AttributeError(f"module {__name__} has no attribute {name}") from e
    print(f"[Config] Dynamic import of config: {name}")
    globals()[name] = cfg_class
    return cfg_class


# ===== import to register configs =====
import robojudo.config.g1  # noqa: E402, F401
import robojudo.config.h1  # noqa: E402, F401
# print("Available configs:", cfg_registry.registered_modules.keys())
