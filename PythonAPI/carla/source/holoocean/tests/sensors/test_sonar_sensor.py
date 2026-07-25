import holoocean
import uuid
import os
import pytest
import numpy as np


@pytest.fixture
def config():
    c = {
        "name": "test_location_sensor",
        "world": "TestWorld",
        "main_agent": "auv0",
        "frames_per_sec": False,
        "agents": [
            {
                "agent_name": "auv0",
                "agent_type": "HoveringAUV",
                "sensors": [
                    {
                        "sensor_type": "ImagingSonar",
                        "configuration": {"MinRange": 0.1, "MaxRange": 1},
                    }
                ],
                "control_scheme": 0,
                "location": [0.95, -1.75, 0.5],
            }
        ],
    }
    return c


@pytest.mark.parametrize("size", [(0.02, 5.12), (0.1, 12.8), (0.05, 25.6)])
def test_folder_creation(size, config):
    """Make sure folders are made with the correct size"""

    config["octree_min"] = size[0]
    config["octree_max"] = size[1]

    binary_path = holoocean.packagemanager.get_binary_path_for_package("TestWorlds")

    with holoocean.environments.HoloOceanEnvironment(
        scenario=config,
        binary_path=binary_path,
        show_viewport=False,
        verbose=True,
        uuid=str(uuid.uuid4()),
    ) as env:
        for _ in range(10):
            env.tick()

    dir = os.path.join(
        holoocean.util.get_holoocean_path(),
        "worlds/TestWorlds/LinuxNoEditor/Holodeck/Octrees",
    )
    dir = os.path.join(
        dir, f"{config['world']}/min{int(size[0]*100)}_max{int(size[1]*100)}"
    )

    assert os.path.isdir(dir), "Sonar folder wasn't created"


def test_blank(config):
    """Test to make sure when we're in the middle of nowhere, we get nothing back"""

    config["agents"][0]["location"] = [-100, -100, -100]

    binary_path = holoocean.packagemanager.get_binary_path_for_package("TestWorlds")

    with holoocean.environments.HoloOceanEnvironment(
        scenario=config,
        binary_path=binary_path,
        show_viewport=False,
        verbose=True,
        uuid=str(uuid.uuid4()),
    ) as env:
        for _ in range(10):
            state = env.tick()["ImagingSonar"]
            assert np.allclose(np.zeros_like(state), state)
