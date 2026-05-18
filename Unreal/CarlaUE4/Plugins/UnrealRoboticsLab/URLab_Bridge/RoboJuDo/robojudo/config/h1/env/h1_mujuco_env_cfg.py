from robojudo.environment.env_cfgs import MujocoEnvCfg

from .h1_env_cfg import H1EnvCfg


class H1MujocoEnvCfg(H1EnvCfg, MujocoEnvCfg):
    env_type: str = MujocoEnvCfg.model_fields["env_type"].default
    is_sim: bool = MujocoEnvCfg.model_fields["is_sim"].default
    # ====== ENV CONFIGURATION ======
    pass
