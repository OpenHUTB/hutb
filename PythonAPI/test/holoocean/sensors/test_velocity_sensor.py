import holoocean
import uuid

uav_config = {
    "name": "test_velocity_sensor",
    "world": "TestWorld",
    "main_agent": "uav0",
    "frames_per_sec": False,
    "agents": [
        {
            "agent_name": "uav0",
            "agent_type": "UavAgent",
            "sensors": [
                {
                    "sensor_type": "VelocitySensor",
                }
            ],
            "control_scheme": 0,
            "location": [0, 0, 20],
        }
    ],
}


def test_velocity_sensor_uav_z_axis():
    """Drop the UAV, make sure the z velocity is increasingly negative as it falls.
    Make sure it zeros out after it hits the ground, and then goes positive on takeoff
    """

    binary_path = holoocean.packagemanager.get_binary_path_for_package("TestWorlds")

    with holoocean.environments.HoloOceanEnvironment(
        scenario=uav_config,
        binary_path=binary_path,
        show_viewport=False,
        verbose=True,
        uuid=str(uuid.uuid4()),
    ) as env:
        last_z_velocity = env.tick()["VelocitySensor"][2]

        for _ in range(50):
            new_z_velocity = env.tick()["VelocitySensor"][2]
            assert new_z_velocity <= last_z_velocity, "The velocity didn't decrease!"
            last_z_velocity = new_z_velocity

        # Make sure it hits the ground
        for _ in range(60):
            env.tick()

        last_z_velocity = env.tick()["VelocitySensor"][2]

        assert last_z_velocity <= 1e-4, "The velocity wasn't close enough to zero!"

        # Send it flying up into the air to make sure the z velocity increases
        env.step([0, 0, 0, 100])

        # z velocity should be positive now
        for _ in range(20):
            new_z_velocity = env.tick()["VelocitySensor"][2]
            assert new_z_velocity >= last_z_velocity, "The velocity didn't increase!"
            last_z_velocity = new_z_velocity


# TODO: Test other axises
