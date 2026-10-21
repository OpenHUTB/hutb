# Driving Study Launcher and Data Recording

## Quick Start

1. Connect the headset, enable eye tracking in Pimax Play, and complete calibration. When using a Logitech G29 / G920 / G923 racing wheel, confirm that Logitech G HUB detects the wheel and that steering and pedal axes respond in Windows `joy.cpl`, then close the test dialog.
2. Double-click `StartDrivingStudy.cmd` in the repository or package root.
3. Choose **Pimax Controller** or **Logitech Racing Wheel**. On a source checkout, enter the UE4 engine root directory; installed maps and ego vehicle profiles are discovered automatically from the project.
4. To record a trial, check **Record driving data**. The default target rate is **120 Hz** (set to **20 Hz** for lightweight classroom sessions) and enter or select a participant ID and output directory. Optional switches:
   - **Record viewport screenshots** (default **20 Hz**, 640x480 JPEG; optionally check **Async screenshots** so GPU readback and JPEG encoding do not block the game thread).
   - **120 Hz eye-tracking thread** (polls the eye tracker on a dedicated background thread and runs the online I-VT saccade detector at 120 Hz).
   - **Render quality downgrade** (explicitly sets `sg.ShadowQuality 0`, `sg.EffectsQuality 0`, `sg.PostProcessQuality 0`; off by default so normal launches preserve existing scalability settings).
5. Click **Check Settings**, then click **Start VR** once preflight succeeds. The launcher starts Pimax Play, SteamVR, and the simulator in order.
6. When driving with a racing wheel, press and release the throttle and brake pedals once before starting the trial to calibrate pedal travel. After exiting the simulator, click **Open Data Directory** to inspect the generated CSV files.

Recording is off by default. Each recorded run creates a new `<session>.csv` (71 columns) and `<session>_gaze.csv` (18 columns) without overwriting previous sessions. The default output root is `Unreal/CarlaUE4/Saved/DrivingStudy/` in a source build or `CarlaUE4/Saved/DrivingStudy/` in a packaged build; when a valid participant name is provided and recording or screenshots are enabled, files are placed under `<OutputDir>/<Participant>/`. Reloading the map or respawning the ego vehicle starts a new session file.

When the racing wheel input mode is selected, VR controllers and the keyboard no longer drive steering, throttle, brake, gear shift, or turn signals (controllers still adjust camera/seat pose). When the controller input mode is selected, racing wheel driving axes are ignored and standard keyboard shortcuts remain available. If the racing wheel disconnects or read fails during wheel mode, full brake is applied and reconnection is retried every 2 seconds.

If the wheel does not respond, inspect `Unreal/CarlaUE4/Saved/Logs/CarlaUE4.log`:
- `Logitech SDK initialize: success` indicates the DirectInput/Logitech SDK initialized.
- `Found Logitech wheel` indicates a supported wheel device was opened.
- `Logitech wheel sample` logs raw steering and pedal values every 2 seconds.

Command-line launch is also supported:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\StartPimaxVR.ps1 -UE4Root '<UE4 source engine root>' -Map /Game/Carla/Maps/Town10HD_Opt -InputDevice wheel -RecordDriving -RecordHz 120 -EyeThreadHz 120
```

- Use `-InputDevice controller` for VR controller mode; omit `-RecordDriving` when recording is not needed.
- Pass `-RecordHz 20` for lightweight 20 Hz logging, or `-RecordDirectory <local absolute path>` to customize the output directory.
- Optional flags: `-ScreenshotDriving -ScreenshotHz 20` (also honors `-RecordDirectory` when used standalone), `-AsyncScreenshot`, `-EyeThreadHz 120`, and `-PerfDowngrade`.

Building from source requires `LogitechWheelPlugin` and `SteamVR` enabled in `CarlaUE4.uproject`.

## CSV Schema

### 1. Primary Driving Table `<session>.csv` (71 Columns)

Each row represents one game-thread sample combining current eye-tracking state, focus raycast hit, head pose, online saccade metrics, vehicle kinematics, control inputs, OpenDRIVE road location, and affecting traffic light state.

| Column | Description |
| --- | --- |
| `schema_version` | CSV schema version (`2`) |
| `ue_frame`, `sim_time_s`, `host_elapsed_s` | Unreal frame number, simulation timestamp (s), wall-clock elapsed time since recording started (s) |
| `eye_device_timestamp_ms` | Eye-tracker device timestamp (ms); uses the device clock rather than simulation time |
| `input_mode`, `wheel_connected`, `autopilot` | Selected input mode (`controller` / `wheel`), wheel connection state, autopilot state |
| `map`, `vehicle_id` | Map name and ego vehicle CARLA actor ID |
| `road_id`, `lane_id`, `road_s_m` | OpenDRIVE road ID, lane ID, and longitudinal `s` coordinate (m) |
| `junction_id`, `in_junction` | OpenDRIVE junction ID and junction flag (`1`/`0`); empty if off-road |
| `motion`, `yaw_rate_deg_s` | Motion label (`stopped`, `straight`, `left`, `right`; first row `unknown`) and yaw rate (deg/s) |
| `red_light_stop_candidate` | `1` when the affecting traffic light is `red` and vehicle speed is below 1 km/h (filter helper, not causal proof) |
| `traffic_light_id`, `traffic_light_state`, `traffic_light_distance_m` | Affecting traffic light ID, state, and pole distance (m); state may be `unknown` outside the light's trigger box |
| `vehicle_x_m`, `vehicle_y_m`, `vehicle_z_m`, `vehicle_yaw_deg`, `speed_kmh` | Ego vehicle world position (m), heading (deg), and speed (km/h) |
| `steer_input`, `throttle_input`, `brake_input` | Current frame control inputs (`steer` in `[-1, 1]`, `throttle`/`brake` in `[0, 1]`, after project input scaling) |
| `previous_applied_steer`, `previous_applied_throttle`, `previous_applied_brake`, `previous_reverse` | Control values and reverse gear state already applied on the vehicle before the current frame's input is flushed |
| `eye_source`, `gaze_valid` | Eye-tracker backend (`pimax_pvr`, `sranipal`, `dummy`, etc.) and validity flag (`1`/`0`; `dummy` is never marked valid) |
| `gaze_origin_x_m`, `gaze_origin_y_m`, `gaze_origin_z_m`, `gaze_dir_x`, `gaze_dir_y`, `gaze_dir_z` | World-space gaze ray origin (m) and unit direction vector |
| `gaze_local_x`, `gaze_local_y`, `gaze_local_z` | Camera-local gaze unit direction vector (separates eye rotation from head rotation) |
| `focus_hit`, `focus_actor_id`, `focus_actor_name`, `focus_kind` | Whether the gaze ray hit an actor (`1`/`0`), plus actor ID, name, and semantic kind |
| `focus_x_m`, `focus_y_m`, `focus_z_m`, `focus_distance_m` | World hit location and distance (m); empty when `focus_hit=0` |
| `focus_traffic_light_state`, `looking_at_affecting_light` | State of a focused traffic light and whether the focused actor is the affecting traffic light (`1`/`0`) |
| `head_x_m`, `head_y_m`, `head_z_m`, `head_pitch_deg`, `head_yaw_deg`, `head_roll_deg` | World-space headset camera position (m) and orientation (deg) |
| `gaze_h_deg`, `gaze_v_deg` | Combined horizontal (`atan2(Y, X)`) and vertical (`atan2(Z, sqrt(X^2+Y^2))`) gaze angles (deg) |
| `focus_actor_yaw_deg`, `focus_actor_pitch_deg`, `focus_actor_roll_deg` | World orientation (deg) of the focused actor; empty when `focus_hit=0` |
| `gaze_velocity_deg_s`, `saccade_state`, `saccade_onset_s`, `saccade_amplitude_deg`, `saccade_peak_velocity_deg_s` | Angular gaze velocity (deg/s) and online I-VT saccade state (`fixation` / `saccade`), onset simulation time (s), amplitude (deg), and peak velocity (deg/s) |
| `sample_dt_ms` | Simulation time delta from the previous recorded row (ms) |

### 2. High-Frequency Gaze Stream `<session>_gaze.csv` (18 Columns)

Recorded on the eye tracker's native sample grid with closed-event saccade verdicts:

| Column | Description |
| --- | --- |
| `device_time_ms`, `host_recv_s`, `sim_time_s`, `ue_frame` | Tracker timestamp (ms), host receive time (s), simulation time (s), and UE frame number |
| `comb_h_deg`, `comb_v_deg`, `l_h_deg`, `l_v_deg`, `r_h_deg`, `r_v_deg` | Combined cyclopean horizontal/vertical gaze angles (deg) and raw per-eye fields reported by the runtime (on Pimax PVR 1.33.1, `l_*`/`r_*` are synthesized from the fused cyclopean ray with a constant `4.0000 deg` horizontal offset; use `comb_h_deg`/`comb_v_deg` for analysis) |
| `vergence_deg`, `valid` | Angle between the two runtime rays (deg; synthesized on Pimax PVR, do not use for vergence analysis) and sample validity (`1`/`0`) |
| `gaze_vel_deg_s`, `saccade_state` | Instantaneous gaze angular velocity (deg/s) and closed-event verdict (`fixation`, `saccade`, `rejected`, or `saccade_open`) |
| `saccade_onset_s`, `saccade_amp_deg`, `saccade_peak_deg_s`, `saccade_samples` | Closed saccade event onset time (s), amplitude (deg), peak velocity (deg/s), and sample count (identical across all rows of a closed event) |

Coordinates use the UE/CARLA left-handed coordinate system (X forward, Y right, Z up) with large-map origin offset support. When `gaze_valid=0`, vehicle and road columns are still written while gaze and focus coordinates remain empty.

## Example Filter Queries

- **Gaze allocation during turns:** filter `in_junction=1`, `motion` in `left`/`right`, and `gaze_valid=1`, then compare `steer_input`, `gaze_local_*`, `head_yaw_deg`, and `focus_*`.
- **Braking behavior at red lights:** filter `red_light_stop_candidate=1` and inspect `throttle_input`, `brake_input`, and `focus_*` across `sim_time_s`, grouping by `traffic_light_id`.

## Performance and Verification

- `<session>.csv` defaults to **120 Hz** target sampling (at most once per rendered frame; no synthetic interpolation on low-FPS frames; configurable down to **20 Hz**). When `-EyeThreadHz 120` is enabled, `<session>_gaze.csv` polls the tracker on a dedicated 120 Hz background thread and runs the calibrated I-VT detector (onset 120 deg/s, offset 60 deg/s, minimum duration 20 ms). No samples are written while the simulation is paused.
- CSV writers use buffered I/O flushed once per second and on session close. If a disk write fails, recording stops and logs `Driving study: write failed`.

Developer test commands (run from the repository root):

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\Util\Tests\RunPimaxTests.ps1 -UE4Root '<UE4 source engine root>'
powershell -NoProfile -ExecutionPolicy Bypass -File .\Util\Tests\TestPimaxLauncher.ps1
powershell -NoProfile -STA -ExecutionPolicy Bypass -File .\StartDrivingStudy.ps1 -ValidateOnly
```
