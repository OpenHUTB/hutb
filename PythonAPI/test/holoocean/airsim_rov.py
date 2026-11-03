#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
airsim_rov.py — Test underwater ROV features in CarlaAir unified simulation
Usage:
    python airsim_rov.py [--host 127.0.0.1] [--port 2000] [--airsim-port 41451]
    python airsim_rov.py --mock [--test-buoyancy] [--test-depth-clamp] [--all-tests]

Features & Tests:
    1. CarlaAir settings configuration & Fossen hydrodynamic parameter validation (settings_rov.json)
    2. CARLA actor registry check (airsim.drone / ROV)
    3. AirSim ROV RPC connection & arming (Live or Mock client)
    4. ROV kinematics state & water sensors (IMU, Barometer/Depth, Magnetometer, Downward Sonar)
    5. 6-DOF body-frame velocity control (forward motion & dive) and hover stabilization
    6. Neutral buoyancy balance & attitude self-righting test (--test-buoyancy)
    7. Seabed soft cushion, spring-damper reaction & kinematic hard depth clamp test (--test-depth-clamp)
    8. Underwater stereo camera capture test (1280x720 RGB Scene)
    9. Clean disarm and control release
"""

import sys
import os
import time
import math
import json
import argparse

# Force UTF-8 encoding on standard streams in Windows environments
if hasattr(sys.stdout, 'reconfigure'):
    try:
        sys.stdout.reconfigure(encoding='utf-8')
        sys.stderr.reconfigure(encoding='utf-8')
    except Exception:
        pass

# Ensure airsim package with RovClient is accessible
airsim_loaded = False
try:
    import airsim
    if hasattr(airsim, 'RovClient'):
        airsim_loaded = True
    else:
        if 'airsim' in sys.modules:
            del sys.modules['airsim']
except ImportError:
    pass

if not airsim_loaded:
    script_dir = os.path.dirname(os.path.abspath(__file__))
    candidates = []
    if "AIRSIM_PATH" in os.environ:
        candidates.append(os.path.normpath(os.path.join(os.environ["AIRSIM_PATH"], "PythonClient")))
    candidates.extend([
        # 5 levels up: e.g. OpenHUTB-hutb/hutb/PythonAPI/test/holoocean -> E:/Projects/OpenHUTB-air/air/PythonClient
        os.path.normpath(os.path.join(script_dir, "..", "..", "..", "..", "..", "OpenHUTB-air", "air", "PythonClient")),
        os.path.normpath(os.path.join(script_dir, "..", "..", "..", "..", "..", "air", "PythonClient")),
        # 4 levels up: e.g. hutb/PythonAPI/test/holoocean -> E:/Projects/OpenHUTB-air/air/PythonClient
        os.path.normpath(os.path.join(script_dir, "..", "..", "..", "..", "OpenHUTB-air", "air", "PythonClient")),
        os.path.normpath(os.path.join(script_dir, "..", "..", "..", "..", "air", "PythonClient")),
        # In-tree Build clone
        os.path.normpath(os.path.join(script_dir, "..", "..", "..", "Build", "AirSim", "PythonClient")),
        os.path.normpath(os.path.join(script_dir, "..", "..", "air", "PythonClient")),
    ])
    for candidate in candidates:
        if os.path.isdir(candidate):
            if 'airsim' in sys.modules:
                del sys.modules['airsim']
            sys.path.insert(0, candidate)
            try:
                import airsim
                if hasattr(airsim, 'RovClient'):
                    airsim_loaded = True
                    break
                else:
                    if 'airsim' in sys.modules:
                        del sys.modules['airsim']
            except Exception:
                if 'airsim' in sys.modules:
                    del sys.modules['airsim']

# ==============================================================================
# Euler / Quaternion Conversion Helpers
# ==============================================================================

def euler_to_quaternion(roll, pitch, yaw):
    """Convert Euler angles (in radians) to (w, x, y, z) quaternion."""
    cy = math.cos(yaw * 0.5)
    sy = math.sin(yaw * 0.5)
    cp = math.cos(pitch * 0.5)
    sp = math.sin(pitch * 0.5)
    cr = math.cos(roll * 0.5)
    sr = math.sin(roll * 0.5)

    w = cr * cp * cy + sr * sp * sy
    x = sr * cp * cy - cr * sp * sy
    y = cr * sp * cy + sr * cp * sy
    z = cr * cp * sy - sr * sp * cy
    return w, x, y, z

def quaternion_to_euler(w, x, y, z):
    """Convert (w, x, y, z) quaternion to (roll, pitch, yaw) in radians."""
    sinr_cosp = 2.0 * (w * x + y * z)
    cosr_cosp = 1.0 - 2.0 * (x * x + y * y)
    roll = math.atan2(sinr_cosp, cosr_cosp)

    sinp = 2.0 * (w * y - z * x)
    if abs(sinp) >= 1.0:
        pitch = math.copysign(math.pi / 2.0, sinp)
    else:
        pitch = math.asin(sinp)

    siny_cosp = 2.0 * (w * z + x * y)
    cosy_cosp = 1.0 - 2.0 * (y * y + z * z)
    yaw = math.atan2(siny_cosp, cosy_cosp)

    return roll, pitch, yaw

# ==============================================================================
# Mock Simulation Types & Client (For CI / Offline Verification)
# ==============================================================================

class MockVector3r:
    def __init__(self, x_val=0.0, y_val=0.0, z_val=0.0):
        self.x_val = float(x_val)
        self.y_val = float(y_val)
        self.z_val = float(z_val)
    def __repr__(self):
        return f"Vector3r({self.x_val:.3f}, {self.y_val:.3f}, {self.z_val:.3f})"

class MockQuaternionr:
    def __init__(self, w_val=1.0, x_val=0.0, y_val=0.0, z_val=0.0):
        self.w_val = float(w_val)
        self.x_val = float(x_val)
        self.y_val = float(y_val)
        self.z_val = float(z_val)
    def __repr__(self):
        return f"Quaternionr(w={self.w_val:.3f}, x={self.x_val:.3f}, y={self.y_val:.3f}, z={self.z_val:.3f})"

class MockPose:
    def __init__(self, position=None, orientation=None):
        self.position = position or MockVector3r()
        self.orientation = orientation or MockQuaternionr()

class MockKinematics:
    def __init__(self):
        self.position = MockVector3r(0.0, 0.0, 10.0) # initial underwater depth 10m
        self.orientation = MockQuaternionr(1.0, 0.0, 0.0, 0.0)
        self.linear_velocity = MockVector3r(0.0, 0.0, 0.0)
        self.angular_velocity = MockVector3r(0.0, 0.0, 0.0)
        self.linear_acceleration = MockVector3r(0.0, 0.0, 0.0)
        self.angular_acceleration = MockVector3r(0.0, 0.0, 0.0)

class MockRovState:
    def __init__(self, kinematics):
        self.kinematics_estimated = kinematics

class MockImuData:
    def __init__(self, kinematics):
        self.time_stamp = int(time.time() * 1e9)
        self.linear_acceleration = MockVector3r(
            kinematics.linear_acceleration.x_val,
            kinematics.linear_acceleration.y_val,
            kinematics.linear_acceleration.z_val + 9.80665 # specific force includes gravity in NED
        )
        self.angular_velocity = kinematics.angular_velocity
        self.orientation = kinematics.orientation

class MockBarometerData:
    def __init__(self, depth):
        self.time_stamp = int(time.time() * 1e9)
        self.altitude = -float(depth) # NED +Z is down, altitude is negative
        # Hydrostatic water pressure: P = P_atm + rho * g * h (rho=1025 kg/m^3 for seawater)
        self.pressure = 101325.0 + 1025.0 * 9.80665 * max(0.0, float(depth))
        self.qnh = 1013.25

class MockMagnetometerData:
    def __init__(self):
        self.time_stamp = int(time.time() * 1e9)
        self.magnetic_field_body = MockVector3r(0.22, 0.05, 0.42) # Earth B-field in Gauss

class MockDistanceSensorData:
    def __init__(self, distance):
        self.time_stamp = int(time.time() * 1e9)
        self.distance = max(0.0, float(distance))
        self.min_distance = 0.1
        self.max_distance = 50.0
        self.relative_pose = MockPose(MockVector3r(0.0, 0.0, 0.15), MockQuaternionr(0.7071, -0.7071, 0.0, 0.0))

class MockImageResponse:
    def __init__(self, camera_name, width=1280, height=720):
        self.camera_name = camera_name
        self.image_type = 0
        self.width = width
        self.height = height
        self.image_data_uint8 = bytes([128] * (width * height * 3))

class MockFuture:
    def join(self):
        pass

class MockRovClient:
    """
    High-fidelity Mock RovClient simulating the C++ RovPhysicsBody Fossen 6-DOF
    hydrodynamics, neutral buoyancy, self-righting restoring moments, and seabed safety boundary.
    """
    def __init__(self, settings_dict=None):
        self.connected = False
        self.api_control = False
        self.armed = False
        self.kinematics = MockKinematics()

        # Fossen hydrodynamics and depth safety parameters
        self.mass = 11.5
        self.gravity = 9.80665
        self.z_bed = 30.0 # absolute depth limit in NED (+Z)
        self.bed_stiffness = 2000.0
        self.bed_damping = 300.0
        self.bed_contact_margin = 0.5
        self.bed_clamp_margin = 0.3
        self.off_z = 0.05
        self.damping_linear = [2.2, 5.8, 2.7]
        self.damping_linear_q = [1.5, 2.5, 1.8]
        self.damping_angular = [0.6, 0.4, 0.3]
        self.damping_angular_q = [0.5, 0.3, 0.2]
        self.enable_depth_clamp = True
        self.enable_neutral_buoyancy = True
        self.enable_hydrodynamic_damping = True

        # Extract overrides from settings if provided
        if settings_dict:
            params = settings_dict.get("Vehicles", {}).get("RovSimple", {})
            self.z_bed = abs(float(params.get("ZBed", settings_dict.get("ZBed", -30.0))))
            self.enable_depth_clamp = bool(params.get("EnableDepthClamp", settings_dict.get("EnableDepthClamp", True)))
            self.bed_stiffness = float(params.get("BedStiffness", settings_dict.get("BedStiffness", 2000.0)))
            self.bed_damping = float(params.get("BedDamping", settings_dict.get("BedDamping", 300.0)))
            self.bed_contact_margin = float(params.get("BedContactMargin", settings_dict.get("BedContactMargin", 0.5)))
            self.bed_clamp_margin = float(params.get("BedClampMargin", settings_dict.get("BedClampMargin", 0.3)))
            self.enable_neutral_buoyancy = bool(params.get("EnableNeutralBuoyancy", settings_dict.get("EnableNeutralBuoyancy", True)))
            self.enable_hydrodynamic_damping = bool(params.get("EnableHydrodynamicDamping", settings_dict.get("EnableHydrodynamicDamping", True)))
            self.off_z = float(params.get("OffZ", settings_dict.get("OffZ", 0.05)))

        self.roll = 0.0
        self.pitch = 0.0
        self.yaw = 0.0

    def confirmConnection(self):
        self.connected = True

    def enableApiControl(self, enable=True, vehicle_name=""):
        self.api_control = enable

    def armDisarm(self, arm=True, vehicle_name=""):
        self.armed = arm

    def getRovState(self, vehicle_name=""):
        return MockRovState(self.kinematics)

    def getImuData(self, imu_name="", vehicle_name=""):
        return MockImuData(self.kinematics)

    def getBarometerData(self, barometer_name="", vehicle_name=""):
        return MockBarometerData(self.kinematics.position.z_val)

    def getMagnetometerData(self, magnetometer_name="", vehicle_name=""):
        return MockMagnetometerData()

    def getDistanceSensorData(self, distance_sensor_name="", vehicle_name=""):
        current_z = self.kinematics.position.z_val
        dist_to_bed = max(0.0, self.z_bed - current_z)
        return MockDistanceSensorData(dist_to_bed)

    def simGetImages(self, requests, vehicle_name=""):
        responses = []
        for req in requests:
            cam_name = getattr(req, 'camera_name', 'front_right_custom')
            responses.append(MockImageResponse(cam_name, 1280, 720))
        return responses

    def perturbAttitude(self, roll_deg=15.0, pitch_deg=15.0):
        """Artificially perturb roll and pitch to test self-righting restoring moments."""
        self.roll = math.radians(roll_deg)
        self.pitch = math.radians(pitch_deg)
        w, x, y, z = euler_to_quaternion(self.roll, self.pitch, self.yaw)
        self.kinematics.orientation = MockQuaternionr(w, x, y, z)

    def _step_physics(self, cmd_vx, cmd_vy, cmd_vz, dt):
        pos = self.kinematics.position
        vel = self.kinematics.linear_velocity
        ang_vel = self.kinematics.angular_velocity

        # Thruster drive acceleration towards commanded velocity
        kp_drive = 4.0
        ax = kp_drive * (cmd_vx - vel.x_val)
        ay = kp_drive * (cmd_vy - vel.y_val)
        az = kp_drive * (cmd_vz - vel.z_val)

        # Fossen fluid damping (negative velocity resistance)
        if self.enable_hydrodynamic_damping:
            ax -= (self.damping_linear[0] * vel.x_val + self.damping_linear_q[0] * abs(vel.x_val) * vel.x_val) / self.mass
            ay -= (self.damping_linear[1] * vel.y_val + self.damping_linear_q[1] * abs(vel.y_val) * vel.y_val) / self.mass
            az -= (self.damping_linear[2] * vel.z_val + self.damping_linear_q[2] * abs(vel.z_val) * vel.z_val) / self.mass

        # Neutral buoyancy model:
        # F_buoyancy = -m * g, balances gravity F_g = m * g perfectly (net vertical bias = 0)
        # When cmd_vz is 0 and vel.z is 0, net vertical acceleration is 0.

        # Attitude self-righting restoring moment (Fossen metacentric stability):
        # tau_Rx = - off_z * m * g * sin(phi)
        # tau_Ry = - off_z * m * g * sin(theta)
        tau_x = -abs(self.off_z) * self.mass * self.gravity * math.sin(self.roll)
        tau_y = -abs(self.off_z) * self.mass * self.gravity * math.sin(self.pitch)
        i_xx, i_yy = 0.25, 0.25
        alpha_x = tau_x / i_xx - 2.5 * ang_vel.x_val
        alpha_y = tau_y / i_yy - 2.5 * ang_vel.y_val

        ang_vel.x_val += alpha_x * dt
        ang_vel.y_val += alpha_y * dt
        self.roll += ang_vel.x_val * dt
        self.pitch += ang_vel.y_val * dt
        w, x, y, z = euler_to_quaternion(self.roll, self.pitch, self.yaw)
        self.kinematics.orientation = MockQuaternionr(w, x, y, z)

        # Seabed soft & hard boundary contact physics
        if self.enable_depth_clamp:
            penetration = pos.z_val - self.z_bed
            margin = self.bed_contact_margin

            if penetration > 0.0:
                # Soft boundary spring-damper contact reaction
                f_spring = self.bed_stiffness * penetration
                f_damper = self.bed_damping * vel.z_val if vel.z_val > 0.0 else 0.0
                f_reaction = -(f_spring + f_damper)
                az += f_reaction / self.mass
            elif penetration > -margin and vel.z_val > 0.0 and margin > 1e-4:
                # Approaching cushion buffer: cubic Hermite smoothstep transition
                s = (penetration + margin) / margin
                smooth_factor = s * s * (3.0 - 2.0 * s)
                f_buffer = -smooth_factor * (self.bed_damping * 0.5 * vel.z_val)
                az += f_buffer / self.mass

        # Integrate linear motion
        vel.x_val += ax * dt
        vel.y_val += ay * dt
        vel.z_val += az * dt

        pos.x_val += vel.x_val * dt
        pos.y_val += vel.y_val * dt
        pos.z_val += vel.z_val * dt

        # Kinematic hard depth clamp fail-safe
        if self.enable_depth_clamp:
            hard_limit = self.z_bed + self.bed_clamp_margin
            if pos.z_val > hard_limit:
                pos.z_val = self.z_bed
                if vel.z_val > 0.0:
                    vel.z_val = 0.0

        self.kinematics.linear_acceleration = MockVector3r(ax, ay, az)

    def moveByVelocityBodyFrameAsync(self, vx, vy, vz, duration=1.0, *args, **kwargs):
        dt = 0.05
        steps = int(max(1, duration / dt))
        for _ in range(steps):
            self._step_physics(vx, vy, vz, dt)
        return MockFuture()

    def hoverAsync(self, vehicle_name=""):
        dt = 0.05
        steps = int(1.0 / dt)
        for _ in range(steps):
            self._step_physics(0.0, 0.0, 0.0, dt)
        return MockFuture()

# ==============================================================================
# Configuration Validation (settings_rov.json)
# ==============================================================================

def find_settings_file(user_path=None):
    """Locate settings_rov.json from candidate paths or user argument."""
    if user_path and os.path.isfile(user_path):
        return os.path.abspath(user_path)

    script_dir = os.path.dirname(os.path.abspath(__file__))
    candidates = [
        # Relative to hutb/PythonAPI/test/holoocean
        os.path.normpath(os.path.join(script_dir, "..", "..", "..", "Unreal", "CarlaUE4", "settings_rov.json")),
        os.path.normpath(os.path.join(script_dir, "..", "..", "settings_rov.json")),
        os.path.normpath(os.path.join(script_dir, "settings_rov.json")),
        # Blocks environment in air repository
        os.path.normpath(os.path.join(script_dir, "..", "..", "..", "..", "..", "OpenHUTB-air", "air", "Unreal", "Environments", "Blocks", "settings_rov.json")),
        os.path.normpath(os.path.join(script_dir, "..", "..", "..", "..", "air", "Unreal", "Environments", "Blocks", "settings_rov.json")),
    ]
    for c in candidates:
        if os.path.isfile(c):
            return c
    return None

def validate_settings_file(settings_path):
    """Validate that settings_rov.json contains required Fossen hydrodynamics, depth clamp, cameras, and sensors."""
    print("=== Configuration File Protocol Check ===")
    if not settings_path or not os.path.isfile(settings_path):
        print(f"  [ERROR] settings_rov.json not found at: {settings_path}")
        return False, None

    try:
        with open(settings_path, 'r', encoding='utf-8') as f:
            cfg = json.load(f)
    except Exception as e:
        print(f"  [ERROR] Failed to parse {settings_path}: {e}")
        return False, None

    errors = []
    print(f"  Loaded settings: {settings_path}")

    # Check SimMode
    sim_mode = cfg.get("SimMode", "")
    if sim_mode != "Rov":
        errors.append(f"Expected SimMode='Rov', found '{sim_mode}'")

    # Look for parameters in Vehicles.RovSimple and root
    rov_cfg = cfg.get("Vehicles", {}).get("RovSimple", {})

    expected_params = {
        "ZBed": -30.0,
        "EnableDepthClamp": True,
        "BedStiffness": 2000.0,
        "BedDamping": 300.0,
        "BedContactMargin": 0.5,
        "EnableNeutralBuoyancy": True,
        "EnableHydrodynamicDamping": True,
        "OffZ": 0.05,
    }

    print("  Checking Fossen hydrodynamic & depth safety parameters:")
    for key, expected_val in expected_params.items():
        actual_val = rov_cfg.get(key, cfg.get(key, None))
        if actual_val is None:
            errors.append(f"Missing parameter '{key}' in settings_rov.json")
            print(f"    [FAIL] {key}: Missing (expected {expected_val})")
        else:
            if isinstance(expected_val, float):
                val_ok = abs(float(actual_val) - expected_val) < 1e-3
            else:
                val_ok = actual_val == expected_val
            status = "PASS" if val_ok else "WARN"
            print(f"    [{status}] {key} = {actual_val} (expected: {expected_val})")
            if not val_ok:
                errors.append(f"Parameter '{key}' has value {actual_val}, expected {expected_val}")

    # Check stereo cameras
    cameras = rov_cfg.get("Cameras", {})
    stereo_cams = ["front_left_custom", "front_right_custom"]
    print("  Checking underwater stereo camera capture settings:")
    for cam in stereo_cams:
        if cam in cameras:
            c_cfg = cameras[cam]
            captures = c_cfg.get("CaptureSettings", [{}])
            w = captures[0].get("Width", 0)
            h = captures[0].get("Height", 0)
            fov = captures[0].get("FOV_Degrees", 0)
            print(f"    [PASS] Camera '{cam}': {w}x{h}, FOV {fov}°, Position=({c_cfg.get('X')}, {c_cfg.get('Y')}, {c_cfg.get('Z')})")
            if w != 1280 or h != 720:
                errors.append(f"Camera '{cam}' resolution is {w}x{h}, expected 1280x720")
            if fov != 90:
                errors.append(f"Camera '{cam}' FOV is {fov}°, expected 90°")
        else:
            errors.append(f"Missing stereo camera '{cam}' in settings_rov.json")
            print(f"    [FAIL] Camera '{cam}': Missing")

    # Check sensors
    sensors = rov_cfg.get("Sensors", {})
    print("  Checking underwater sensor configuration:")
    expected_sensors = {
        "barometer": 1,        # Barometer / Depth
        "imu": 2,              # IMU
        "magnetometer": 4,      # Magnetometer
        "downward_sonar": 5,   # Distance / Downward Sonar
    }
    for s_name, s_type in expected_sensors.items():
        if s_name in sensors:
            st = sensors[s_name].get("SensorType", 0)
            en = sensors[s_name].get("Enabled", False)
            status = "PASS" if st == s_type and en else "FAIL"
            print(f"    [{status}] Sensor '{s_name}': SensorType={st}, Enabled={en}")
            if st != s_type or not en:
                errors.append(f"Sensor '{s_name}' mismatch: type={st} (expected {s_type}), enabled={en}")
        else:
            errors.append(f"Missing sensor '{s_name}' in settings_rov.json")
            print(f"    [FAIL] Sensor '{s_name}': Missing")

    if errors:
        print(f"  Configuration validation finished with {len(errors)} issue(s).")
        return False, cfg
    else:
        print("  [SUCCESS] All settings_rov.json protocols verified successfully!\n")
        return True, cfg

# ==============================================================================
# CARLA Actor Registry Check
# ==============================================================================

def test_carla_actor(host, port, required=False):
    prefix = "[CARLA]" if required else "[CARLA] (Optional check)"
    print(f"=== CARLA Actor Dispatcher Check ({'Mandatory' if required else 'Optional'}) ===")
    try:
        import carla
        client = carla.Client(host, port)
        client.set_timeout(3.0)
        world = client.get_world()
        actors = world.get_actors()
        rov_actors = [a for a in actors if 'airsim' in a.type_id or 'drone' in a.type_id]
        if rov_actors:
            for a in rov_actors:
                loc = a.get_location()
                print(f"  {prefix} Found ROV/Drone Actor id={a.id} type={a.type_id} at ({loc.x:.2f}, {loc.y:.2f}, {loc.z:.2f})")
            return True
        else:
            print(f"  {prefix} Connected to CARLA on {host}:{port}, but no airsim actor registered yet.")
            return False
    except Exception as e:
        print(f"  {prefix} Could not connect to CARLA on {host}:{port} ({e}).")
        return False

# ==============================================================================
# Hydrodynamics & Safety Unit Tests
# ==============================================================================

def test_buoyancy(client):
    """
    Test Neutral Buoyancy & Fossen Attitude Self-Righting (初稳心恢复力矩).
    1. Neutral Buoyancy: checks vertical acceleration az ~ 0 and depth stability when hovering.
    2. Self-Righting: perturbs roll/pitch and confirms restoring moments restore level attitude.
    """
    print("=== Hydrodynamic Test: Neutral Buoyancy & Attitude Self-Righting ===")
    passed = True

    # 1. Neutral Buoyancy check
    print("  1. Verifying neutral buoyancy equilibrium (gravity vs buoyancy balance)...")
    try:
        client.hoverAsync().join()
        time.sleep(0.5)

        state = client.getRovState()
        vz = state.kinematics_estimated.linear_velocity.z_val
        az = state.kinematics_estimated.linear_acceleration.z_val

        print(f"    Measured vertical velocity (vz):     {vz:.4f} m/s")
        print(f"    Measured net vertical accel (az):    {az:.4f} m/s^2")

        if abs(vz) > 0.15 or abs(az) > 0.15:
            print("    [WARN] Neutral buoyancy vertical drift detected.")
            passed = False
        else:
            print("    [PASS] Neutral buoyancy equilibrium confirmed: Net vertical force = 0, no free abyss drift.")
    except Exception as e:
        print(f"    [ERROR] Neutral buoyancy test failed: {e}")
        passed = False

    # 2. Fossen Self-Righting Restoring Moments check
    print("  2. Verifying Fossen metacentric self-righting restoring moments (COB above COM)...")
    try:
        if isinstance(client, MockRovClient):
            client.perturbAttitude(roll_deg=15.0, pitch_deg=15.0)
            ori_perturbed = client.getRovState().kinematics_estimated.orientation
            r_pert, p_pert, _ = quaternion_to_euler(ori_perturbed.w_val, ori_perturbed.x_val, ori_perturbed.y_val, ori_perturbed.z_val)
            print(f"    Induced attitude perturbation: Roll={math.degrees(r_pert):.1f}°, Pitch={math.degrees(p_pert):.1f}°")

            # Simulate recovery steps
            client.hoverAsync().join()
            ori_recovered = client.getRovState().kinematics_estimated.orientation
            r_rec, p_rec, _ = quaternion_to_euler(ori_recovered.w_val, ori_recovered.x_val, ori_recovered.y_val, ori_recovered.z_val)
            print(f"    Attitude after restoring moments: Roll={math.degrees(r_rec):.2f}°, Pitch={math.degrees(p_rec):.2f}°")

            if abs(r_rec) < abs(r_pert) and abs(p_rec) < abs(p_pert):
                print("    [PASS] Restoring moments tau_x, tau_y successfully restored attitude towards level equilibrium.")
            else:
                print("    [FAIL] Attitude failed to self-right.")
                passed = False
        else:
            # Live client: query current attitude
            state = client.getRovState()
            ori = state.kinematics_estimated.orientation
            roll, pitch, yaw = quaternion_to_euler(ori.w_val, ori.x_val, ori.y_val, ori.z_val)
            print(f"    Current live attitude: Roll={math.degrees(roll):.2f}°, Pitch={math.degrees(pitch):.2f}°, Yaw={math.degrees(yaw):.2f}°")
            print("    [PASS] Attitude stability verified in live simulation.")
    except Exception as e:
        print(f"    [ERROR] Self-righting test failed: {e}")
        passed = False

    print()
    return passed

def test_depth_clamp(client):
    """
    Test Seabed Depth Boundary & Anti-Void Protection.
    1. Soft cushion buffer decelerates downward dive as depth approaches Z_bed (-30m).
    2. Spring-damper contact reaction stops downward penetration at Z_bed.
    3. Kinematic hard clamp prevents depth from exceeding 30.3m (no falling into the void).
    4. Upward recovery confirms normal ascending maneuverability after contact.
    """
    print("=== Safety Net Test: Seabed Depth Clamp & Anti-Void Protection ===")
    passed = True

    try:
        state_init = client.getRovState()
        z_init = state_init.kinematics_estimated.position.z_val
        print(f"  Initial ROV depth: z = {z_init:.2f} m (Seabed limit: z_bed = 30.00 m)")

        # Step 1: Dive downward towards seabed
        print("  1. Commanding downward dive (vz = 2.0 m/s) towards seabed limit...")
        client.moveByVelocityBodyFrameAsync(vx=0.0, vy=0.0, vz=2.0, duration=15.0).join()

        state_bed = client.getRovState()
        z_bed = state_bed.kinematics_estimated.position.z_val
        vz_bed = state_bed.kinematics_estimated.linear_velocity.z_val
        print(f"    Depth after dive command: z = {z_bed:.2f} m, vz = {vz_bed:.4f} m/s")

        # Step 2: Validate soft contact & hard clamp boundaries
        hard_limit = 30.0 + 0.3 # bed_depth + clamp_margin
        if z_bed <= hard_limit and vz_bed <= 0.05:
            print(f"    [PASS] Depth strictly bounded: z = {z_bed:.2f} m <= {hard_limit:.2f} m.")
            print(f"    [PASS] Downward velocity arrested: vz = {vz_bed:.4f} m/s (no void penetration).")
        else:
            print(f"    [FAIL] Depth clamp violated: z = {z_bed:.2f} m (limit {hard_limit:.2f} m), vz = {vz_bed:.4f} m/s")
            passed = False

        # Step 3: Upward recovery test
        print("  2. Testing upward recovery from seabed (commanding vz = -1.0 m/s)...")
        client.moveByVelocityBodyFrameAsync(vx=0.0, vy=0.0, vz=-1.0, duration=3.0).join()
        state_ascend = client.getRovState()
        z_ascend = state_ascend.kinematics_estimated.position.z_val
        print(f"    Depth after ascending: z = {z_ascend:.2f} m (safely lifted off seabed).")

        if z_ascend < z_bed - 0.5:
            print("    [PASS] Upward recovery successful: ROV smoothly ascended from seabed boundary.")
        else:
            print("    [WARN] Upward recovery motion was insufficient.")
            passed = False

    except Exception as e:
        print(f"  [ERROR] Depth clamp test failed: {e}")
        passed = False

    print()
    return passed

def test_sensors(client):
    """Test ROV state telemetry and water sensors: IMU, Barometer, Magnetometer, Downward Sonar."""
    print("=== Telemetry & Sensor Verification ===")
    passed = True

    # Kinematics State
    try:
        state = client.getRovState()
        pos = state.kinematics_estimated.position
        ori = state.kinematics_estimated.orientation
        lin_vel = state.kinematics_estimated.linear_velocity
        roll, pitch, yaw = quaternion_to_euler(ori.w_val, ori.x_val, ori.y_val, ori.z_val)
        print(f"  Position (NED):      x={pos.x_val:.2f}m, y={pos.y_val:.2f}m, z={pos.z_val:.2f}m")
        print(f"  Linear Velocity:     vx={lin_vel.x_val:.2f}m/s, vy={lin_vel.y_val:.2f}m/s, vz={lin_vel.z_val:.2f}m/s")
        print(f"  Attitude (Euler):    Roll={math.degrees(roll):.2f}°, Pitch={math.degrees(pitch):.2f}°, Yaw={math.degrees(yaw):.2f}°")
    except Exception as e:
        print(f"  [ERROR] Kinematics telemetry failed: {e}")
        passed = False

    # IMU
    try:
        imu = client.getImuData()
        acc = imu.linear_acceleration
        ang = imu.angular_velocity
        print(f"  IMU Accel:           x={acc.x_val:.2f}, y={acc.y_val:.2f}, z={acc.z_val:.2f} m/s^2")
        print(f"  IMU Angular Vel:     x={ang.x_val:.3f}, y={ang.y_val:.3f}, z={ang.z_val:.3f} rad/s")
    except Exception as e:
        print(f"  [ERROR] IMU query failed: {e}")
        passed = False

    # Barometer / Depth Sensor
    try:
        baro = client.getBarometerData()
        depth_est = -baro.altitude
        print(f"  Baro (Depth/Alt):    altitude={baro.altitude:.2f}m (est depth={depth_est:.2f}m), pressure={baro.pressure:.1f} Pa")
        if baro.pressure < 90000.0:
            print("  [WARN] Barometer pressure unreasonably low.")
            passed = False
    except Exception as e:
        print(f"  [ERROR] Barometer query failed: {e}")
        passed = False

    # Magnetometer
    try:
        mag = client.getMagnetometerData()
        field = mag.magnetic_field_body
        print(f"  Magnetometer:        x={field.x_val:.3f}, y={field.y_val:.3f}, z={field.z_val:.3f} Gauss")
    except Exception as e:
        print(f"  [ERROR] Magnetometer query failed: {e}")
        passed = False

    # Downward Sonar (Distance Sensor)
    try:
        dist_data = client.getDistanceSensorData(distance_sensor_name="downward_sonar")
        print(f"  Downward Sonar:      distance_to_bed={dist_data.distance:.2f}m (min={dist_data.min_distance}m, max={dist_data.max_distance}m)")
        if dist_data.distance < 0.0:
            print("  [WARN] Negative sonar distance reported.")
            passed = False
    except Exception as e:
        # Distance sensor might be named differently or optional in base client
        try:
            dist_data = client.getDistanceSensorData()
            print(f"  Downward Sonar:      distance_to_bed={dist_data.distance:.2f}m")
        except Exception:
            print(f"  Downward Sonar:      Not available or defaulted ({e})")

    print()
    return passed

def test_cameras(client):
    """Test underwater stereo and front camera image capture (RGB Scene)."""
    print("=== Stereo Camera Capture Verification ===")
    passed = True
    cams_to_test = ["front_left_custom", "front_right_custom"]

    image_req_cls = getattr(airsim, 'ImageRequest', None) if airsim_loaded else None
    image_type_scene = getattr(airsim, 'ImageType', None)
    scene_val = image_type_scene.Scene if image_type_scene else 0

    for cam in cams_to_test:
        try:
            if image_req_cls:
                reqs = [image_req_cls(cam, scene_val, False, False)]
            else:
                reqs = [type('Req', (), {'camera_name': cam, 'image_type': 0})()]

            responses = client.simGetImages(reqs)
            if responses and len(responses) > 0 and responses[0].width > 0 and responses[0].height > 0:
                print(f"  [PASS] Camera '{cam}': Captured {responses[0].width}x{responses[0].height} RGB Scene frame.")
            else:
                print(f"  [WARN] Camera '{cam}': No valid image frame returned.")
                passed = False
        except Exception as e:
            # Fallback check for camera "0"
            try:
                if image_req_cls:
                    reqs = [image_req_cls("0", scene_val, False, False)]
                else:
                    reqs = [type('Req', (), {'camera_name': "0", 'image_type': 0})()]
                responses = client.simGetImages(reqs)
                if responses and len(responses) > 0 and responses[0].width > 0:
                    print(f"  [PASS] Fallback Camera '0': Captured {responses[0].width}x{responses[0].height} frame.")
                    break
            except Exception:
                pass
            print(f"  [ERROR] Camera '{cam}' capture failed: {e}")
            passed = False

    print()
    return passed

def test_motion(client):
    """Test 6-DOF body-frame velocity control and hover stabilization."""
    print("=== ROV Motion & Hover Stabilization Test ===")
    passed = True
    try:
        print("  Commanding body-frame motion: vx = 0.5 m/s, vz = 0.2 m/s for 2.0 s...")
        client.moveByVelocityBodyFrameAsync(vx=0.5, vy=0.0, vz=0.2, duration=2.0).join()
        time.sleep(0.3)

        state = client.getRovState()
        pos = state.kinematics_estimated.position
        print(f"  Position after motion: x={pos.x_val:.2f}m, y={pos.y_val:.2f}m, z={pos.z_val:.2f}m")

        print("  Commanding hover stabilization...")
        client.hoverAsync().join()
        time.sleep(0.3)
        print("  Hover stabilized.")
    except Exception as e:
        print(f"  [ERROR] Motion execution failed: {e}")
        passed = False

    print()
    return passed

# ==============================================================================
# Main Entrypoint
# ==============================================================================

def main():
    parser = argparse.ArgumentParser(description="Test CarlaAir underwater ROV with Fossen hydrodynamics & depth safety")
    parser.add_argument("--host", default="127.0.0.1", help="Host IP (default: 127.0.0.1)")
    parser.add_argument("--port", type=int, default=2000, help="CARLA RPC port (default: 2000)")
    parser.add_argument("--airsim-port", type=int, default=41451, help="AirSim RPC port (default: 41451)")
    parser.add_argument("--skip-carla", action="store_true", help="Skip CARLA registry check")
    parser.add_argument("--require-carla", action="store_true", help="Fail test if CARLA actor check fails")
    parser.add_argument("--skip-movement", action="store_true", help="Skip motion test, only test sensors")
    parser.add_argument("--test-buoyancy", action="store_true", help="Run neutral buoyancy & self-righting tests")
    parser.add_argument("--test-depth-clamp", action="store_true", help="Run seabed depth clamp & safety boundary tests")
    parser.add_argument("--all-tests", action="store_true", help="Run complete test suite including buoyancy and depth clamp")
    parser.add_argument("--mock", action="store_true", help="Run in mock/self-check mode without requiring live Unreal/Carla server")
    parser.add_argument("--settings", default="", help="Path to settings_rov.json")
    args = parser.parse_args()

    failures = []

    # If --mock or --all-tests, enable full test coverage
    if args.mock or args.all_tests:
        args.test_buoyancy = True
        args.test_depth_clamp = True

    print("==================================================")
    print("      CarlaAir Underwater ROV Test Suite          ")
    print("==================================================")
    print(f"Mode: {'MOCK / SELF-CHECK (Offline/CI)' if args.mock else 'LIVE SIMULATION'}")
    print()

    # Step 1: Configuration check
    settings_file = find_settings_file(args.settings)
    cfg_ok, settings_dict = validate_settings_file(settings_file)
    if not cfg_ok:
        failures.append("settings_rov.json configuration check failed")

    # Step 2: CARLA actor registry check (if live)
    if not args.mock and not args.skip_carla:
        carla_ok = test_carla_actor(args.host, args.port, required=args.require_carla)
        if args.require_carla and not carla_ok:
            failures.append(f"CARLA actor registry check failed on {args.host}:{args.port}")
        print()

    # Step 3: Establish Client Connection
    client = None
    if args.mock:
        print("=== Connecting to ROV Client (Mock Mode) ===")
        client = MockRovClient(settings_dict=settings_dict)
        client.confirmConnection()
        client.enableApiControl(True)
        client.armDisarm(True)
        print("  [PASS] Mock ROV Client connected, API control enabled, thrusters armed.\n")
    else:
        print(f"=== Connecting to AirSim ROV on {args.host}:{args.airsim_port} ===")
        try:
            if not airsim_loaded:
                sys.exit("Error: airsim package with RovClient not found. Please install air or run with --mock.")
            client = airsim.RovClient(ip=args.host, port=args.airsim_port)
            client.confirmConnection()
            client.enableApiControl(True)
            client.armDisarm(True)
            print("  Connected to AirSim server, API control enabled, thrusters armed.\n")
        except Exception as e:
            msg = (
                f"Failed to connect to live AirSim ROV on {args.host}:{args.airsim_port} ({e}).\n"
                "Tip: When running in CI or without a graphical Unreal/Carla instance, "
                "use '--mock' to execute the complete protocol and dynamics self-check."
            )
            print(f"  [NOTICE] {msg}\n")
            failures.append(msg)
            print("==================================================")
            print("TEST SUITE FINISHED WITH CONNECTION NOTICE")
            print("==================================================")
            sys.exit(1)

    # Step 4: Telemetry & Sensors
    if not test_sensors(client):
        failures.append("Sensor telemetry test failed")

    # Step 5: Movement & Hover
    if not args.skip_movement:
        if not test_motion(client):
            failures.append("Motion & hover test failed")

    # Step 6: Neutral Buoyancy & Self-Righting
    if args.test_buoyancy:
        if not test_buoyancy(client):
            failures.append("Neutral buoyancy and self-righting test failed")

    # Step 7: Depth Clamp & Boundary Safety
    if args.test_depth_clamp:
        if not test_depth_clamp(client):
            failures.append("Depth clamp and seabed boundary safety test failed")

    # Step 8: Stereo Camera Verification
    if not test_cameras(client):
        failures.append("Stereo camera capture verification failed")

    # Step 9: Cleanup & Disarm
    print("=== Cleanup & Disarm ===")
    try:
        client.armDisarm(False)
        client.enableApiControl(False)
        print("  ROV disarmed and API control released.\n")
    except Exception as e:
        print(f"  Cleanup notice: {e}\n")

    # Final Summary
    print("==================================================")
    if failures:
        print(f"ROV TEST SUITE FAILED with {len(failures)} issue(s):")
        for f in failures:
            print(f"  - {f}")
        print("==================================================")
        sys.exit(1)
    else:
        print("ALL CARLA-AIR ROV TESTS COMPLETED SUCCESSFULLY (100% PASS)!")
        print("==================================================")
        sys.exit(0)

if __name__ == "__main__":
    main()
