r"""Drive MS-Human-700's upper limbs from a recorded controller CSV.

No simulator, no UE: loads the MuJoCo model, puts it in a seated posture, maps
the two controller trajectories into model space, then solves a damped
least-squares IK for each arm per frame and renders a video.

The MS-Human-700 model is not vendored here. Clone it once and point the script
at the clone (argument first, env var second):

  git clone https://github.com/LNSGroup/MS-Human-700.git
  set MS_HUMAN_700_DIR=C:\models\MS-Human-700        (PowerShell: $env:MS_HUMAN_700_DIR=...)

Usage:
  python replay_arms.py hands.csv replay.mp4 [fps] [seconds] [--start S] [--fit] [--raw-hands] [--no-muscle] [--model-dir DIR]

The CSV is the output of record_hands.py (or a driving CSV recorded with
-DReyeVRHandPose, which carries hand_{l,r}_{x,y,z}_m and steer_input columns).
"""
import csv, math, os, sys
import numpy as np
import mujoco

DEFAULT_MODEL_XML = "MS-Human-700.xml"
# Seven independent anatomical DOFs per arm; MS-Human-700's 26 upper-limb coupled
# joints (clavicle, scapula, phantom frames, distal carpal row) follow analytically
# via mjEQ_JOINT polynomial constraints. Tuple: (joint_name, ik_weight, comfort_rad).
ARM_JOINTS = {
    side: [
        ("elv_angle_" + side, 1.0, 1.05),
        ("shoulder_elv_" + side, 1.0, 0.68),
        ("shoulder_rot_" + side, 0.9, 0.15),
        ("elbow_flexion_" + side, 1.0, 1.38),
        ("pro_sup_" + side, 0.75, 0.45),
        ("deviation_" + side, 0.5, 0.05),
        ("flexion_" + side, 0.5, 0.0),
    ]
    for side in ("l", "r")
}
SEAT_DROP = -0.35   # metres, along pelvis_ty (the pelvis body is rotated 90 deg)
HEAD_LIFT = 0.35    # radians; positive lifts the chin (sign verified visually)
SEATED_LEGS = (
    ("hip_flexion", 1.25),
    ("hip_adduction", -0.08),
    ("hip_rotation", 0.0),
    ("knee_angle", 1.15),
    ("ankle_angle", 0.12),
)
MAX_WHEEL_RAD = math.radians(450.0)  # 900 deg lock-to-lock racing wheel
# Two accepted CSV layouts: record_hands.py, and the driving CSV's optional hand_* block.
SCHEMAS = (
    {"tag": "hands.csv", "t": "host_time_s", "v": "{s}_valid",
     "x": "{s}_x", "y": "{s}_y", "z": "{s}_z", "steer": None},
    {"tag": "driving.csv", "t": "host_elapsed_s", "v": "hand_{s}_valid",
     "x": "hand_{s}_x_m", "y": "hand_{s}_y_m", "z": "hand_{s}_z_m", "steer": "steer_input"},
)


def parse_args(argv):
    """Positional args stay in order; options may appear anywhere."""
    positional, model_dir, start, fit, muscle, raw_hands = (
        [], os.environ.get("MS_HUMAN_700_DIR"), None, False, True, False)
    i = 0
    while i < len(argv):
        a = argv[i]
        if a in ("--model-dir", "-m"):
            i += 1
            if i >= len(argv):
                raise SystemExit("--model-dir needs a directory or an .xml path")
            model_dir = argv[i]
        elif a.startswith("--model-dir="):
            model_dir = a.split("=", 1)[1]
        elif a == "--start":
            i += 1
            start = float(argv[i])
        elif a.startswith("--start="):
            start = float(a.split("=", 1)[1])
        elif a == "--fit":
            fit = True
        elif a == "--no-muscle":
            muscle = False
        elif a == "--raw-hands":
            raw_hands = True
        else:
            positional.append(a)
        i += 1
    return positional, model_dir, start, fit, muscle, raw_hands


def resolve_model(model_dir):
    if not model_dir:
        raise SystemExit(
            "MS-Human-700 is not bundled here. Pass --model-dir <path-to-clone>/MS-Human-700 "
            "or set MS_HUMAN_700_DIR.\n"
            "  git clone https://github.com/LNSGroup/MS-Human-700.git")
    path = os.path.abspath(os.path.expanduser(model_dir))
    if os.path.isdir(path):
        path = os.path.join(path, DEFAULT_MODEL_XML)
    if not os.path.isfile(path):
        raise SystemExit("no MuJoCo model at " + path)
    return path


def qmul(q1, q2):
    w1, x1, y1, z1 = q1
    w2, x2, y2, z2 = q2
    return np.array([
        w1 * w2 - x1 * x2 - y1 * y2 - z1 * z2,
        w1 * x2 + x1 * w2 + y1 * z2 - z1 * y2,
        w1 * y2 - x1 * z2 + y1 * w2 + z1 * x2,
        w1 * z2 + x1 * y2 - y1 * x2 + z1 * w2,
    ])


def build_model(model_path, wheel_center=None, wheel_radius=0.145, wheel_tilt_rad=0.18):
    """Load MS-Human-700, curl fingers into a rim clasp, and add cockpit props."""
    spec = mujoco.MjSpec.from_file(model_path)
    wb = spec.worldbody

    # Curl static phalanx meshes + finger tendon wrap sites around the MCP line so
    # both hands form a closed cylindrical clasp around palm_{l,r}.
    for side, zsign in (("l", 1.0), ("r", -1.0)):
        hb = spec.body("hand_" + side)
        pivot = np.array([0.0, -0.064, -0.004 * zsign])
        ang = -0.42 * zsign
        dq = np.array([math.cos(ang / 2), math.sin(ang / 2), 0.0, 0.0])
        c, s = math.cos(ang), math.sin(ang)
        Rx = np.array([[1.0, 0.0, 0.0], [0.0, c, -s], [0.0, s, c]])
        for g in hb.geoms:
            if g.pos[1] < -0.066 and "thumb" not in g.name:
                g.pos[:] = pivot + Rx @ (np.array(g.pos) - pivot)
                g.quat[:] = qmul(dq, np.array(g.quat))
        for st in hb.sites:
            if st.pos[1] < -0.066 and "thumb" not in st.name:
                st.pos[:] = pivot + Rx @ (np.array(st.pos) - pivot)
        ps = hb.add_site()
        ps.name = "palm_" + side
        ps.pos[:] = [0.012, -0.061, 0.022 * zsign]

    def prop_geom(parent, name, gtype, size, pos, rgba, quat=None):
        g = parent.add_geom()
        g.name, g.type = name, gtype
        g.size[:], g.pos[:], g.rgba[:] = size, pos, rgba
        g.contype, g.conaffinity = 0, 0
        if quat is not None:
            g.quat[:] = quat
        return g

    def z_to(vec):
        vec = np.asarray(vec, float)
        vec = vec / np.linalg.norm(vec)
        z = np.array([0.0, 0.0, 1.0])
        axis = np.cross(z, vec)
        s = np.linalg.norm(axis)
        if s < 1e-9:
            return np.array([1.0, 0.0, 0.0, 0.0]) if vec[2] > 0 else np.array([0.0, 1.0, 0.0, 0.0])
        axis /= s
        ang = math.acos(max(-1.0, min(1.0, float(np.dot(z, vec)))))
        return np.r_[math.cos(ang / 2), math.sin(ang / 2) * axis]

    grey = [0.20, 0.20, 0.23, 1.0]
    prop_geom(wb, "seat_base", mujoco.mjtGeom.mjGEOM_BOX, [0.24, 0.23, 0.04], [0.06, 0.0, 0.46], grey)
    prop_geom(wb, "seat_back", mujoco.mjtGeom.mjGEOM_BOX, [0.04, 0.23, 0.32], [-0.22, 0.0, 0.80], grey)

    # Model faces +x: wheel normal tilts upward toward the driver (-tilt around +y).
    wc = np.array([0.33, 0.0, 0.96]) if wheel_center is None else np.asarray(wheel_center, float)
    rim = float(wheel_radius)
    w_norm = np.array([math.cos(wheel_tilt_rad), 0.0, -math.sin(wheel_tilt_rad)])
    col_rgba = [0.15, 0.15, 0.17, 0.42]
    col_p0 = wc + 0.02 * w_norm
    col_p1 = wc + 0.15 * w_norm
    prop_geom(wb, "column", mujoco.mjtGeom.mjGEOM_CAPSULE,
              [0.018, 0.5 * float(np.linalg.norm(col_p1 - col_p0)), 0.0],
              0.5 * (col_p0 + col_p1), col_rgba, z_to(col_p1 - col_p0))
    prop_geom(wb, "dash_mount", mujoco.mjtGeom.mjGEOM_BOX,
              [0.025, 0.070, 0.045], col_p1 + 0.02 * w_norm, col_rgba)

    wheel_body = wb.add_body()
    wheel_body.name = "steering_wheel"
    wheel_body.pos[:] = wc
    # Rotate wheel local +x onto w_norm (rotation around +y by wheel_tilt_rad).
    wheel_body.quat[:] = [math.cos(wheel_tilt_rad / 2), 0.0, math.sin(wheel_tilt_rad / 2), 0.0]
    wj = wheel_body.add_joint()
    wj.name, wj.type = "wheel_steer", mujoco.mjtJoint.mjJNT_HINGE
    wj.axis[:], wj.range[:] = [1.0, 0.0, 0.0], [-12.0, 12.0]

    dark = [0.10, 0.10, 0.11, 0.48]  # translucent so the torso & active muscles stay readable
    top_mark = [0.92, 0.76, 0.22, 0.85]
    n = 24
    for i in range(n):
        a0, a1 = 2 * math.pi * i / n, 2 * math.pi * (i + 1) / n
        p0 = np.array([0.0, rim * math.cos(a0), rim * math.sin(a0)])
        p1 = np.array([0.0, rim * math.cos(a1), rim * math.sin(a1)])
        mid = 0.5 * (p0 + p1)
        is_top = i in (5, 6)  # 12 o'clock centre marker (+z is sin=1 around a=pi/2)
        prop_geom(wheel_body, "rim_%02d" % i, mujoco.mjtGeom.mjGEOM_CAPSULE,
                  [0.013 if is_top else 0.012, 0.5 * float(np.linalg.norm(p1 - p0)), 0.0],
                  mid, top_mark if is_top else dark, z_to(p1 - p0))
    for a in (0.0, math.pi, 1.5 * math.pi):  # 9, 3, and 6 o'clock T-spokes
        end = np.array([0.0, rim * math.cos(a), rim * math.sin(a)])
        prop_geom(wheel_body, "spoke_%d" % int(round(math.degrees(a))), mujoco.mjtGeom.mjGEOM_CAPSULE,
                  [0.008, 0.5 * rim, 0.0], 0.5 * end, dark, z_to(end))
    prop_geom(wheel_body, "hub", mujoco.mjtGeom.mjGEOM_CYLINDER,
              [0.036, 0.02, 0.0], [0.0, 0.0, 0.0], grey, [0.70710678, 0.0, 0.70710678, 0.0])
    return spec.compile()


def load_csv(path):
    """Read either record_hands.py output or a driving CSV written with -DReyeVRHandPose."""
    rows = list(csv.DictReader(open(path, encoding="utf-8-sig")))
    if not rows:
        return [], None
    keys = set(rows[0].keys())
    schema = next((s for s in SCHEMAS if s["v"].format(s="l") in keys), None)
    if schema is None:
        raise SystemExit("no l_valid / hand_l_valid columns in " + path)
    tkey = schema["t"] if schema["t"] in keys else "sim_time_s"
    skey = schema["steer"] if schema["steer"] and schema["steer"] in keys else None

    raw_t, raw_l, raw_r, raw_v, raw_st, raw_hp, raw_hq = [], [], [], [], [], [], []
    has_drv_head = {"head_x_m", "vehicle_x_m", "head_yaw_deg", "vehicle_yaw_deg"}.issubset(keys)
    has_hmd_cols = {"hmd_valid", "hmd_x", "hmd_qw"}.issubset(keys)
    for r in rows:
        vl = r.get(schema["v"].format(s="l")) == "1"
        vr = r.get(schema["v"].format(s="r")) == "1"
        if not vl and not vr:
            continue
        try:
            t = float(r[tkey])
            pl = [float(r[schema[c].format(s="l")]) for c in ("x", "y", "z")] if vl else [np.nan] * 3
            pr = [float(r[schema[c].format(s="r")]) for c in ("x", "y", "z")] if vr else [np.nan] * 3
            st = float(r[skey]) if (skey and r.get(skey, "") != "") else np.nan
            hpos = [np.nan] * 3
            hquat = [np.nan] * 4
            if has_drv_head and r.get("head_x_m", "") != "" and r.get("vehicle_x_m", "") != "":
                dx = float(r["head_x_m"]) - float(r["vehicle_x_m"])
                dy = float(r["head_y_m"]) - float(r["vehicle_y_m"])
                dz = float(r["head_z_m"]) - float(r["vehicle_z_m"])
                vy = math.radians(float(r["vehicle_yaw_deg"]))
                hpos = [(dx * math.cos(vy) + dy * math.sin(vy)) * 100.0,
                        (-dx * math.sin(vy) + dy * math.cos(vy)) * 100.0,
                        dz * 100.0]
                hp = 0.5 * math.radians(float(r["head_pitch_deg"]))
                hy = 0.5 * math.radians(((float(r["head_yaw_deg"]) - float(r["vehicle_yaw_deg"]) + 180.0) % 360.0) - 180.0)
                hr = 0.5 * math.radians(float(r["head_roll_deg"]))
                cp, sp = math.cos(hp), math.sin(hp)
                cy, sy = math.cos(hy), math.sin(hy)
                cr, sr = math.cos(hr), math.sin(hr)
                hquat = [cr * cp * cy + sr * sp * sy,
                         sr * cp * cy - cr * sp * sy,
                         cr * sp * cy + sr * cp * sy,
                         cr * cp * sy - sr * sp * cy]
            elif has_hmd_cols and r.get("hmd_valid") == "1" and r.get("hmd_x", "") != "":
                hpos = [float(r["hmd_x"]) * 100.0, float(r["hmd_y"]) * 100.0, float(r["hmd_z"]) * 100.0]
                hquat = [float(r["hmd_qw"]), float(r["hmd_qx"]), float(r["hmd_qy"]), float(r["hmd_qz"])]
        except (KeyError, ValueError):
            continue
        raw_t.append(t)
        raw_l.append(pl)
        raw_r.append(pr)
        raw_v.append((vl, vr))
        raw_st.append(st)
        raw_hp.append(hpos)
        raw_hq.append(hquat)
    if not raw_t:
        return [], schema["tag"], None

    times = np.asarray(raw_t, float)
    arr_l, arr_r = np.asarray(raw_l, float), np.asarray(raw_r, float)
    for arr in (arr_l, arr_r):
        good = np.isfinite(arr[:, 0])
        if not np.any(good):
            return [], schema["tag"], None
        if not np.all(good):
            for c in range(3):
                arr[:, c] = np.interp(times, times[good], arr[good, c])
    steers = np.asarray(raw_st, float)
    has_steer = bool(np.any(np.isfinite(steers)))
    if has_steer and not np.all(np.isfinite(steers)):
        good_s = np.isfinite(steers)
        steers = np.interp(times, times[good_s], steers[good_s])
    both_mask = np.array([vl and vr for vl, vr in raw_v], dtype=bool)
    calib_idx = detect_auto_calibration(
        times, np.asarray(raw_hp, float), np.asarray(raw_hq, float),
        steers if has_steer else None, arr_l, arr_r)
    out = [(times[i], np.array([arr_l[i], arr_r[i]]), steers[i] if has_steer else None, both_mask[i])
           for i in range(len(times))]
    return out, schema["tag"], calib_idx


def detect_auto_calibration(times, head_pos_cm, head_quats, steers, arr_l, arr_r,
                            still_req_s=5.0, max_pos_cm=5.0, max_ang_deg=6.0,
                            quiet_rate=0.008, steer_delta=0.015):
    """Mirror FDReyeVRAutoCalibrator: >= 5 s continuous head stillness + wheel rotation start."""
    if len(times) < 5 or not np.any(np.isfinite(head_pos_cm[:, 0])):
        return None
    if steers is None:
        mjc_l = np.array([ovr_to_mjc(p) for p in arr_l])
        mjc_r = np.array([ovr_to_mjc(p) for p in arr_r])
        chord = mjc_l - mjc_r
        ang = np.unwrap(np.arctan2(chord[:, 2], chord[:, 1]))
        steers = (ang - np.median(ang)) / MAX_WHEEL_RAD

    anchor_p = None
    anchor_q = None
    still_s = 0.0
    steer_rest = 0.0
    prev_steer = 0.0
    for i in range(len(times)):
        hp, hq, st = head_pos_cm[i], head_quats[i], float(steers[i])
        if not (np.all(np.isfinite(hp)) and np.all(np.isfinite(hq)) and math.isfinite(st)):
            continue
        qn = float(np.linalg.norm(hq))
        if qn < 1e-6:
            continue
        hq = hq / qn
        if anchor_p is None:
            anchor_p, anchor_q = hp.copy(), hq.copy()
            steer_rest = prev_steer = st
            still_s = 0.0
            continue
        raw_dt = float(times[i] - times[i - 1]) if i > 0 else 0.0
        if raw_dt <= 0.0:
            continue
        step_dt = min(raw_dt, 0.1)

        # If already armed (>= 5 s stillness) and the wheel rotates on this exact frame,
        # fire even if ADReyeVRPawn simultaneously snapped FirstPersonCam yaw/pos on recenter.
        if still_s >= still_req_s and abs(st - steer_rest) >= steer_delta:
            return i

        pos_drift = float(np.linalg.norm(hp - anchor_p))
        dot = min(1.0, abs(float(np.dot(hq, anchor_q))))
        ang_drift = math.degrees(2.0 * math.acos(dot))
        if pos_drift > max_pos_cm or ang_drift > max_ang_deg:
            anchor_p, anchor_q = hp.copy(), hq.copy()
            steer_rest = prev_steer = st
            still_s = 0.0
            continue

        steer_rate = abs(st - prev_steer) / step_dt
        prev_steer = st
        still_s += step_dt
        if still_s < still_req_s:
            if steer_rate < quiet_rate:
                steer_rest = st
        elif abs(st - steer_rest) >= steer_delta:
            return i
    return None


def ovr_to_mjc(p):
    # OpenVR right-handed (x right, y up, z back) -> MuJoCo right-handed (x fwd, y left, z up)
    return np.array([-p[2], -p[0], p[1]])


def build_couplings(m):
    """Extract mjEQ_JOINT polynomial couplings (dep_jnt <- indep_jnt) from MS-Human-700."""
    out = []
    for i in range(m.neq):
        if m.eq_type[i] == mujoco.mjtEq.mjEQ_JOINT and m.eq_obj2id[i] >= 0:
            j_dep, j_ind = int(m.eq_obj1id[i]), int(m.eq_obj2id[i])
            q_dep, q_ind = int(m.jnt_qposadr[j_dep]), int(m.jnt_qposadr[j_ind])
            d_dep, d_ind = int(m.jnt_dofadr[j_dep]), int(m.jnt_dofadr[j_ind])
            out.append((q_dep, q_ind, d_dep, d_ind, float(m.qpos0[q_dep]), float(m.qpos0[q_ind]),
                        m.eq_data[i, :5].copy()))
    return out


def apply_coupled_joints(d, couplings):
    """Update all coupled dependent joints so max |d.efc_pos| stays at machine zero."""
    for q_dep, q_ind, _, _, y0, x0, c in couplings:
        dx = float(d.qpos[q_ind]) - x0
        d.qpos[q_dep] = y0 + c[0] + dx * (c[1] + dx * (c[2] + dx * (c[3] + dx * c[4])))


def coupling_matrix(m, d, couplings, dofs):
    """Chain-rule matrix G (nv x n_indep) mapping independent joint velocities to full nv."""
    G = np.zeros((m.nv, len(dofs)))
    col_of = {d_adr: k for k, (_, d_adr, _, _, _) in enumerate(dofs)}
    for k, (_, d_adr, _, _, _) in enumerate(dofs):
        G[d_adr, k] = 1.0
    for _, q_ind, d_dep, d_ind, _, x0, c in couplings:
        k = col_of.get(d_ind)
        if k is not None:
            dx = float(d.qpos[q_ind]) - x0
            G[d_dep, k] += c[1] + dx * (2.0 * c[2] + dx * (3.0 * c[3] + 4.0 * c[4] * dx))
    return G


def seat_posture(m, d, couplings):
    """Seated posture with exact patellar/tibiofemoral and scapulohumeral coupling."""
    mujoco.mj_resetData(m, d)
    jt = mujoco.mj_name2id(m, mujoco.mjtObj.mjOBJ_JOINT, "pelvis_ty")
    d.qpos[m.jnt_qposadr[jt]] = SEAT_DROP
    for side in ("l", "r"):
        for base, val in SEATED_LEGS:
            jid = mujoco.mj_name2id(m, mujoco.mjtObj.mjOBJ_JOINT, base + "_" + side)
            if jid >= 0:
                lo, hi = m.jnt_range[jid] if m.jnt_limited[jid] else (-10.0, 10.0)
                d.qpos[m.jnt_qposadr[jid]] = min(max(val, lo), hi)
        for name, _, comfort in ARM_JOINTS[side]:
            jid = mujoco.mj_name2id(m, mujoco.mjtObj.mjOBJ_JOINT, name)
            if jid >= 0:
                d.qpos[m.jnt_qposadr[jid]] = comfort
    hj = mujoco.mj_name2id(m, mujoco.mjtObj.mjOBJ_JOINT, "T1_head_neck_FE")
    if hj >= 0:
        d.qpos[m.jnt_qposadr[hj]] = HEAD_LIFT
    apply_coupled_joints(d, couplings)
    mujoco.mj_forward(m, d)


def arm_dofs(m, side):
    out = []
    for n, w, comfort in ARM_JOINTS[side]:
        j = mujoco.mj_name2id(m, mujoco.mjtObj.mjOBJ_JOINT, n)
        if j < 0:
            raise SystemExit("missing joint " + n)
        out.append((int(m.jnt_qposadr[j]), int(m.jnt_dofadr[j]), int(j), float(w), float(comfort)))
    return out


def solve_ik(m, d, side, dofs, site_id, hand_bid, target, wheel_center, wheel_normal, couplings,
             iters=140, tol=8e-4):
    """Coupled 6-DoF rim-clasp IK with active-set nullspace projection & gravitational elbow swivel."""
    jacp, jacr, jace = np.zeros((3, m.nv)), np.zeros((3, m.nv)), np.zeros((3, m.nv))
    w = np.array([x[3] for x in dofs])
    q_ref = np.array([x[4] for x in dofs])
    q_idx = [x[0] for x in dofs]
    bounds = np.array([m.jnt_range[x[2]] if m.jnt_limited[x[2]] else (-10.0, 10.0) for x in dofs])
    zsign = 1.0 if side == "l" else -1.0
    elbow_bid = mujoco.mj_name2id(m, mujoco.mjtObj.mjOBJ_BODY, "ulna_" + side)
    sh_bid = mujoco.mj_name2id(m, mujoco.mjtObj.mjOBJ_BODY, "humerus_" + side)
    elbow_pref = d.xpos[sh_bid].copy() + np.array([0.12, 0.09 * zsign, -0.22])

    # Project hand vector onto the tilted wheel plane for rim tangent & overhand palm normal.
    w_up = np.array([-wheel_normal[2], 0.0, wheel_normal[0]])
    rel = target - wheel_center
    in_plane = rel - float(np.dot(rel, wheel_normal)) * wheel_normal
    rn = float(np.linalg.norm(in_plane))
    u_r = in_plane / rn if rn > 0.025 else np.array([0.0, zsign, 0.0])
    tang = np.cross(wheel_normal, u_r) if side == "l" else np.cross(u_r, wheel_normal)
    tang /= max(float(np.linalg.norm(tang)), 1e-8)
    topness = max(0.0, float(np.dot(u_r, w_up)))
    thumb_des = 0.60 * tang + 0.40 * w_up
    thumb_des /= max(float(np.linalg.norm(thumb_des)), 1e-8)
    palm_des = (0.42 + 0.42 * topness) * wheel_normal - (0.91 - 0.38 * topness) * u_r
    palm_des /= max(float(np.linalg.norm(palm_des)), 1e-8)

    lam = 1.0e-3
    for _ in range(iters):
        apply_coupled_joints(d, couplings)
        mujoco.mj_forward(m, d)
        err_p = target - d.site_xpos[site_id]
        R = d.xmat[hand_bid].reshape(3, 3)
        err_r = 0.22 * np.cross(R[:, 0], thumb_des) + np.cross(zsign * R[:, 2], palm_des)
        if float(np.linalg.norm(err_p)) < tol and float(np.linalg.norm(err_r)) < 0.05:
            break

        mujoco.mj_jacSite(m, d, jacp, jacr, site_id)
        mujoco.mj_jacBody(m, d, jace, None, elbow_bid)
        G = coupling_matrix(m, d, couplings, dofs)
        q_cur = np.array([d.qpos[q] for q in q_idx])

        Jp_raw = jacp @ G
        Jp = Jp_raw * w
        Jr = (jacr @ G) * w
        Je = (jace @ G) * w
        Jp_pinv = Jp.T @ np.linalg.solve(Jp @ Jp.T + lam * np.eye(3), np.eye(3))
        N_proj = np.eye(len(dofs)) - Jp_pinv @ Jp

        # Secondary task in 4-D nullspace: cylindrical rim clasp + natural low-lateral elbow swivel.
        J_sec = np.vstack([Jr @ N_proj, 1.25 * (Je @ N_proj)])
        e_sec = np.concatenate([0.65 * err_r, 1.25 * (elbow_pref - d.xpos[elbow_bid])])
        dq_sec = N_proj @ (
            J_sec.T @ np.linalg.solve(J_sec @ J_sec.T + 0.04 * np.eye(6), e_sec)
            - 0.12 * (q_cur - q_ref)
        )
        q_sec = np.clip(q_cur + w * dq_sec, bounds[:, 0], bounds[:, 1])
        actual_sec = q_sec - q_cur

        # Active-set primary position step: compensates any drift from clamped wrist limits.
        ep_corr = err_p - Jp_raw @ actual_sec
        w_act = w.copy()
        probe = w * (Jp_pinv @ ep_corr)
        for k in range(len(dofs)):
            if ((q_sec[k] <= bounds[k, 0] + 1e-4 and probe[k] < 0.0) or
                    (q_sec[k] >= bounds[k, 1] - 1e-4 and probe[k] > 0.0)):
                w_act[k] = 0.0
        Jp_act = Jp_raw * w_act
        dq_pri = w_act * (Jp_act.T @ np.linalg.solve(Jp_act @ Jp_act.T + lam * np.eye(3), ep_corr))

        dq = actual_sec + dq_pri
        step = float(np.max(np.abs(dq))) if dq.size else 0.0
        if step > 0.22:
            dq *= 0.22 / step
        q_next = np.clip(q_cur + dq, bounds[:, 0], bounds[:, 1])
        for k, qadr in enumerate(q_idx):
            d.qpos[qadr] = float(q_next[k])
        lam = max(1e-4, lam * 0.80)
    apply_coupled_joints(d, couplings)
    mujoco.mj_forward(m, d)
    return float(np.linalg.norm(target - d.site_xpos[site_id]))


def smooth_series(arr, sigma_frames):
    """Reflect-padded 1-D Gaussian filter along axis 0 so qvel/qacc have no knot spikes."""
    if sigma_frames <= 0.25 or len(arr) < 5:
        return arr.copy()
    rad = max(1, int(math.ceil(3.0 * sigma_frames)))
    x = np.arange(-rad, rad + 1, dtype=float)
    k = np.exp(-0.5 * (x / sigma_frames) ** 2)
    k /= k.sum()
    flat = arr.reshape(len(arr), -1)
    pad = np.pad(flat, ((rad, rad), (0, 0)), mode="edge")
    out = np.column_stack([np.convolve(pad[:, c], k, mode="valid") for c in range(flat.shape[1])])
    return out.reshape(arr.shape)


def solve_muscle_activations(m, d, q_traj, dt, couplings, all_arm_dofs, wheel_dof, palm_sites):
    """Hill-muscle static optimization from mj_inverse torques across all frames."""
    n_frames = len(q_traj)
    acts = np.zeros((n_frames, m.nu))
    if n_frames < 3 or m.nu == 0:
        return acts

    q_smooth = smooth_series(q_traj, max(0.8, 0.035 / max(dt, 1e-3)))
    qvel = np.gradient(q_smooth, dt, axis=0)
    qacc = np.gradient(qvel, dt, axis=0)
    dense_mom = np.zeros((m.nu, m.nv))
    jacp = np.zeros((3, m.nv))
    wc_body = mujoco.mj_name2id(m, mujoco.mjtObj.mjOBJ_BODY, "steering_wheel")

    for t in range(n_frames):
        d.qpos[:] = q_smooth[t]
        apply_coupled_joints(d, couplings)
        d.qvel[:] = qvel[t]
        d.qacc[:] = qacc[t]

        # Passive Hill force (act=0) vs full active capacity (act=1) at current (q, qvel).
        d.act[:] = 0.0
        mujoco.mj_forward(m, d)
        f0 = d.actuator_force.copy()
        mujoco.mju_sparse2dense(dense_mom, d.actuator_moment, d.moment_rownnz, d.moment_rowadr, d.moment_colind)
        M = dense_mom.copy()

        d.act[:] = 1.0
        mujoco.mj_forward(m, d)
        df = d.actuator_force - f0

        d.act[:] = 0.0
        mujoco.mj_inverse(m, d)
        tau_full = d.qfrc_inverse - (M.T @ f0)

        # Add realistic steering self-centring + damping rim load shared by both palms.
        if wheel_dof >= 0 and wc_body >= 0:
            w_ang = float(d.qpos[wheel_dof])
            w_vel = float(d.qvel[wheel_dof])
            rim_tau = 5.5 * w_ang + 1.2 * w_vel
            wc = d.xpos[wc_body]
            for sid in palm_sites:
                r_vec = d.site_xpos[sid] - wc
                r_yz = np.array([0.0, r_vec[1], r_vec[2]])
                rn = float(np.linalg.norm(r_yz))
                if rn > 0.04:
                    tang = np.array([0.0, -r_yz[2], r_yz[1]]) / rn
                    f_hand_on_rim = 0.5 * (rim_tau / max(rn, 0.12)) * tang
                    mujoco.mj_jacSite(m, d, jacp, None, sid)
                    tau_full += jacp.T @ f_hand_on_rim

        G = coupling_matrix(m, d, couplings, all_arm_dofs)
        tau_req = G.T @ tau_full
        B = (G.T @ M.T) * df  # (14, nu)
        active_mask = np.linalg.norm(B, axis=0) > 1e-3
        if not np.any(active_mask):
            continue
        # Normalize each joint torque row by its moment-generating capacity so small
        # distal wrist joints do not saturate while proximal shoulder muscles coast.
        Bs_raw = B[:, active_mask]
        cap = np.maximum(np.linalg.norm(Bs_raw, axis=1), 6.0)
        Bs = Bs_raw / cap[:, None]
        tau_n = tau_req / cap
        reg = 0.35
        lip = float(np.linalg.norm(Bs, ord=2) ** 2) + reg
        lr = 1.0 / max(lip, 1e-4)
        a = acts[t - 1, active_mask].copy() if t > 0 else np.zeros(Bs.shape[1])
        y = a.copy()
        for k in range(30):
            grad = Bs.T @ (Bs @ y - tau_n) + reg * y
            a_next = np.clip(y - lr * grad, 0.0, 1.0)
            y = a_next + (k / (k + 3.0)) * (a_next - a)
            a = a_next
        acts[t, active_mask] = a

    return smooth_series(acts, 1.2)


def apply_muscle_colors(m, act_row, upper_mask, base_width):
    """Color active upper-limb tendons from calm maroon -> vivid scarlet -> glowing gold."""
    m.tendon_rgba[:] = [0.42, 0.15, 0.16, 0.36]
    m.tendon_width[:] = base_width * 0.85
    for i in np.flatnonzero(upper_mask):
        tid = int(m.actuator_trnid[i, 0])
        if tid < 0 or tid >= m.ntendon:
            continue
        s = min(1.0, max(0.0, float(act_row[i]) / 0.35))
        if s < 0.5:
            u = s * 2.0
            rgba = [0.42 + 0.56 * u, 0.15 + 0.11 * u, 0.16 - 0.06 * u, 0.42 + 0.48 * u]
        else:
            u = (s - 0.5) * 2.0
            rgba = [0.98 + 0.02 * u, 0.26 + 0.62 * u, 0.10 + 0.14 * u, 0.90 + 0.10 * u]
        m.tendon_rgba[tid] = rgba
        m.tendon_width[tid] = base_width[tid] * (0.90 + 0.75 * s)


def rim_distance_3d(pts, wc, w_norm, rim):
    """Exact 3-D distance from point(s) (..., 3) to the circular rim centerline."""
    rel = pts - wc
    axial = np.sum(rel * w_norm, axis=-1)
    in_plane = rel - axial[..., None] * w_norm
    rad = np.linalg.norm(in_plane, axis=-1)
    return np.hypot(axial, rad - rim), in_plane, rad


def main():
    positional, model_dir, start_s, fit, show_muscle, raw_hands = parse_args(sys.argv[1:])
    csv_path = positional[0] if len(positional) > 0 else "hands.csv"
    out_path = positional[1] if len(positional) > 1 else "replay.mp4"
    fps = int(positional[2]) if len(positional) > 2 else 25
    seconds = float(positional[3]) if len(positional) > 3 else 60.0

    model_path = resolve_model(model_dir)
    print("model:", model_path)

    m0 = build_model(model_path)
    d0 = mujoco.MjData(m0)
    c0 = build_couplings(m0)
    seat_posture(m0, d0, c0)

    ps_l = mujoco.mj_name2id(m0, mujoco.mjtObj.mjOBJ_SITE, "palm_l")
    ps_r = mujoco.mj_name2id(m0, mujoco.mjtObj.mjOBJ_SITE, "palm_r")
    rest_palms = np.array([d0.site_xpos[ps_l].copy(), d0.site_xpos[ps_r].copy()])

    frames, schema, calib_idx = load_csv(csv_path)
    if not frames:
        raise SystemExit("no valid frames in " + csv_path)
    times_all = np.array([f[0] for f in frames])
    dts = np.diff(times_all)
    hz = 1.0 / float(np.median(dts)) if dts.size and np.median(dts) > 0 else 90.0
    print("csv: %d valid frames at %.1f Hz (%s columns)" % (len(frames), hz, schema))

    calib_time = float(times_all[calib_idx]) if calib_idx is not None else None
    if calib_time is not None:
        print("auto-calib: triggered once at t=%.2f s (5.0 s head stillness + wheel rotation)" % calib_time)
    if start_s is None:
        start_s = max(float(times_all[0]), calib_time - 1.0) if calib_time is not None else 0.0

    # Slice to the requested time window FIRST so setup/teardown outliers outside the
    # window never distort room yaw, wheel centre/radius, or --fit scaling.
    t_end = min(float(times_all[-1]), start_s + seconds)
    win = (times_all >= start_s) & (times_all <= t_end + 1e-6)
    if np.count_nonzero(win) < 2:
        raise SystemExit("empty time window; check --start/seconds against the CSV span")
    times = times_all[win]
    pos = np.array([[ovr_to_mjc(p) for p in frames[i][1]] for i in np.flatnonzero(win)])
    steers = [frames[i][2] for i in np.flatnonzero(win)]
    both_mask = np.array([frames[i][3] for i in np.flatnonzero(win)], dtype=bool)

    # If the 5-s head-stillness + wheel-turn auto-calibration fired, lock room yaw and
    # bilateral grip centre once from the steady grip window right around calib_time.
    calib_pos = None
    if calib_time is not None:
        c_win = (times_all >= calib_time - 2.0) & (times_all <= calib_time + 0.5)
        c_both = np.array([frames[i][3] for i in np.flatnonzero(c_win)], dtype=bool)
        if np.count_nonzero(c_win) >= 5 and np.any(c_both):
            raw_c = np.array([[ovr_to_mjc(p) for p in frames[i][1]] for i in np.flatnonzero(c_win)])
            calib_pos = raw_c[c_both]

    ref_pos = calib_pos if calib_pos is not None else (pos[both_mask] if np.any(both_mask) else pos)
    v_ctrl = (ref_pos[:, 1] - ref_pos[:, 0]).mean(axis=0)[:2]
    v_model = (rest_palms[1] - rest_palms[0])[:2]
    yaw = math.atan2(v_model[1], v_model[0]) - math.atan2(v_ctrl[1], v_ctrl[0])
    c, s = math.cos(yaw), math.sin(yaw)
    Rz = np.array([[c, -s, 0], [s, c, 0], [0, 0, 1]])
    pos = pos @ Rz.T

    hum_l = mujoco.mj_name2id(m0, mujoco.mjtObj.mjOBJ_BODY, "humerus_l")
    hum_r = mujoco.mj_name2id(m0, mujoco.mjtObj.mjOBJ_BODY, "humerus_r")
    sh_l, sh_r = d0.xpos[hum_l].copy(), d0.xpos[hum_r].copy()
    anchor = 0.5 * (sh_l + sh_r) + np.array([0.38, 0.0, -0.10])
    if calib_pos is not None:
        cal_rot = calib_pos @ Rz.T
        bi_center = 0.5 * (np.median(cal_rot[:, 0], axis=0) + np.median(cal_rot[:, 1], axis=0))
    else:
        bi_center = 0.5 * (np.median(pos[:, 0], axis=0) + np.median(pos[:, 1], axis=0))
    if fit:
        rel = pos - bi_center
        p95 = float(np.percentile(np.linalg.norm(rel, axis=2), 95))
        sc = min(1.0, 0.36 / max(p95, 1e-6))
        pos = bi_center + rel * sc
        print("fit: scaled hand targets by %.2f (p95 was %.2f m)" % (sc, p95))
    pos = pos + (anchor - bi_center)

    # Fit steering wheel centre, column tilt, and radius directly to the window's hands.
    flat3 = pos.reshape(-1, 3)
    wheel_center = anchor.copy()
    _, _, vt = np.linalg.svd(flat3 - wheel_center, full_matrices=False)
    n_svd = vt[2] if vt[2][0] >= 0 else -vt[2]
    wheel_tilt_rad = float(np.clip(math.atan2(-float(n_svd[2]), float(n_svd[0])), 0.08, 0.38))
    wheel_normal = np.array([math.cos(wheel_tilt_rad), 0.0, -math.sin(wheel_tilt_rad)])
    _, _, raw_rads = rim_distance_3d(flat3, wheel_center, wheel_normal, 0.145)
    wheel_radius = float(np.clip(np.median(raw_rads), 0.138, 0.185))
    raw_d, in_pl, rads = rim_distance_3d(pos, wheel_center, wheel_normal, wheel_radius)
    print("wheel fitted: centre=%s radius=%.3f m tilt=%.1f deg (raw controller->rim p50=%.1f cm)"
          % (np.round(wheel_center, 3), wheel_radius, math.degrees(wheel_tilt_rad),
             float(np.percentile(raw_d, 50)) * 100.0))

    # Compensate VR controller housing offset onto the rim when the hand is near the wheel
    # (< 16.5 cm), while smoothly releasing to free 3-D tracking if the hand leaves the wheel.
    snapped_count = 0
    if not raw_hands:
        for i in range(pos.shape[0]):
            for side_k, zsign in ((0, 1.0), (1, -1.0)):
                d_to_rim = float(raw_d[i, side_k])
                if d_to_rim < 0.165:
                    rn = float(rads[i, side_k])
                    u_r = in_pl[i, side_k] / rn if rn > 0.025 else np.array([0.0, zsign, 0.0])
                    p_on_rim = wheel_center + wheel_radius * u_r
                    if d_to_rim <= 0.115:
                        alpha = 1.0
                    else:
                        alpha = 0.5 * (1.0 + math.cos(math.pi * (d_to_rim - 0.115) / 0.05))
                    pos[i, side_k] = alpha * p_on_rim + (1.0 - alpha) * pos[i, side_k]
                    if alpha > 0.5:
                        snapped_count += 1

    m = build_model(model_path, wheel_center, wheel_radius, wheel_tilt_rad)
    d = mujoco.MjData(m)
    couplings = build_couplings(m)
    mujoco.mj_resetData(m, d)
    apply_coupled_joints(d, couplings)
    mujoco.mj_forward(m, d)
    true_arm = float(np.linalg.norm(
        d.site_xpos[mujoco.mj_name2id(m, mujoco.mjtObj.mjOBJ_SITE, "palm_l")] -
        d.xpos[mujoco.mj_name2id(m, mujoco.mjtObj.mjOBJ_BODY, "humerus_l")]))
    max_reach = round(0.92 * true_arm, 3)

    seat_posture(m, d, couplings)
    hl = mujoco.mj_name2id(m, mujoco.mjtObj.mjOBJ_BODY, "hand_l")
    hr = mujoco.mj_name2id(m, mujoco.mjtObj.mjOBJ_BODY, "hand_r")
    sl = mujoco.mj_name2id(m, mujoco.mjtObj.mjOBJ_SITE, "palm_l")
    sr = mujoco.mj_name2id(m, mujoco.mjtObj.mjOBJ_SITE, "palm_r")
    sh_l = d.xpos[mujoco.mj_name2id(m, mujoco.mjtObj.mjOBJ_BODY, "humerus_l")].copy()
    sh_r = d.xpos[mujoco.mj_name2id(m, mujoco.mjtObj.mjOBJ_BODY, "humerus_r")].copy()
    wj = mujoco.mj_name2id(m, mujoco.mjtObj.mjOBJ_JOINT, "wheel_steer")
    wheel_qadr = int(m.jnt_qposadr[wj]) if wj >= 0 else -1
    wheel_dof = int(m.jnt_dofadr[wj]) if wj >= 0 else -1
    dofs = {"l": arm_dofs(m, "l"), "r": arm_dofs(m, "r")}

    clamped = 0
    for i in range(pos.shape[0]):
        for k, sh in ((0, sh_l), (1, sh_r)):
            v = pos[i, k] - sh
            n = float(np.linalg.norm(v))
            if n > max_reach:
                pos[i, k] = sh + v * (max_reach / n)
                clamped += 1

    t_targets = np.arange(float(times[0]), float(times[-1]), 1.0 / fps)
    if t_targets.size == 0:
        raise SystemExit("empty time window; check --start/seconds against the CSV span")
    flat = pos.reshape(len(pos), -1)
    pos_t = np.stack([np.interp(t_targets, times, flat[:, k]) for k in range(flat.shape[1])], axis=1)
    pos_t = smooth_series(pos_t.reshape(len(t_targets), 2, 3), max(0.6, 0.025 * fps))
    # Re-project snapped rim points cleanly onto the circle after temporal smoothing.
    if not raw_hands:
        d_sm, in_sm, r_sm = rim_distance_3d(pos_t, wheel_center, wheel_normal, wheel_radius)
        for i in range(len(pos_t)):
            for k in (0, 1):
                if d_sm[i, k] < 0.04 and r_sm[i, k] > 0.03:
                    pos_t[i, k] = wheel_center + wheel_radius * (in_sm[i, k] / r_sm[i, k])

    if steers[0] is not None:
        steer_arr = np.interp(t_targets, times, np.asarray(steers, float))
        wheel_angles = smooth_series(steer_arr * MAX_WHEEL_RAD, max(0.6, 0.025 * fps))
    else:
        chord = pos_t[:, 0] - pos_t[:, 1]
        ang = np.unwrap(np.arctan2(chord[:, 2], chord[:, 1]))
        wheel_angles = smooth_series(ang - np.median(ang), max(0.8, 0.03 * fps))
    print("window %.2f..%.2f s at %d fps (%d frames)" % (float(times[0]), float(times[-1]), fps, len(pos_t)))

    res_l, res_r, palm_rim_d, q_all, qtrack = [], [], [], [], []
    arm_q = [x[0] for side in ("l", "r") for x in dofs[side]]
    for idx, (tl, tr) in enumerate(pos_t):
        if wheel_qadr >= 0:
            d.qpos[wheel_qadr] = float(wheel_angles[idx])
        res_l.append(solve_ik(m, d, "l", dofs["l"], sl, hl, tl, wheel_center, wheel_normal, couplings))
        res_r.append(solve_ik(m, d, "r", dofs["r"], sr, hr, tr, wheel_center, wheel_normal, couplings))
        pr_d, _, _ = rim_distance_3d(np.array([d.site_xpos[sl], d.site_xpos[sr]]),
                                     wheel_center, wheel_normal, wheel_radius)
        palm_rim_d.extend(pr_d.tolist())
        q_all.append(d.qpos.copy())
        qtrack.append(d.qpos[arm_q].copy())

    all_arm_dofs = dofs["l"] + dofs["r"]
    acts = (solve_muscle_activations(m, d, np.asarray(q_all), 1.0 / fps, couplings,
                                     all_arm_dofs, wheel_dof, (sl, sr))
            if show_muscle else np.zeros((len(pos_t), m.nu)))

    dense_mom = np.zeros((m.nu, m.nv))
    mujoco.mju_sparse2dense(dense_mom, d.actuator_moment, d.moment_rownnz, d.moment_rowadr, d.moment_colind)
    G_all = coupling_matrix(m, d, couplings, all_arm_dofs)
    upper_mask = np.linalg.norm(dense_mom @ G_all, axis=1) > 1e-4
    base_width = m.tendon_width.copy()

    renderer = mujoco.Renderer(m, 720, 720)
    cam = mujoco.MjvCamera()
    cam.lookat[:] = [0.08, 0.0, 0.96]
    cam.distance, cam.elevation, cam.azimuth = 1.45, -6, 205

    imgs = []
    for idx in range(len(pos_t)):
        d.qpos[:] = q_all[idx]
        mujoco.mj_forward(m, d)
        if show_muscle:
            apply_muscle_colors(m, acts[idx], upper_mask, base_width)
        renderer.update_scene(d, camera=cam)
        imgs.append(renderer.render())

    import imageio.v2 as imageio
    imageio.mimsave(out_path, imgs, fps=fps, macro_block_size=None)
    from PIL import Image
    for tag, k in (("first", 0), ("mid", len(imgs) // 2), ("last", len(imgs) - 1)):
        Image.fromarray(imgs[k]).save(out_path.replace(".mp4", "_%s.png" % tag))

    q = np.array(qtrack)
    excursion = np.degrees(q.max(axis=0) - q.min(axis=0))
    print("joint excursion over the clip (deg): " + ", ".join("%.0f" % v for v in excursion))
    p50 = np.percentile(np.r_[res_l, res_r], 50) * 100
    p95 = np.percentile(np.r_[res_l, res_r], 95) * 100
    rim_p50 = np.percentile(palm_rim_d, 50) * 100
    rim_p95 = np.percentile(palm_rim_d, 95) * 100
    print("frames=%d  IK residual: p50=%.1f cm  p95=%.1f cm  max=%.1f cm"
          % (len(imgs), p50, p95, max(res_l + res_r) * 100))
    print("palm-to-rim 3D distance: p50=%.1f cm  p95=%.1f cm  (rim-offset compensated: %d/%d, %.1f%%)"
          % (rim_p50, rim_p95, snapped_count, pos.shape[0] * 2, 100.0 * snapped_count / (pos.shape[0] * 2)))
    print("targets clamped to %.2f m reach: %d of %d (%.1f%%)"
          % (max_reach, clamped, pos.shape[0] * 2, 100.0 * clamped / (pos.shape[0] * 2)))
    if show_muscle and np.any(acts > 0):
        peaks = acts.max(axis=0)
        means = acts.mean(axis=0)
        top_idx = np.argsort(-means)[:6]
        summary = [
            "%s(mean=%.0f%%,peak=%.0f%%)"
            % (mujoco.mj_id2name(m, mujoco.mjtObj.mjOBJ_ACTUATOR, int(i)), means[i] * 100, peaks[i] * 100)
            for i in top_idx if peaks[i] > 0.01
        ]
        print("top recruited muscles: " + ", ".join(summary))
    print("wrote", out_path)


if __name__ == "__main__":
    main()
