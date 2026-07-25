import holoocean
import uuid
from copy import deepcopy

from tests.utils.equality import almost_equal

sphere_config = {
    "name": "test_location_sensor",
    "world": "TestWorld",
    "main_agent": "sphere0",
    "frames_per_sec": False,
    "agents": [
        {
            "agent_name": "sphere0",
            "agent_type": "SphereAgent",
            "sensors": [
                {
                    "sensor_type": "LocationSensor",
                }
            ],
            "control_scheme": 0,
            "location": [0.95, -1.75, 0.5],
        }
    ],
}


def test_location_sensor_after_teleport():
    """Make sure the location sensor updates after a teleport. Also verifies that the coordinates for the teleport
    command match the coordinates used by the location sensor
    """
    binary_path = holoocean.packagemanager.get_binary_path_for_package("TestWorlds")

    with holoocean.environments.HoloOceanEnvironment(
        scenario=sphere_config,
        binary_path=binary_path,
        show_viewport=False,
        verbose=True,
        uuid=str(uuid.uuid4()),
    ) as env:
        for _ in range(10):
            env.tick()

        loc = [507, 301, 1620]
        env.agents["sphere0"].teleport(loc, [0, 0, 0])

        state = env.tick()
        sensed_loc = state["LocationSensor"]

        assert almost_equal(
            loc, sensed_loc
        ), "Sensed location did not match the expected location!"


uav_config = {
    "name": "test_location_sensor",
    "world": "TestWorld",
    "main_agent": "uav0",
    "frames_per_sec": False,
    "agents": [
        {
            "agent_name": "uav0",
            "agent_type": "UavAgent",
            "sensors": [
                {
                    "sensor_type": "LocationSensor",
                }
            ],
            "control_scheme": 0,
            "location": [0.95, -1.75, 0.5],
        }
    ],
}


def test_location_sensor_falling():
    """Makes sure that the location sensor updates as the UAV falls, and after it comes to a rest"""
    cfg = deepcopy(uav_config)

    # Spawn the UAV 10 meters up
    cfg["agents"][0]["location"] = [0, 0, 20]

    binary_path = holoocean.packagemanager.get_binary_path_for_package("TestWorlds")

    with holoocean.environments.HoloOceanEnvironment(
        scenario=cfg,
        binary_path=binary_path,
        show_viewport=False,
        verbose=True,
        uuid=str(uuid.uuid4()),
    ) as env:
        last_location = env.tick()["LocationSensor"]

        for _ in range(85):
            new_location = env.tick()["LocationSensor"]
            assert (
                new_location[2] < last_location[2]
            ), "UAV's location sensor did not detect falling!"
            last_location = new_location

        # Give the UAV time to bounce and settle
        for _ in range(80):
            env.tick()

        # Make sure it is stationary now
        last_location = env.tick()["LocationSensor"]
        new_location = env.tick()["LocationSensor"]

        assert almost_equal(
            last_location, new_location
        ), "The UAV did not seem to settle!"


######################################################
# LocationSensor noise tests are found in test_mvn.py
######################################################
