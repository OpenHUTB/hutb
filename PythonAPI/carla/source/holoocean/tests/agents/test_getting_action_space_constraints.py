import uuid
from holoocean import packagemanager as pm
from holoocean.environments import HoloOceanEnvironment
import math

test_data = [
    # agent_name, control_scheme, exp_min, exp_max
    {
        "type": "UavAgent",
        "name": "UavAgent",
        "control_scheme": 0,
        "min": [-5.087, -6.5080, -0.8, -59.844],
        "max": [5.087, 6.5080, 0.8, 59.844],
    },
    {
        "type": "SphereAgent",
        "name": "SphereAgent0",
        "control_scheme": 0,
        "min": 0,
        "max": 4,
    },
    {
        "type": "SphereAgent",
        "name": "SphereAgent1",
        "control_scheme": 1,
        "min": [-20, -20],
        "max": [20, 20],
    },
    {
        "type": "TurtleAgent",
        "name": "TurtleAgent",
        "control_scheme": 0,
        "min": [-160.0, -35.0],
        "max": [160.0, 35.0],
    },
]

agents = [
    {
        "agent_name": x["name"],
        "agent_type": x["type"],
        "sensors": [],
        "control_scheme": x["control_scheme"],
        "location": [0, 0, 5],
    }
    for x in test_data
]

config = {
    "name": "test_collision_sensor",
    "world": "TestWorld",
    "main_agent": "UavAgent",
    "frames_per_sec": False,
    "agents": agents,
}


def is_close(a, b):
    for x, y in zip(a, b):
        if not math.isclose(x, y, rel_tol=1e-05):
            return False

    return True


def check_constraints(x, y):
    if type(x) == list:
        assert is_close(x, y)
    else:
        assert math.isclose(x, y, rel_tol=1e-05)


def test_min_max_action_space_constraints():
    binary_path = pm.get_binary_path_for_package("TestWorlds")

    with HoloOceanEnvironment(
        scenario=config,
        binary_path=binary_path,
        show_viewport=False,
        verbose=True,
        uuid=str(uuid.uuid4()),
    ) as env:
        for x in test_data:
            agent = x["name"]
            control_scheme = x["control_scheme"]

            min_result = env.agents[agent].control_schemes[control_scheme][1].get_low()
            exp_min = x["min"]

            check_constraints(min_result, exp_min)

            max_result = env.agents[agent].control_schemes[control_scheme][1].get_high()
            exp_max = x["max"]

            check_constraints(max_result, exp_max)
