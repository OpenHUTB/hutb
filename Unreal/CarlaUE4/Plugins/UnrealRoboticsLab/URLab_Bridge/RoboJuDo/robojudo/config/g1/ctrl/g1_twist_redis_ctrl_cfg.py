from robojudo.controller.ctrl_cfgs import TwistRedisCtrlCfg


class G1TwistRedisCtrlCfg(TwistRedisCtrlCfg):
    redis_host: str = "localhost"
    redis_key: str = "action_mimic_g1"  # key to get command data from redis

    buffer_size: int = 5  # size of the data buffer to store recent commands
