import json
import os
import queue
import threading
import time
from pathlib import Path

import msgpack
import msgpack_numpy

from robojudo.config import Config

msgpack_numpy.patch()


class DebugCfg(Config):
    log_obs: bool = False
    """Warning, this is debug only, may generate large log files and slow down the system."""


class DebugLogger:
    """
    A recorder that logs environment and control data to a file using msgpack format.
    """

    def __init__(self, base_path="logs", run_cfg=None):
        timestamp = time.strftime("%y%m%d-%H%M%S")

        log_name = f"log_{timestamp}"
        if run_cfg is not None:
            cfg_name = run_cfg.__class__.__name__
            log_name += f"_{cfg_name}"

        self.log_path = Path(base_path) / log_name
        os.makedirs(self.log_path, exist_ok=True)
        self.log_file = self.log_path / "log.msgpack"

        # save run_cfg as json
        if run_cfg is not None:
            cfg_file = self.log_path / "config.json"
            with open(cfg_file, "w") as f:
                json.dump(run_cfg.to_dict(), f, indent=4)

        self._queue = queue.Queue()
        self._thread = threading.Thread(target=self._writer_thread, daemon=True)
        self._stop_event = threading.Event()
        self._thread.start()

    def log(self, env_data, ctrl_data, extras, pd_target, timestep):
        self._queue.put(
            {
                "env_data": env_data,
                "ctrl_data": ctrl_data,
                "extras": extras,
                "pd_target": pd_target,
                "timestep": timestep,
                "time": time.time(),
            }
        )

    def _writer_thread(self):
        with open(self.log_file, "ab") as f:
            f.write(b"")  # create the file if not exists
            while not self._stop_event.is_set() or not self._queue.empty():
                try:
                    log_item = self._queue.get(timeout=0.1)
                except queue.Empty:
                    continue

                msgpacked = msgpack.packb(log_item, use_bin_type=True)
                if msgpacked:
                    f.write(msgpacked)

                f.flush()
                os.fsync(f.fileno())
                self._queue.task_done()

    def close(self):
        try:
            self._stop_event.set()
            self._thread.join()
        except Exception:
            # print(f"Error closing DebugLogger: {e}")
            pass
