# Copyright (c) 2026 Jonathan Embley-Riches. All rights reserved.
# Licensed under the Apache License, Version 2.0.

"""
Registry of available RoboJuDo policies for the GUI dropdown.
Each entry defines the import path, DOF requirement, and description.
"""

POLICIES = {
    "unitree_12dof": {
        "label": "Unitree Locomotion (12 DOF)",
        "policy_cfg": "robojudo.config.g1.policy.g1_unitree_policy_cfg.G1UnitreePolicyCfg",
        "env_cfg": "urlab_policy.env_config.G1UnrealEnvCfg",
        "dofs": 12,
        "xml": "g1_12dof",
        "desc": "Basic walking — WASD twist control",
        "ctrl_type": "twist",
    },
    "unitree_wo_gait": {
        "label": "Unitree Full Body (29 DOF)",
        "policy_cfg": "robojudo.config.g1.policy.g1_unitree_policy_cfg.G1UnitreeWoGaitPolicyCfg",
        "env_cfg": "urlab_policy.env_config.G1_29UnrealEnvCfg",
        "dofs": 29,
        "xml": "g1_29dof",
        "desc": "Full body walking without gait clock",
        "ctrl_type": "twist",
    },
    "smooth": {
        "label": "Smooth Locomotion (29 DOF)",
        "policy_cfg": "robojudo.config.g1.policy.g1_smooth_policy_cfg.G1SmoothPolicyCfg",
        "env_cfg": "urlab_policy.env_config.G1_29UnrealEnvCfg",
        "dofs": 29,
        "xml": "g1_29dof",
        "desc": "Smoother walking policy",
        "ctrl_type": "twist",
    },
    "beyondmimic_dance": {
        "label": "BeyondMimic Dance (29 DOF) [PHC]",
        "policy_cfg": "robojudo.config.g1.policy.g1_beyondmimic_policy_cfg.G1BeyondMimicPolicyCfg",
        "env_cfg": "urlab_policy.env_config.G1_29UnrealEnvCfg",
        "dofs": 29,
        "xml": "g1_29dof",
        "desc": "Motion imitation — dance. Requires PHC submodule",
        "ctrl_type": "motion",
        "requires_phc": True,
    },
    "h2h": {
        "label": "Human2Humanoid (21 DOF) [PHC]",
        "policy_cfg": "robojudo.config.g1.policy.g1_h2h_policy_cfg.G1H2HPolicyCfg",
        "env_cfg": "urlab_policy.env_config.G1_29UnrealEnvCfg",
        "dofs": 21,
        "xml": "g1_29dof",
        "desc": "Human motion retargeting. Requires PHC submodule",
        "ctrl_type": "motion_h2h",
        "requires_phc": True,
    },
    "amo": {
        "label": "AMO Locomotion (29 DOF) [PHC]",
        "policy_cfg": "robojudo.config.g1.policy.g1_amo_policy_cfg.G1AmoPolicyCfg",
        "env_cfg": "urlab_policy.env_config.G1_29UnrealEnvCfg",
        "dofs": 29,
        "xml": "g1_29dof",
        "desc": "Adaptive motion optimization. Requires PHC submodule",
        "ctrl_type": "twist",
        "requires_phc": True,
    },
    "twist_tracker": {
        "label": "Twist General Motion (12 DOF) [PHC]",
        "policy_cfg": "robojudo.config.g1.policy.g1_twist_policy_cfg.G1TwistPolicyCfg",
        "env_cfg": "urlab_policy.env_config.G1UnrealEnvCfg",
        "dofs": 12,
        "xml": "g1_12dof",
        "desc": "Motion tracker with twist. Requires PHC submodule",
        "ctrl_type": "motion_twist",
        "requires_phc": True,
    },
    # ─── Go2 Quadruped ───
    "go2_wtw": {
        "label": "Go2 Walk-These-Ways (12 DOF)",
        "policy_cfg": "robojudo.config.go2.policy.go2_wtw_policy_cfg.Go2WtwPolicyCfg",
        "env_cfg": "urlab_policy.env_config.Go2UnrealEnvCfg",
        "dofs": 12,
        "xml": "go2",
        "desc": "Gait-conditioned agility — rough terrain locomotion (WASD twist)",
        "ctrl_type": "twist",
    },
}


def get_policy_labels():
    return {k: v["label"] for k, v in POLICIES.items()}


def import_class(dotted_path):
    """Import a class from a dotted path like 'module.submodule.ClassName'."""
    module_path, class_name = dotted_path.rsplit(".", 1)
    import importlib
    mod = importlib.import_module(module_path)
    return getattr(mod, class_name)
