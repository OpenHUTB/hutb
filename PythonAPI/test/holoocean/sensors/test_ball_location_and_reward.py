import holoocean
import uuid
import pytest

cfg = {
    "name": "test_ball_location_and_reward",
    "world": "CupGame",
    "main_agent": "sphere0",
    "frames_per_sec": False,
    "agents": [
        {
            "agent_name": "sphere0",
            "agent_type": "SphereAgent",
            "sensors": [
                {
                    "sensor_type": "CupGameTask",
                    "configuration": {"Speed": 3, "NumShuffles": 3, "Seed": 0},
                },
                {"sensor_type": "BallLocationSensor"},
            ],
            "control_scheme": 0,
            "location": [-0.4, -0.9, 1.8],
            "rotation": [90, 0, 0],
        }
    ],
    "window_width": 1024,
    "window_height": 1024,
}


@pytest.mark.skipif(
    "Dexterity" not in holoocean.installed_packages(),
    reason="Dexterity package not installed",
)
def test_ball_location_and_reward():
    """Shuffle the ball using a seed. Ensure that after shuffling the ball location sensor
    detects the correct position and move the sphere agent forward to collide with the correct cup.
    Make sure it receives a reward of 1.
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

        env.tick(300)
        for _ in range(30):
            state = env.step([0])
            reward, terminal = env.get_reward_terminal()
            if reward == 1:
                touched_cup = True
        assert touched_cup and state["BallLocationSensor"] == 2
