import holoocean
import uuid

uav_config = {
    "name": "test_collision_sensor",
    "world": "TestWorld",
    "main_agent": "uav0",
    "frames_per_sec": False,
    "agents": [
        {
            "agent_name": "uav0",
            "agent_type": "UavAgent",
            "sensors": [
                {
                    "sensor_type": "CollisionSensor",
                }
            ],
            "control_scheme": 0,
            "location": [0, 0, 3],
        }
    ],
}


def test_collision_sensor_uav_falling():
    """Tests the collision sensor as the UAV falls, makes sure it fires when it hits the ground,
    and it turns off when the UAV goes flying into the air
    """

    binary_path = holoocean.packagemanager.get_binary_path_for_package("TestWorlds")

    with holoocean.environments.HoloOceanEnvironment(
        scenario=uav_config,
        binary_path=binary_path,
        show_viewport=False,
        verbose=True,
        uuid=str(uuid.uuid4()),
    ) as env:
        collided = env.tick()["CollisionSensor"][0]

        assert (
            not collided
        ), "The UAV is in the air but reported it collided with something!"

        for _ in range(80):
            if env.tick()["CollisionSensor"][0]:
                collided = True

        assert collided, "The UAV never reported it hit the ground!"

        # Make sure it stays colliding after it hits the ground

        for _ in range(30):
            assert env.tick()["CollisionSensor"][0], "The UAV stopped colliding!"

        # Make sure it resets to not collided after shooting it up in the air
        env.step([0, 0, 0, 100])

        for _ in range(50):
            env.tick()

        assert not env.tick()["CollisionSensor"][0], "The UAV is still colliding?"
