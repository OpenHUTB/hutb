import holoocean
import uuid

import numpy as np

turtle_config = {
    "name": "test_velocity_sensor",
    "world": "TestWorld",
    "main_agent": "turtle0",
    "frames_per_sec": False,
    "agents": [
        {
            "agent_name": "turtle0",
            "agent_type": "TurtleAgent",
            "sensors": [
                {
                    "sensor_type": "DVLSensor",
                },
                {
                    "sensor_type": "DVLSensor",
                    "sensor_name": "noise",
                    "configuration": {
                        "VelSigma": 5,
                        "RangeSigma": 5,
                    },
                },
            ],
            "control_scheme": 0,
            "location": [-1.5, -1.50, 3.0],
        }
    ],
}


def test_dvl_sensor_straight():
    """Make sure when we move forward x is positive, y is close to 0. Make sure when not moving both are close to 0"""

    binary_path = holoocean.packagemanager.get_binary_path_for_package("TestWorlds")
    turtle_config["agents"][0]["rotation"] = [0, 0, 0]

    with holoocean.environments.HoloOceanEnvironment(
        scenario=turtle_config,
        binary_path=binary_path,
        show_viewport=False,
        verbose=True,
        uuid=str(uuid.uuid4()),
    ) as env:
        # let it land and then start moving forward
        for _ in range(200):
            last_x_velocity, _, _ = env.tick()["DVLSensor"][:3]
        env.step([100, 0])

        # Move forward, making sure y is relatively small, and x is increasing
        for i in range(30):
            new_x_velocity, y_velocity, z_velocity = env.step([100, 0])["DVLSensor"][:3]
            assert (
                new_x_velocity >= last_x_velocity
            ), f"The velocity didn't increase at step {i}!"
            assert y_velocity <= 0.5
            assert z_velocity <= 0.5
            last_x_velocity = new_x_velocity

        # Let it stop
        for _ in range(100):
            env.step([0, 0])

        # Move backward, making sure y is relatively small, and x is decreasing
        for i in range(30):
            new_x_velocity, y_velocity, z_velocity = env.step([-100, 0])["DVLSensor"][
                :3
            ]
            assert (
                new_x_velocity <= last_x_velocity
            ), f"The velocity didn't decrease at step {i}!"
            assert y_velocity <= 0.5
            assert z_velocity <= 0.5
            last_x_velocity = new_x_velocity

        # Let it stop
        for _ in range(100):
            env.step([0, 0])

        # make sure everythign is close to 0
        x_velocity, y_velocity, z_velocity = env.tick()["DVLSensor"][:3]
        assert x_velocity <= 1e-2, "The x velocity wasn't close enough to zero!"
        assert y_velocity <= 1e-2, "The y velocity wasn't close enough to zero!"
        assert z_velocity <= 1e-2, "The z velocity wasn't close enough to zero!"


def test_dvl_sensor_rotated():
    """Make sure when we move forward x is positive, y is close to 0. Make sure when not moving both are close to 0"""

    binary_path = holoocean.packagemanager.get_binary_path_for_package("TestWorlds")
    turtle_config["agents"][0]["rotation"] = [0, 0, 90]

    with holoocean.environments.HoloOceanEnvironment(
        scenario=turtle_config,
        binary_path=binary_path,
        show_viewport=False,
        verbose=True,
        uuid=str(uuid.uuid4()),
    ) as env:
        # let it land and then start moving forward
        for _ in range(100):
            last_x_velocity, _, _ = env.tick()["DVLSensor"][:3]
        env.step([100, 0])

        # Move forward, making sure y is relatively small, and x is increasing
        for i in range(22):
            new_x_velocity, y_velocity, z_velocity = env.step([100, 0])["DVLSensor"][:3]
            assert (
                new_x_velocity >= last_x_velocity
            ), f"The velocity didn't increase at step {i}!"
            assert y_velocity <= 0.5
            assert z_velocity <= 0.5
            last_x_velocity = new_x_velocity

        # Let it stop
        for _ in range(100):
            env.step([0, 0])

        # Move backward, making sure y is relatively small, and x is decreasing
        for i in range(20):
            new_x_velocity, y_velocity, z_velocity = env.step([-100, 0])["DVLSensor"][
                :3
            ]
            assert (
                new_x_velocity <= last_x_velocity
            ), f"The velocity didn't decrease at step {i}!"
            assert y_velocity <= 0.5
            assert z_velocity <= 0.5
            last_x_velocity = new_x_velocity

        # Let it stop
        for _ in range(100):
            env.step([0, 0])

        # make sure everythign is close to 0
        x_velocity, y_velocity, z_velocity = env.tick()["DVLSensor"][:3]
        assert x_velocity <= 1e-2, "The x velocity wasn't close enough to zero!"
        assert y_velocity <= 1e-2, "The y velocity wasn't close enough to zero!"
        assert z_velocity <= 1e-2, "The z velocity wasn't close enough to zero!"


def test_dvl_noise():
    """Make sure turning on noise actually turns it on"""
    binary_path = holoocean.packagemanager.get_binary_path_for_package("TestWorlds")
    turtle_config["agents"][0]["rotation"] = [0, 0, 0]

    with holoocean.environments.HoloOceanEnvironment(
        scenario=turtle_config,
        binary_path=binary_path,
        show_viewport=False,
        verbose=True,
        uuid=str(uuid.uuid4()),
    ) as env:
        # let it land and then start moving forward
        for _ in range(10):
            state = env.tick()
            assert not np.allclose(state["DVLSensor"], state["noise"])
