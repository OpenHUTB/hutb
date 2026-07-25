import holoocean
import uuid
import pytest

cfg = {
    "name": "test_clean_up_reward",
    "world": "CleanUp",
    "main_agent": "sphere0",
    "frames_per_sec": False,
    "agents": [
        {
            "agent_name": "sphere0",
            "agent_type": "SphereAgent",
            "sensors": [
                {
                    "sensor_type": "LocationSensor",
                },
                {
                    "sensor_type": "CleanUpTask",
                    "configuration": {
                        "NumTrash": 5,
                        "UseTable": True,
                    },
                },
            ],
            "control_scheme": 0,
            "location": [1, 1, 1],
        }
    ],
}


@pytest.mark.skipif(
    "Dexterity" not in holoocean.installed_packages(),
    reason="Dexterity package not installed",
)
def test_ball_location_and_reward():
    """This is currently a stub test. There is no way to reliably test the trash world so this is just meant to a manual
    test where the tester makes sure that trash is spawning on a table. For now the reward and terminal should be zero.
    Eventually there should be a way to add a debug option in the config so it can be tested programmatically.
    """

    binary_path = holoocean.packagemanager.get_binary_path_for_package("Dexterity")

    with holoocean.environments.HoloOceanEnvironment(
        scenario=cfg,
        binary_path=binary_path,
        show_viewport=False,
        verbose=True,
        uuid=str(uuid.uuid4()),
    ) as env:
        env.reset()

        # env.agents["sphere0"].sensors["CleanUpTask"].start_task(4, False)
        env.tick(100)
        assert True
