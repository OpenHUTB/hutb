import setup_path
import airsim
import time
import pprint

def main():
    # Connect to the AirSim simulator (ROV)
    client = airsim.RovClient()
    print("Connecting to AirSim ROV...")
    client.confirmConnection()
    client.enableApiControl(True)

    print("Arming the ROV...")
    client.armDisarm(True)

    # Query vehicle state
    state = client.getRovState()
    print("ROV State:\n%s" % pprint.pformat(state))

    # Query sensor states
    try:
        imu_data = client.getImuData()
        print("IMU Data:\n%s" % pprint.pformat(imu_data))
    except Exception as e:
        print("IMU query failed: %s" % e)

    try:
        baro_data = client.getBarometerData()
        print("Barometer (Depth) Data:\n%s" % pprint.pformat(baro_data))
    except Exception as e:
        print("Barometer query failed: %s" % e)

    try:
        mag_data = client.getMagnetometerData()
        print("Magnetometer Data:\n%s" % pprint.pformat(mag_data))
    except Exception as e:
        print("Magnetometer query failed: %s" % e)

    # Basic movement test: dive 2 meters and move forward
    print("Moving ROV by velocity in body frame (forward 0.5 m/s, dive 0.2 m/s for 3s)...")
    client.moveByVelocityBodyFrameAsync(vx=0.5, vy=0.0, vz=0.2, duration=3).join()

    time.sleep(1)

    state = client.getRovState()
    print("Updated ROV Position: %s" % state.kinematics_estimated.position)

    # Hover in place
    print("Hovering...")
    client.hoverAsync().join()

    # Disarm and release control
    print("Disarming and resetting...")
    client.armDisarm(False)
    client.enableApiControl(False)
    print("Done!")

if __name__ == "__main__":
    main()
