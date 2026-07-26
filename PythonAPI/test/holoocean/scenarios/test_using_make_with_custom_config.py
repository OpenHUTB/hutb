import holoocean
import uuid


def test_using_make_with_custom_config():
    """
    Validate that we can use holoocean.make with a custom configuration instead
    of loading it from a config file
    """

    sphere_config = {
        "name": "test_custom_config",
        "world": "TestWorld",
        "main_agent": "sphere0",
        "frames_per_sec": False,
        "agents": [
            {
                "agent_name": "sphere0",
                "agent_type": "SphereAgent",
                "sensors": [],
                "control_scheme": 0,
                "location": [0.95, -1.75, 0.5],
            }
        ],
    }

    binary_path = holoocean.packagemanager.get_binary_path_for_package("TestWorlds")

    with holoocean.environments.HoloOceanEnvironment(
        scenario=sphere_config,
        binary_path=binary_path,
        show_viewport=False,
        verbose=True,
        uuid=str(uuid.uuid4()),
    ) as env:
        for _ in range(0, 10):
            env.tick()
        assert True
