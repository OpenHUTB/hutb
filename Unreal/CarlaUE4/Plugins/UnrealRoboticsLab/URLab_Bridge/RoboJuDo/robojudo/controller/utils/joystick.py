import logging
import os
import struct
import time
from queue import Empty, Queue
from threading import Thread

import numpy as np

logger = logging.getLogger(__name__)

os.environ["PYGAME_HIDE_SUPPORT_PROMPT"] = "hide"


# TODO: axis post processing
class JoystickThread(Thread):
    def __init__(self, state_queue: Queue, event_queue: Queue):
        super().__init__(name="JoystickThread", daemon=True)
        self.state_queue = state_queue
        self.event_queue = event_queue

        self.config = self._init_config()

        self.running = True

    # fmt: off
    def _init_config(self):
        config = {
            "button_map": {
                0: "A", 1: "B", 2: "X", 3: "Y",
                4: "LB", 5: "RB", 6: "Back", 7: "Start",
                8: "Xbox", 9: "L", 10: "R",
            },
            "axis_config": {
                "axis_map": {
                    "LeftX": 0,
                    "LeftY": 1,
                    "RightX": 3,
                    "RightY": 4,
                    "LT": 2,
                    "RT": 5
                },
                "axis_range": {
                    "LT": [0, 1],
                    "RT": [0, 1]
                },
                "invert": ["LeftY", "RightY"],
            },
            "dpad_config": {
                "as_button_event": True,  # map dpad to button events
                "dpad_map": {
                    "Up": (1, 1),
                    "Right": (0, 1),
                    "Down": (1, -1),
                    "Left": (0, -1),
                }

            }
        }

        # if windows
        if os.name == 'nt':  # Windows
            config["button_map"].update({
                8: "L", 9: "R",
            })
            del config["button_map"][10]
            config["axis_config"]["axis_map"].update({
                "RightX": 2,
                "RightY": 3,
                "LT": 4,
            })
        return config
    # fmt: on

    @staticmethod
    def normalize_axis(axis_range, name, value):
        target_range = axis_range.get(name)
        if target_range:
            min_val, max_val = -1.0, 1.0  # SDL default range
            min_target, max_target = target_range
            value = (value - min_val) / (max_val - min_val) * (max_target - min_target) + min_target
        return round(value, 3)

    def run(self):
        import pygame

        pygame.init()
        if pygame.joystick.get_count() == 0:
            self.running = False
            logger.error("No joystick connected. Try to fix with: export SDL_JOYSTICK_DEVICE=/dev/input/js0")
            # raise RuntimeError("No joystick connected, try to fix with: export SDL_JOYSTICK_DEVICE=/dev/input/js0")
            return

        joystick = pygame.joystick.Joystick(0)
        joystick.init()

        name = joystick.get_name().lower()
        logger.info(f"[Joystick] Initialized: {name}")
        logger.info(
            f"[Joystick] Buttons: {joystick.get_numbuttons()}, \
                Axes: {joystick.get_numaxes()}, \
                Hats: {joystick.get_numhats()}"
        )

        button_map = self.config.get("button_map", {})
        axis_config = self.config.get("axis_config", {})

        dpad_config = self.config.get("dpad_config", {})
        dpad_as_button = dpad_config.get("as_button_event", True)
        dpad_state = {key: False for key in dpad_config.get("dpad_map", {}).keys()}

        axis_map = axis_config.get("axis_map", {})
        axis_range = axis_config.get("axis_range", {})
        invert = set(axis_config.get("invert", []))

        clock = pygame.time.Clock()
        last_state_time = time.time()
        state_interval = 1.0 / 100  # 100Hz

        while self.running:
            pygame.event.pump()
            now = time.time()

            # Poll events for buttons and DPad
            for event in pygame.event.get():
                if event.type == pygame.JOYBUTTONDOWN or event.type == pygame.JOYBUTTONUP:
                    btn_index = event.button
                    btn_name = button_map.get(btn_index, f"Button_{btn_index}")
                    self.event_queue.put(
                        {
                            "type": "button",
                            "name": btn_name,
                            "pressed": event.type == pygame.JOYBUTTONDOWN,
                            "timestamp": now,
                        }
                    )

                elif event.type == pygame.JOYHATMOTION:
                    if dpad_as_button:
                        dpad_state_new = {
                            name: event.value[axis] == direction
                            for name, (axis, direction) in dpad_config.get("dpad_map", {}).items()
                        }
                        for name, pressed in dpad_state_new.items():
                            if pressed != dpad_state[name]:
                                dpad_state[name] = pressed
                                self.event_queue.put(
                                    {
                                        "type": "button",
                                        "name": name,
                                        "pressed": pressed,
                                        "timestamp": now,
                                    }
                                )
                    else:
                        self.event_queue.put(
                            {
                                "type": "dpad",
                                "value": event.value,
                                "timestamp": now,
                            }
                        )

            # Axes update at fixed rate
            if now - last_state_time >= state_interval:
                axes_state = {}
                for name, index in axis_map.items():
                    val = joystick.get_axis(index)
                    if name in invert:
                        val = -val
                    val = self.normalize_axis(axis_range, name, val)
                    axes_state[name] = val

                while self.state_queue.full():
                    self.state_queue.get()
                self.state_queue.put(
                    {
                        "type": "axes",
                        "axes": axes_state,
                        "timestamp": now,
                    }
                )
                last_state_time = now

            clock.tick(500)  # avoid busy loop


# Modified From unitree_sdk2_python
class unitreeRemoteController:
    def __init__(self, state_queue, event_queue):
        self.state_queue = state_queue
        self.event_queue = event_queue

        # button
        self.button_map = [
            "R1",
            "L1",
            "Start",
            "Select",
            "R2",
            "L2",
            "F1",
            "F2",
            "A",
            "B",
            "X",
            "Y",
            "Up",
            "Right",
            "Down",
            "Left",
        ]
        self.last_button_state = np.zeros((16), dtype=bool)

    def parse(self, remoteData):
        now = time.time()
        # button
        keys = struct.unpack("H", remoteData[2:4])[0]
        button = [((keys & (1 << i)) >> i) for i in range(16)]
        button_state = np.array(button, dtype=bool)

        # Check for button state changes
        changed = button_state != self.last_button_state
        for i in range(16):
            if changed[i]:
                self.event_queue.put(
                    {
                        "type": "button",
                        "name": self.button_map[i],
                        "pressed": bool(button_state[i]),
                        "timestamp": now,
                    }
                )
        self.last_button_state = button_state.copy()

        # axis
        lx_offset = 4
        LeftX = struct.unpack("<f", remoteData[lx_offset : lx_offset + 4])[0]
        rx_offset = 8
        RightX = struct.unpack("<f", remoteData[rx_offset : rx_offset + 4])[0]
        ry_offset = 12
        RightY = struct.unpack("<f", remoteData[ry_offset : ry_offset + 4])[0]
        # L2_offset = 16
        # L2 = struct.unpack('<f', remoteData[L2_offset:L2_offset + 4])[0] # Placeholder，unused
        ly_offset = 20
        LeftY = struct.unpack("<f", remoteData[ly_offset : ly_offset + 4])[0]

        while self.state_queue.full():
            self.state_queue.get()
        self.state_queue.put(
            {
                "type": "axes",
                "axes": {
                    "LeftX": LeftX,
                    "LeftY": LeftY,
                    "RightX": RightX,
                    "RightY": RightY,
                },
                "timestamp": now,
            }
        )


if __name__ == "__main__":
    state_queue = Queue(maxsize=10)
    event_queue = Queue(maxsize=100)
    js_thread = JoystickThread(state_queue, event_queue)
    js_thread.start()

    print("Press joystick buttons (Ctrl+C to exit)...")
    try:
        while True:
            try:
                state = state_queue.get(timeout=1.0)
                print("State:", state)
            except Empty:
                pass

            while not event_queue.empty():
                try:
                    event = event_queue.get_nowait()
                    print("Event:", event)
                except Empty:
                    break
    except KeyboardInterrupt:
        print("Exiting...")
        js_thread.running = False
        js_thread.join()
