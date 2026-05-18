import time
import tkinter as tk
from threading import Thread
from tkinter import messagebox


class MotionGUI:
    def __init__(self, motion_ctrl):
        self.motion_ctrl = motion_ctrl

        self.motion_name = "Motion Blank Name"
        self.motion_time = 0.0
        self.motion_length = 10.0

        self.thread_gui = Thread(target=self._run_gui, daemon=True)
        self.thread_gui.start()
        # time.sleep(1)

    def create_widgets(self):
        self.motion_name_label = tk.Label(self.root, text="Motion Name: ")
        self.motion_name_label.grid(row=0, column=0, padx=10, pady=5)
        self.motion_name_value = tk.Label(self.root, text=self.motion_name, wraplength=300, anchor="w", justify="left")
        self.motion_name_value.grid(row=0, column=1, padx=10, pady=5, sticky="w")

        self.motion_length_label = tk.Label(self.root, text="Motion Len: ")
        self.motion_length_label.grid(row=1, column=0, padx=10, pady=5)
        self.motion_length_value = tk.Label(self.root, text=f"{self.motion_length:.2f} s")
        self.motion_length_value.grid(row=1, column=1, padx=10, pady=5)

        self.motion_time_label = tk.Label(self.root, text="Motion Time: ")
        self.motion_time_label.grid(row=2, column=0, padx=10, pady=5)
        self.motion_time_value = tk.Label(self.root, text=f"{self.motion_time:.2f} s")
        self.motion_time_value.grid(row=2, column=1, padx=10, pady=5)

        self.motion_progress_label = tk.Label(self.root, text="Motion Progress: ")
        self.motion_progress_label.grid(row=3, column=0, padx=10, pady=5)
        self.motion_progress_value = tk.Label(self.root, text=f"{self.motion_time / self.motion_length * 100:.2f}%")
        self.motion_progress_value.grid(row=3, column=1, padx=10, pady=5)

        self.reset_button = tk.Button(self.root, text="Reset Progress", command=self.reset_motion)
        self.reset_button.grid(row=4, column=0, padx=10, pady=5)

        self.motion_input_label = tk.Label(self.root, text="Switch Motion ID：")
        self.motion_input_label.grid(row=5, column=0, padx=10, pady=5)
        self.motion_input = tk.Spinbox(self.root, from_=0, to=15000, increment=1)
        self.motion_input.grid(row=5, column=1, padx=10, pady=5)
        self.motion_input.bind("<Return>", lambda event: self.switch_motion())
        self.confirm_button = tk.Button(self.root, text="Confirm", command=self.switch_motion)
        self.confirm_button.grid(row=5, column=2, padx=10, pady=5)

        self.fade_in_button = tk.Button(self.root, text="Fade In", command=self.fade_in)
        self.fade_in_button.grid(row=6, column=0, padx=10, pady=10)

        self.fade_out_button = tk.Button(self.root, text="Fade Out", command=self.fade_out)
        self.fade_out_button.grid(row=6, column=1, padx=10, pady=10)

    def update_time(self, motion_time):
        self.motion_time = motion_time

    def update_info(self, motion_name, motion_length):
        self.motion_name = motion_name
        self.motion_length = motion_length

    def reset_motion(self):
        # self.motion_ctrl.reset()
        self.motion_ctrl.gui_commands.put("[MOTION_RESET]")

    def switch_motion(self):
        try:
            motion_id = int(self.motion_input.get())
            self.motion_ctrl.load_motion(motion_id)
        except ValueError:
            messagebox.showerror("Error", "Invalid ID")

    def fade_in(self):
        # self.motion_ctrl.fade_in()
        self.motion_ctrl.gui_commands.put("[MOTION_FADE_IN]")

    def fade_out(self):
        # self.motion_ctrl.fade_out()
        self.motion_ctrl.gui_commands.put("[MOTION_FADE_OUT]")

    def _run_gui(self):
        self.root = tk.Tk()
        self.root.title("Motion Control Panel")
        self.root.attributes("-topmost", True)
        self.create_widgets()
        self.gui_valid = True

        self.thread_gui_update = Thread(target=self._run_gui_update, daemon=True)
        self.thread_gui_update.start()

        self.root.mainloop()
        self.gui_valid = False
        print("MotionGUI closed")

    def _run_gui_update(self):
        while self.gui_valid:
            self.motion_time_value.config(text=f"{self.motion_time:.2f} s")
            self.motion_progress_value.config(text=f"{self.motion_time / self.motion_length * 100:.2f}%")

            self.motion_name_value.config(text=f"{self.motion_name}")
            self.motion_length_value.config(text=f"{self.motion_length:.2f} s")

            time.sleep(1 / 5)
