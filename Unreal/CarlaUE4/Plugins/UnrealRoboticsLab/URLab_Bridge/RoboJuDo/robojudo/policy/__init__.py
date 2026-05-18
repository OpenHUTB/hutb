from robojudo.utils.module_registry import Registry

from .base_policy import Policy
from .policy_cfgs import PolicyCfg

policy_registry = Registry(package="robojudo.policy", base_class=Policy)

__all__ = [
    "Policy",
    "PolicyCfg",
    "policy_registry",
]


def __getattr__(name: str) -> type[Policy]:
    try:
        policy_class = policy_registry.get(name)
    except Exception as e:
        raise AttributeError(f"module {__name__} has no attribute {name}") from e
    print(f"[Policy] Dynamic import of policy: {name}")
    globals()[name] = policy_class
    return policy_class


# ===== Declare all your custom environments here =====
policy_registry.add("UnitreePolicy", ".unitree_policy")
policy_registry.add("UnitreeWoGaitPolicy", ".unitree_policy")
policy_registry.add("SmoothPolicy", ".smooth_policy")
policy_registry.add("H2HStudentPolicy", ".h2h_student_policy")
policy_registry.add("AMOPolicy", ".amo_policy")
# policy_registry.add("GMTPolicy", ".gmt_policy")
# policy_registry.add("HugWbcPolicy", ".hugwbc_policy")
policy_registry.add("BeyondMimicPolicy", ".beyondmimic_policy")
policy_registry.add("AsapPolicy", ".asap_policy")
policy_registry.add("AsapLocoPolicy", ".asap_policy")
policy_registry.add("KungfuBotGeneralPolicy", ".kungfubot_policy")
policy_registry.add("TwistPolicy", ".twist_policy")
policy_registry.add("ProtoMotionsTrackerPolicy", ".protomotions_tracker_policy")
