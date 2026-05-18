import json
import logging
import time
from collections import deque
from threading import Thread

import numpy as np
import redis  # TODO: add requirements
from redis.exceptions import RedisError

from robojudo.controller import Controller, ctrl_registry
from robojudo.controller.ctrl_cfgs import TwistRedisCtrlCfg

logger = logging.getLogger(__name__)


@ctrl_registry.register
class TwistRedisCtrl(Controller):
    cfg_ctrl: TwistRedisCtrlCfg

    def __init__(self, cfg_ctrl: TwistRedisCtrlCfg, env=None, device="cpu"):
        super().__init__(cfg_ctrl=cfg_ctrl, env=env, device=device)

        self.redis_host = self.cfg_ctrl.redis_host
        self.redis_port = self.cfg_ctrl.redis_port
        self.redis_db = self.cfg_ctrl.redis_db
        self.redis_key = self.cfg_ctrl.redis_key
        self.buffer_size = self.cfg_ctrl.buffer_size

        self.data_buffer = deque(maxlen=self.buffer_size)
        self._stop = False
        self.redis_thread = Thread(target=self.redis_worker, daemon=True)
        self.redis_thread.start()

        self.last_data = None

        # wait for first data
        while self.last_data is None:
            self.get_data()
            time.sleep(0.01)
        logger.info("[TwistRedisCtrl] Initialized with first data.")

    def reset(self):
        # reset buffer
        self.data_buffer.clear()

    def redis_worker(self):
        """worker: try connect, subscribe or poll, auto-reconnect"""
        redis_client = None
        while not self._stop:
            if redis_client is None:
                redis_client = self._connect_redis()

            try:
                # # send proprio to redis
                # proprio_json = json.dumps(obs_proprio.tolist())
                # self.redis_client.set("state_body_g1", proprio_json)

                # receive mimic_obs from redis
                action_mimic_json = redis_client.get(self.redis_key)  # pyright: ignore[reportOptionalMemberAccess]
            except RedisError as e:
                logger.warning(f"[Redis] Lost connection: {e}, retrying...")
                redis_client = None
                continue

            if action_mimic_json is not None:
                action_mimic_list = json.loads(action_mimic_json)  # pyright: ignore[reportArgumentType]
                action_mimic = np.array(action_mimic_list, dtype=np.float32)
                self.data_buffer.append(action_mimic)

            time.sleep(0.01)

    def _connect_redis(self):
        while not self._stop:
            try:
                redis_client = redis.Redis(
                    host=self.redis_host,
                    port=self.redis_port,
                    db=self.redis_db,
                    socket_timeout=1,
                    socket_connect_timeout=1,
                )
                redis_client.ping()
                logger.info("[Redis] Connected")
                return redis_client
            except Exception as e:
                logger.error(f"[Redis] Connect failed: {e}, retrying...")
                time.sleep(0.5)

    def get_data(self):
        """pop oldeset data from buffer to latest data"""
        if len(self.data_buffer) > 0:
            data = self.data_buffer.popleft()
            self.last_data = data
        else:
            data = self.last_data

        return {
            "action_mimic": data,
        }


if __name__ == "__main__":
    twist_redis_ctrl = TwistRedisCtrl(
        cfg_ctrl=TwistRedisCtrlCfg(),
        env=None,
    )
    for _ in range(10000):
        ctrl_data = twist_redis_ctrl.get_data()
        print(ctrl_data)
        print("================================")
        time.sleep(0.3)
    exit()
