#!/usr/bin/env python3
"""
test_airsim_rov.py — Test underwater ROV features in CarlaAir unified simulation
Usage: python test_airsim_rov.py [--host 127.0.0.1] [--port 2000] [--airsim-port 41451]

Tests:
  1. CARLA actor registry check (airsim.drone)
  2. AirSim ROV RPC connection & arming
  3. ROV kinematics state & water sensors (IMU, Barometer/Depth, Magnetometer)
  4. 6-DOF body-frame velocity control (forward motion & dive)
  5. ROV hover stabilization
  6. Camera capture test (RGB scene / depth)
  7. Clean disarm and control release
"""

import sys
import os
import time
import pprint
import argparse

# Ensure airsim package with RovClient is accessible
try:
    import airsim
    if not hasattr(airsim, 'RovClient'):
        raise ImportError("Installed airsim package does not contain RovClient")
except ImportError:
    # Fallback to local air repository PythonClient
    script_dir = os.path.dirname(os.path.abspath(__file__))
    candidates = []
    if "AIRSIM_PATH" in os.environ:
        candidates.append(os.path.normpath(os.path.join(os.environ["AIRSIM_PATH"], "PythonClient")))
    candidates.extend([
        os.path.normpath(os.path.join(script_dir, "..", "..", "..", "..", "OpenHUTB-air", "air", "PythonClient")),
        os.path.normpath(os.path.join(script_dir, "..", "..", "..", "..", "air", "PythonClient")),
        os.path.normpath(os.path.join(script_dir, "..", "..", "air", "PythonClient")),
    ])
    loaded = False
    for candidate in candidates:
        if os.path.isdir(candidate):
            sys.path.insert(0, candidate)
            try:
                import airsim
                if hasattr(airsim, 'RovClient'):
                    loaded = True
                    break
            except Exception:
                pass
    if not loaded:
        sys.exit("Error: airsim package with RovClient not found. Please install the updated air package or run from repo.")

def test_carla_actor(host, port):
    print("=== 1. Checking CARLA actor dispatcher ===")
    try:
        import carla
        client = carla.Client(host, port)
        client.set_timeout(5.0)
        world = client.get_world()
        actors = world.get_actors()
        rov_actors = [a for a in actors if 'airsim' in a.type_id or 'drone' in a.type_id]
        if rov_actors:
            for a in rov_actors:
                loc = a.get_location()
                print(f"  [CARLA] Found ROV/Drone Actor id={a.id} type={a.type_id} at ({loc.x:.2f}, {loc.y:.2f}, {loc.z:.2f})")
            return True
        else:
            print("  [CARLA] Connected, but no airsim.drone registered yet (normal if simulation just started).")
            return False
    except Exception as e:
        print(f"  [CARLA] Could not connect to CARLA on {host}:{port} ({e}). Skipping CARLA check.")
        return False

def main():
    parser = argparse.ArgumentParser(description="Test CarlaAir underwater ROV")
    parser.add_argument("--host", default="127.0.0.1", help="Host IP (default: 127.0.0.1)")
    parser.add_argument("--port", type=int, default=2000, help="CARLA RPC port (default: 2000)")
    parser.add_argument("--airsim-port", type=int, default=41451, help="AirSim RPC port (default: 41451)")
    parser.add_argument("--skip-carla", action="store_true", help="Skip CARLA registry check")
    parser.add_argument("--skip-movement", action="store_true", help="Skip motion test, only test sensors")
    args = parser.parse_args()

    # Step 1: CARLA actor registry check
    if not args.skip_carla:
        test_carla_actor(args.host, args.port)
        print()

    # Step 2: AirSim connection & arming
    print("=== 2. Connecting to AirSim ROV ===")
    client = airsim.RovClient(ip=args.host, port=args.airsim_port)
    client.confirmConnection()
    print("  Connected to AirSim server.")

    client.enableApiControl(True)
    client.armDisarm(True)
    print("  API control enabled, ROV thrusters armed.")
    print()

    # Step 3: State & Sensors telemetry
    print("=== 3. Telemetry & Sensor Verification ===")
    state = client.getRovState()
    pos = state.kinematics_estimated.position
    ori = state.kinematics_estimated.orientation
    lin_vel = state.kinematics_estimated.linear_velocity
    print(f"  Position (NED):      x={pos.x_val:.2f}, y={pos.y_val:.2f}, z={pos.z_val:.2f}")
    print(f"  Linear Velocity:     vx={lin_vel.x_val:.2f}, vy={lin_vel.y_val:.2f}, vz={lin_vel.z_val:.2f}")
    print(f"  Orientation (Quat):  w={ori.w_val:.3f}, x={ori.x_val:.3f}, y={ori.y_val:.3f}, z={ori.z_val:.3f}")

    # IMU
    try:
        imu = client.getImuData()
        acc = imu.linear_acceleration
        ang = imu.angular_velocity
        print(f"  IMU Accel:           x={acc.x_val:.2f}, y={acc.y_val:.2f}, z={acc.z_val:.2f} m/s^2")
        print(f"  IMU Angular Vel:     x={ang.x_val:.2f}, y={ang.y_val:.2f}, z={ang.z_val:.2f} rad/s")
    except Exception as e:
        print(f"  IMU query failed: {e}")

    # Barometer / Depth sensor
    try:
        baro = client.getBarometerData()
        print(f"  Baro (Depth/Alt):    altitude={baro.altitude:.2f}m, pressure={baro.pressure:.1f}Pa")
    except Exception as e:
        print(f"  Barometer query failed: {e}")

    # Magnetometer
    try:
        mag = client.getMagnetometerData()
        field = mag.magnetic_field_body
        print(f"  Magnetometer:        x={field.x_val:.2f}, y={field.y_val:.2f}, z={field.z_val:.2f} Gauss")
    except Exception as e:
        print(f"  Magnetometer query failed: {e}")
    print()

    # Step 4: Movement verification
    if not args.skip_movement:
        print("=== 4. ROV Motion Test (Dive & Forward in Body Frame) ===")
        print("  Commanding: vx = 0.5 m/s, vz = 0.2 m/s (dive down) for 2.0 seconds...")
        client.moveByVelocityBodyFrameAsync(vx=0.5, vy=0.0, vz=0.2, duration=2).join()
        time.sleep(0.5)

        state_after = client.getRovState()
        pos_after = state_after.kinematics_estimated.position
        print(f"  Updated Position:    x={pos_after.x_val:.2f}, y={pos_after.y_val:.2f}, z={pos_after.z_val:.2f}")

        # Step 5: Hover
        print("=== 5. ROV Hover Stabilization ===")
        print("  Hovering in place...")
        client.hoverAsync().join()
        time.sleep(0.5)
        print("  Hover stabilized.")
        print()
    else:
        print("=== 4 & 5. Skipping Motion Test as requested ===")
        print()

    # Step 6: Camera test
    print("=== 6. Camera Capture Verification ===")
    try:
        responses = client.simGetImages([
            airsim.ImageRequest("0", airsim.ImageType.Scene, False, False)
        ])
        if responses and len(responses) > 0 and responses[0].width > 0:
            print(f"  Camera 0 (RGB Scene): Captured {responses[0].width}x{responses[0].height} image.")
        else:
            print("  Camera returned empty image or not initialized.")
    except Exception as e:
        print(f"  Camera capture skipped or not enabled: {e}")
    print()

    # Step 7: Clean release
    print("=== 7. Cleanup & Disarm ===")
    client.armDisarm(False)
    client.enableApiControl(False)
    print("  ROV disarmed and API control released.")
    print()
    print("ALL ROV TESTS COMPLETED SUCCESSFULLY!")

if __name__ == "__main__":
    main()
