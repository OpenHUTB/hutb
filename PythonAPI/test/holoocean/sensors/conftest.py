import pytest

import uuid
import holoocean


def pytest_generate_tests(metafunc):
    """Iterate over every scenario"""
    if "resolution" in metafunc.fixturenames:
        metafunc.parametrize("resolution", [256, 500, 1000])
    elif "1024_env" in metafunc.fixturenames:
        metafunc.parametrize("env_1024", [1024], indirect=True)
    elif "ticks_per_capture" in metafunc.fixturenames:
        metafunc.parametrize("ticks_per_capture", [30, 15, 10, 5, 2])
    elif "abuse_world" in metafunc.fixturenames:
        metafunc.parametrize("abuse_world", ["abuse_world"], indirect=True)
    elif "rotation_env" in metafunc.fixturenames:
        metafunc.parametrize("rotation_env", ["rotation_env"], indirect=True)
    elif "agent_abuse_world" in metafunc.fixturenames:
        metafunc.parametrize(
            "agent_abuse_world", ["turtle0", "uav0"], indirect=True
        )


shared_1024_env = None


@pytest.fixture
def env_1024(request):
    """Shares the 1024x1024 configuration for use in two tests"""
    cfg = {
        "name": "test_viewport_capture",
        "world": "TestWorld",
        "main_agent": "sphere0",
        "frames_per_sec": False,
        "agents": [
            {
                "agent_name": "sphere0",
                "agent_type": "SphereAgent",
                "sensors": [
                    {
                        "sensor_type": "ViewportCapture",
                        "configuration": {"CaptureWidth": 1024, "CaptureHeight": 1024},
                    }
                ],
                "control_scheme": 0,
                "location": [0.95, -1.75, 0.5],
            }
        ],
        "window_width": 1024,
        "window_height": 1024,
    }

    global shared_1024_env

    if shared_1024_env is None:
        binary_path = holoocean.packagemanager.get_binary_path_for_package("TestWorlds")
        shared_1024_env = holoocean.environments.HoloOceanEnvironment(
            scenario=cfg,
            binary_path=binary_path,
            show_viewport=False,
            verbose=True,
            uuid=str(uuid.uuid4()),
        )

    shared_1024_env.reset()

    return shared_1024_env


shared_rotation_env = None
shared_abuse_env = None


def get_abuse_world():
    global shared_abuse_env
    binary_path = holoocean.packagemanager.get_binary_path_for_package("TestWorlds")
    if shared_abuse_env is None:
        shared_abuse_env = holoocean.environments.HoloOceanEnvironment(
            scenario=abuse_config,
            binary_path=binary_path,
            show_viewport=False,
            verbose=True,
            uuid=str(uuid.uuid4()),
        )
    shared_abuse_env.reset()
    return shared_abuse_env


@pytest.fixture
def abuse_world(request):
    return get_abuse_world()


@pytest.fixture
def agent_abuse_world(request):
    env = get_abuse_world()
    return request.param, env


@pytest.fixture
def rotation_env(request):
    """Shares the RotationSensor configuration"""
    cfg = {
        "name": "test_rotation_sensor",
        "world": "TestWorld",
        "main_agent": "sphere0",
        "frames_per_sec": False,
        "agents": [
            {
                "agent_name": "sphere0",
                "agent_type": "SphereAgent",
                "sensors": [{"sensor_type": "RGBCamera", "rotation": [0, -90, 0]}],
                "control_scheme": 0,
                "location": [0.95, -1.75, 0.5],
            }
        ],
    }

    global shared_rotation_env

    if shared_rotation_env is None:
        binary_path = holoocean.packagemanager.get_binary_path_for_package("TestWorlds")
        shared_rotation_env = holoocean.environments.HoloOceanEnvironment(
            scenario=cfg,
            binary_path=binary_path,
            show_viewport=False,
            verbose=True,
            uuid=str(uuid.uuid4()),
        )

    shared_rotation_env.reset()
    return shared_rotation_env


abuse_config = {
    "name": "test_abuse_sensor",
    "world": "TestWorld",
    "main_agent": "uav0",
    "frames_per_sec": False,
    "agents": [
        {
            "agent_name": "uav0",
            "agent_type": "UavAgent",
            "sensors": [
                {
                    "sensor_type": "AbuseSensor",
                }
            ],
            "control_scheme": 0,
            "location": [1.5, 0, 9],
            "rotation": [0, 0, 0],
        },
        {
            "agent_name": "turtle0",
            "agent_type": "TurtleAgent",
            "sensors": [
                {
                    "sensor_type": "AbuseSensor",
                }
            ],
            "control_scheme": 0,
            "location": [2, 1.5, 8],
            "rotation": [0, 0, 0],
        },
    ],
}
