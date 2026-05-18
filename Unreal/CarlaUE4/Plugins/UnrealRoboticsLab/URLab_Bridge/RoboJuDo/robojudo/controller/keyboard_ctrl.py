import time
from queue import Empty, Queue

from robojudo.controller import Controller, ctrl_registry
from robojudo.controller.ctrl_cfgs import KeyboardCtrlCfg
from robojudo.controller.utils.keyboard import KeyboardThread


@ctrl_registry.register
class KeyboardCtrl(Controller):
    cfg_ctrl: KeyboardCtrlCfg

    def __init__(self, cfg_ctrl: KeyboardCtrlCfg, env=None, **kwargs):  # TODO
        super().__init__(cfg_ctrl=cfg_ctrl, env=env, **kwargs)

        self.event_queue = Queue(maxsize=100)
        self.keyboard_thread = KeyboardThread(self.event_queue)
        self.keyboard_thread.start()

        self.reset()

    def reset(self):
        while not self.event_queue.empty():
            try:
                self.event_queue.get_nowait()
            except Empty:
                break

    def get_events(self):
        events = []
        while not self.event_queue.empty():
            try:
                event = self.event_queue.get_nowait()
                events.append(event)
            except Empty:
                break
        return events

    def get_data(self):
        return {"keyboard_event": self.get_events()}

    def process_triggers(self, ctrl_data):
        commands = []
        if len(self.triggers) == 0:
            return ctrl_data, commands

        for event in ctrl_data["keyboard_event"]:
            if event["type"] == "keyboard" and not event["pressed"]:  # trigger when key is released
                command = self.triggers.get(event["name"], None)
                if command is not None:
                    commands.append(command)
                    # remove event after triggered
                    ctrl_data["keyboard_event"].remove(event)

        return ctrl_data, commands


if __name__ == "__main__":
    kb_ctrl = KeyboardCtrl(
        cfg_ctrl=KeyboardCtrlCfg(
            triggers={
                "Key.space": "[TEST]",
                "\x01": "[CTRL_A]",
            }
        )
    )
    while True:
        data = kb_ctrl.get_data()
        ctrl_data, commands = kb_ctrl.process_triggers(data)
        if ctrl_data["keyboard_event"]:
            for e in ctrl_data["keyboard_event"]:
                print(e)
        if commands:
            print("Commands:", commands)
        time.sleep(0.1)
