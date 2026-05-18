import time
from queue import Queue
from threading import Thread

from pynput import keyboard  # TODO: fix DISPLAY error on Linux without GUI


class KeyboardThread(Thread):
    def __init__(self, event_queue: Queue):
        super().__init__(name="KeyboardThread", daemon=True)
        self.event_queue = event_queue

    def run(self):
        def on_press(key):
            key_name = self.get_key_name(key)
            event = {
                "type": "keyboard",
                "name": key_name,
                "pressed": True,
                "timestamp": time.time(),
            }
            self.event_queue.put(event)

        def on_release(key):
            key_name = self.get_key_name(key)
            event = {
                "type": "keyboard",
                "name": key_name,
                "pressed": False,
                "timestamp": time.time(),
            }
            self.event_queue.put(event)

        with keyboard.Listener(on_press=on_press, on_release=on_release) as listener:
            listener.join()

    def get_key_name(self, key):
        try:
            return key.char if key.char is not None else str(key)
        except AttributeError:
            return str(key)


if __name__ == "__main__":
    event_queue = Queue()
    kb_thread = KeyboardThread(event_queue)
    kb_thread.start()

    print("Press keys (ESC to exit)...")
    while True:
        event = event_queue.get()
        print(event)
        if event["name"] == "Key.esc" and event["pressed"]:
            break
