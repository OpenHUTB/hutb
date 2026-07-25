import holoocean
import uuid
import numpy as np
from tests.utils.equality import almost_equal

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
                    "sensor_type": "PoseSensor",
                },
                {
                    "sensor_type": "OrientationSensor",
                },
                {
                    "sensor_type": "LocationSensor",
                },
            ],
            "control_scheme": 0,
            "location": [-1.5, -1.50, 3.0],
        }
    ],
}


def test_pose_sensor_straight():
    """Make sure pose sensor returns the same values as the orientation and location sensors"""

    binary_path = holoocean.packagemanager.get_binary_path_for_package("TestWorlds")

    with holoocean.environments.HoloOceanEnvironment(
        scenario=turtle_config,
        binary_path=binary_path,
        show_viewport=False,
        verbose=True,
        uuid=str(uuid.uuid4()),
    ) as env:
        # let it land and then start moving forward
        for _ in range(200):
            command = np.random.random(size=2)
            state = env.step(command)
            assert almost_equal(
                state["PoseSensor"][:3, :3], state["OrientationSensor"]
            ), f"Rotation in PoseSensor doesn't match that in OrientationSensor at timestep {i}"
            assert almost_equal(
                state["PoseSensor"][:3, 3], state["LocationSensor"]
            ), f"Location in PoseSensor doesn't match that in LocationSensor at timestep {i}"
