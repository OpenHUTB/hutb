# Hand pose capture and upper-limb replay

Two standalone scripts that turn VR controller motion into a musculoskeletal
upper-limb animation. They do not import the simulator and are safe to run while
a driving session is not happening.

| File | What it does |
| --- | --- |
| `record_hands.py` | Polls SteamVR for both controller poses and writes a CSV (no simulator needed) |
| `replay_arms.py` | Replays such a CSV onto MuJoCo's MS-Human-700 model and renders an MP4 |

The same pose data is also produced by the driving simulator itself when it is
launched with `-DReyeVRHandPose` (launcher: `-HandPose`, GUI: **记录手柄位姿**),
which appends 18 `hand_*` columns to `<session>.csv`. `replay_arms.py` reads
either layout.

## Prerequisites

Python 3.9+ on Windows, and:

```powershell
pip install mujoco openvr numpy pillow imageio imageio-ffmpeg
```

SteamVR must be installed and running with the headset on for `record_hands.py`.
`openvr` here is the Python binding (`pyopenvr`), not the C++ SDK.

The MS-Human-700 model is **not** vendored in this repository; clone it once
(~200 MB with meshes):

```powershell
git clone https://github.com/LNSGroup/MS-Human-700.git
```

Pass the clone directory to the replay script, or point the environment variable
at it:

```powershell
$env:MS_HUMAN_700_DIR = 'C:\models\MS-Human-700'
```

## Usage

Record 60 s at 90 Hz (SteamVR running, headset on):

```powershell
python Util\HandPose\record_hands.py hands.csv 60 90
```

Replay a simulator `<session>.csv` driving segment at 1:1 scale (no `--fit`) into `replay.mp4`:

```powershell
python Util\HandPose\replay_arms.py session.csv replay.mp4 60 16.5 --start 28.0 --model-dir C:\models\MS-Human-700
```

Positional order is `csv out.mp4 [fps] [seconds]`; options may appear anywhere:
- `--model-dir DIR` (or `-m DIR`): path to the `MS-Human-700` clone or `.xml` file.
- `--start SECONDS`: start timestamp within the CSV time column. When omitted, `replay_arms.py` automatically detects the one-shot calibration moment (`>= 5.0 s` continuous head stillness within `5 cm / 6 deg` followed by steering rotation `>= 0.015`) to lock room yaw + wheel centre and starts `1.0 s` before that turn (falling back to `0` if no calibration event is present).
- `--fit`: optional visual scaling so 95% of excursions sit comfortably inside reach (preserves shape and timing, alters amplitude; off by default).
- `--raw-hands`: disable controller-housing rim offset compensation and feed raw tracked controller origins directly to the palm IK targets (useful for auditing raw controller-to-rim offsets).
- `--no-muscle`: disable inverse-dynamics muscle activation coloring and render uniform tendon lines.

Trajectories are resampled by time rather than row index, so a 20 Hz driving CSV and a 90 Hz hands CSV both replay smoothly at the requested video fps. In the simulator (`ADReyeVRPawn`), the same one-shot rule (`>= 5 s` continuous head stillness + steering wheel rotation) automatically recenters the VR view once per session without manual menu clicks. When replaying a simulator `<session>.csv`, `steer_input` drives the steering wheel rotation directly; for standalone `hands.csv`, wheel angle is estimated from the bilateral hand chord.

`hands.csv` columns (from `record_hands.py`):
`host_time_s`, `{l,r}_{valid,connected,x,y,z,qw,qx,qy,qz,trigger,grip}`, `hmd_{valid,x,y,z,qw,qx,qy,qz}` (legacy files without `hmd_*` remain supported).
Poses are the controller (grip) frame in the OpenVR **standing tracking space, metres**.

## Output

- `replay.mp4` — 720x720, seated figure with an articulated steering wheel (12 o'clock marker) and dynamic upper-limb muscle activation heatmap, chest-up framing.
- `replay_first.png`, `replay_mid.png`, `replay_last.png` — one frame each, for a quick eyeball check.
- Console lines worth reading: the sample rate detected, the fitted wheel centre/radius,
  per-joint excursion over the clip (degrees), the IK residual percentiles, the fraction
  of targets clamped to the glenohumeral reach sphere, and the top recruited muscles.

## Limits — read before quoting numbers

- **Fingers are fixed.** A controller gives a rigid 6-DoF pose plus two scalar axes
  (trigger, grip). There is no per-finger curl in the data, so phalanx bones and finger
  tendon wrap sites are pre-curled once into a static cylindrical rim clasp — a constant
  geometric assumption, not a measurement.
- **Controller-shell rim offset is compensated by default.** When controllers are strapped
  to or held against a physical racing wheel (e.g. Logitech G29), the OpenVR tracked grip
  origin sits ~4–10 cm outside the physical rim centerline. By default, samples within the
  near-rim shell band (`<= 11.5 cm`, cosine-blended out to `16.5 cm`) are projected onto the
  fitted rim circle while larger excursions release smoothly to free 3-D space. Pass
  `--raw-hands` to inspect uncompensated controller origins; the script prints both raw
  controller-to-rim and final palm-to-rim 3-D distances.
- **Legs are not driven.** They are posed once into a seated posture satisfying MS-Human-700's
  tibiofemoral/patellar coupling constraints and then left static; pedal motion is not reconstructed.
- **Muscle activations are model-inferred, not EMG.** Each frame solves inverse dynamics (`mj_inverse`)
  plus Hill-type force-length-velocity static optimization over MS-Human-700's coupled upper-limb
  moment arms (including a nominal steering rim load) to color active tendons. Those numbers reflect
  model assumptions rather than surface electromyography.
- **Reach is clamped (and optionally scaled with `--fit`).** Targets farther than ~92% of the
  glenohumeral-to-palm arm length are pulled onto the reach sphere; the script prints the exact
  clamped percentage. `--fit` scales amplitudes for cleaner visualization and must not be quoted as
  unscaled metric displacement.
- **Grip orientation and elbow swivel are model-guided.** Palm 3-D position drives the primary
  active-set IK task while the 4-D arm nullspace aligns the palm normal overhand onto the rim and
  regularizes a natural downward-lateral elbow swivel posture rather than raw controller twist.
- **Frame is aligned by assumption.** The recorded room is yaw-aligned to the model's
  left-right shoulder axis and anchored in front of the chest; absolute room position is
  therefore not reproduced.
