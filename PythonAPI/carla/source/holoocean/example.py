"""This file contains multiple examples of how you might use HoloOcean."""
import numpy as np

import holoocean
from holoocean import agents
from holoocean.environments import *
from holoocean import sensors

def hovering_example():
    """A basic example of how to use the HoveringAUV agent."""
    env = holoocean.make("SimpleUnderwater-Hovering")

    # This command tells the AUV go forward with a power of "10"
    # The last four elements correspond to the horizontal thrusters (see docs for more info)
    command = np.array([0, 0, 0, 0, 10, 10, 10, 10])
    for _ in range(1000):
        state = env.step(command)
        # To access specific sensor data:
        if "PoseSensor" in state:
            pose = state["PoseSensor"]
        # Some sensors don't tick every timestep, so we check if it's received.
        if "DVLSensor" in state:
            dvl = state["DVLSensor"]

    # This command tells the AUV to go down with a power of "10"
    # The first four elements correspond to the vertical thrusters
    command = np.array([-10, -10, -10, -10, 0, 0, 0, 0])
    for _ in range(1000):
        # We alternatively use the act function
        env.act("auv0", command)
        state = env.tick()

    # You can control the AgentFollower camera (what you see) by pressing v to toggle spectator
    # mode. This detaches the camera and allows you to move freely about the world.
    # Press h to view the agents x-y-z location
    # You can also press c to snap to the location of the camera to see the world from the perspective of the
    # agent. See the Controls section of the ReadMe for more details.


def torpedo_example():
    """A basic example of how to use the TorpedoAUV agent."""
    env = holoocean.make("SimpleUnderwater-Torpedo")

    # This command tells the AUV go forward with a power of "50"
    # The last four elements correspond to 
    command = np.array([0, 0, 0, 0, 50])
    for _ in range(1000):
        state = env.step(command)

    # Now turn the top and bottom fins to turn left
    command = np.array([0, -45, 0, 45, 50])
    for _ in range(1000):
        state = env.step(command)


def editor_example():
    """This editor example shows how to interact with holodeck worlds while they are being built
    in the Unreal Engine Editor. Most people that use holodeck will not need this.

    This example uses a custom scenario, see 
    https://holoocean.readthedocs.io/en/latest/usage/examples/custom-scenarios.html

    Note: When launching Holodeck from the editor, press the down arrow next to "Play" and select
    "Standalone Game", otherwise the editor will lock up when the client stops ticking it.
    """

    config = {
        "name": "test",
        "world": "ExampleLevel",
        "main_agent": "auv0",
        "agents": [
            {
                "agent_name": "auv0",
                "agent_type": "HoveringAUV",
                "sensors": [
                    {
                        "sensor_type": "LocationSensor",
                    },
                    {
                        "sensor_type": "VelocitySensor"
                    },
                    {
                        "sensor_type": "RGBCamera"
                    }
                ],
                "control_scheme": 1,
                "location": [0, 0, 1]
            }
        ]
    }

    env = HoloOceanEnvironment(scenario=config, start_world=False)
    command = [0, 0, 10, 50]

    for i in range(10):
        env.reset()
        for _ in range(1000):
            state = env.step(command)


def editor_multi_agent_example():
    """This editor example shows how to interact with holodeck worlds that have multiple agents.
    This is specifically for when working with UE4 directly and not a prebuilt binary.

    Note: When launching Holodeck from the editor, press the down arrow next to "Play" and select
    "Standalone Game", otherwise the editor will lock up when the client stops ticking it.
    """
    config = {
        "name": "test_handagent",
        "world": "ExampleLevel",
        "main_agent": "auv0",
        "agents": [
            {
                "agent_name": "auv0",
                "agent_type": "HoveringAUV",
                "sensors": [
                ],
                "control_scheme": 1,
                "location": [0, 0, 1]
            },
            {
                "agent_name": "auv1",
                "agent_type": "TorpedoAUV",
                "sensors": [
                ],
                "control_scheme": 1,
                "location": [0, 0, 5]
            }
        ]
    }

    env = HoloOceanEnvironment(scenario=config, start_world=False)

    cmd0 = np.array([0, 0, -2, 10])
    cmd1 = np.array([0, 0, 5, 10])

    for i in range(10):
        env.reset()
        env.act("uav0", cmd0)
        env.act("uav1", cmd1)
        for _ in range(1000):
            states = env.tick()

