"""Record both SteamVR controller poses to CSV.

SteamVR must already be running with the headset on. This script does NOT start
the simulator, so it can run on its own.

Usage:
  python record_hands.py hands.csv [seconds] [hz]

CSV columns:
  host_time_s, {l,r}_{valid,connected,x,y,z,qw,qx,qy,qz,trigger,grip},
  hmd_{valid,x,y,z,qw,qx,qy,qz}

Pose is the controller (grip) frame in the standing tracking space, metres.
trigger/grip come from rAxis[0].x / rAxis[1].x (0..1, -1 when unknown).
"""
import sys, time, csv, math
import openvr


def quat_from_matrix(m):
    r00, r01, r02 = m[0][0], m[0][1], m[0][2]
    r10, r11, r12 = m[1][0], m[1][1], m[1][2]
    r20, r21, r22 = m[2][0], m[2][1], m[2][2]
    tr = r00 + r11 + r22
    if tr > 0:
        s = math.sqrt(tr + 1.0) * 2
        return 0.25 * s, (r21 - r12) / s, (r02 - r20) / s, (r10 - r01) / s
    if r00 > r11 and r00 > r22:
        s = math.sqrt(1.0 + r00 - r11 - r22) * 2
        return (r21 - r12) / s, 0.25 * s, (r01 + r10) / s, (r02 + r20) / s
    if r11 > r22:
        s = math.sqrt(1.0 + r11 - r00 - r22) * 2
        return (r02 - r20) / s, (r01 + r10) / s, 0.25 * s, (r12 + r21) / s
    s = math.sqrt(1.0 + r22 - r00 - r11) * 2
    return (r10 - r01) / s, (r02 + r20) / s, (r12 + r21) / s, 0.25 * s


HEADER = ["host_time_s"]
for side in ("l", "r"):
    HEADER += ["%s_%s" % (side, k) for k in
               ("valid", "connected", "x", "y", "z", "qw", "qx", "qy", "qz", "trigger", "grip")]
HEADER += ["hmd_" + k for k in ("valid", "x", "y", "z", "qw", "qx", "qy", "qz")]


def main():
    out = sys.argv[1] if len(sys.argv) > 1 else "hands.csv"
    seconds = float(sys.argv[2]) if len(sys.argv) > 2 else 60.0
    hz = float(sys.argv[3]) if len(sys.argv) > 3 else 90.0
    vr = openvr.init(openvr.VRApplication_Background)
    poses = (openvr.TrackedDevicePose_t * openvr.k_unMaxTrackedDeviceCount)()
    period = 1.0 / max(1.0, hz)
    try:
        time.sleep(0.5)
        ids = (vr.getTrackedDeviceIndexForControllerRole(openvr.TrackedControllerRole_LeftHand),
               vr.getTrackedDeviceIndexForControllerRole(openvr.TrackedControllerRole_RightHand))
        print("controllers: left=%d right=%d" % ids)
        n = 0
        t0 = time.time()
        next_t = t0
        with open(out, "w", newline="", encoding="utf-8") as f:
            w = csv.writer(f)
            w.writerow(HEADER)
            while True:
                now = time.time()
                if now - t0 >= seconds:
                    break
                if now < next_t:
                    time.sleep(next_t - now)
                    continue
                next_t += period
                vr.getDeviceToAbsoluteTrackingPose(openvr.TrackingUniverseStanding, 0, poses)
                row = ["%.4f" % (now - t0)]
                for idx in ids:
                    if idx == openvr.k_unTrackedDeviceIndexInvalid:
                        row += ["0", "0", "", "", "", "", "", "", "", "", ""]
                        continue
                    p = poses[idx]
                    valid = 1 if p.bPoseIsValid else 0
                    conn = 1 if p.bDeviceIsConnected else 0
                    if valid:
                        m = p.mDeviceToAbsoluteTracking
                        wq, xq, yq, zq = quat_from_matrix(m)
                        pos = ["%.4f" % m[0][3], "%.4f" % m[1][3], "%.4f" % m[2][3]]
                        quat = ["%.5f" % v for v in (wq, xq, yq, zq)]
                    else:
                        pos, quat = ["", "", ""], ["", "", "", ""]
                    result, state = vr.getControllerState(idx)
                    trig = "%.3f" % state.rAxis[0].x if result else ""
                    grip = "%.3f" % state.rAxis[1].x if result else ""
                    row += [str(valid), str(conn)] + pos + quat + [trig, grip]
                hp = poses[openvr.k_unTrackedDeviceIndex_Hmd]
                if hp.bPoseIsValid:
                    hm = hp.mDeviceToAbsoluteTracking
                    hw, hx, hy, hz = quat_from_matrix(hm)
                    row += ["1", "%.4f" % hm[0][3], "%.4f" % hm[1][3], "%.4f" % hm[2][3],
                            "%.5f" % hw, "%.5f" % hx, "%.5f" % hy, "%.5f" % hz]
                else:
                    row += ["0", "", "", "", "", "", "", ""]
                w.writerow(row)
                n += 1
        print("wrote %d rows to %s" % (n, out))
    finally:
        openvr.shutdown()


if __name__ == "__main__":
    main()
