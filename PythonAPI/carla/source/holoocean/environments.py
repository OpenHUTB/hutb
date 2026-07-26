"""Module containing the environment interface for HoloOcean.
An environment contains all elements required to communicate with a world binary or HoloOceanCore
editor.

It specifies an environment, which contains a number of agents, and the interface for communicating
with the agents.
"""
import atexit
import os
import random
import subprocess
import sys

import numpy as np

from holoocean.command import (
    CommandCenter,
    SpawnAgentCommand,
    TeleportCameraCommand,
    RenderViewportCommand,
    RenderQualityCommand,
    CustomCommand,
    DebugDrawCommand,
)

from holoocean.exceptions import HoloOceanException
from holoocean.holooceanclient import HoloOceanClient
from holoocean.agents import AgentDefinition, SensorDefinition, AgentFactory
from holoocean.weather import WeatherController

from holoocean.sensors import AcousticBeaconSensor
from holoocean.sensors import OpticalModemSensor


class HoloOceanEnvironment:
    """Proxy for communicating with a HoloOcean world

    Instantiate this object using :meth:`holoocean.holoocean.make`.

    Args:
        agent_definitions (:obj:`list` of :class:`AgentDefinition`):
            Which agents are already in the environment

        binary_path (:obj:`str`, optional):
            The path to the binary to load the world from. Defaults to None.

        window_size ((:obj:`int`,:obj:`int`)):
            height, width of the window to open

        start_world (:obj:`bool`, optional):
            Whether to load a binary or not. Defaults to True.

        uuid (:obj:`str`):
            A unique identifier, used when running multiple instances of holoocean. Defaults to "".

        gl_version (:obj:`int`, optional):
            The version of OpenGL to use for Linux. Defaults to 4.

        verbose (:obj:`bool`):
            If engine log output should be printed to stdout

        pre_start_steps (:obj:`int`):
            Number of ticks to call after initializing the world, allows the level to load and settle.

        show_viewport (:obj:`bool`, optional):
            If the viewport should be shown (Linux only) Defaults to True.

        ticks_per_sec (:obj:`int`, optional):
            The number of frame ticks per unreal seconds. This will override whatever is
            in the configuration json. Defaults to 30.

        frames_per_sec (:obj:`int` or :obj:`bool`, optional):
            The max number of frames ticks per real seconds. This will override whatever is
            in the configuration json. If True, will match ticks_per_sec. If False, will not be
            turned on. If an integer, will set to that value. Defaults to true.

        copy_state (:obj:`bool`, optional):
            If the state should be copied or returned as a reference. Defaults to True.

        scenario (:obj:`dict`):
            The scenario that is to be loaded. See :ref:`scenario-files` for the schema.

    """

    def __init__(
        self,
        agent_definitions=None,
        binary_path=None,
        window_size=None,
        start_world=True,
        uuid="",
        gl_version=4,
        verbose=False,
        pre_start_steps=2,
        show_viewport=True,
        ticks_per_sec=None,
        frames_per_sec=None,
        copy_state=True,
        scenario=None,
    ):
        if agent_definitions is None:
            agent_definitions = []

        # Initialize variables

        if window_size is None:
            # Check if it has been configured in the scenario
            if scenario is not None and "window_height" in scenario:
                self._window_size = scenario["window_height"], scenario["window_width"]
            else:
                # Default resolution
                self._window_size = 720, 1280
        else:
            self._window_size = window_size

        # Use env size from scenario/world config
        if scenario is not None and "env_min" in scenario:
            self._env_min = scenario["env_min"]
            self._env_max = scenario["env_max"]
        # Default resolution
        else:
            self._env_min = [-10, -10, -10]
            self._env_max = [10, 10, 10]

        if scenario is not None and "octree_min" in scenario:
            self._octree_min = scenario["octree_min"]
            self._octree_max = scenario["octree_max"]
        else:
            # Default resolution
            self._octree_min = 0.02
            self._octree_max = 5

        if scenario is not None and "lcm_provider" not in scenario:
            scenario["lcm_provider"] = ""

        self._uuid = uuid
        self._pre_start_steps = pre_start_steps
        self._copy_state = copy_state
        self._scenario = scenario
        self._initial_agent_defs = agent_definitions
        self._spawned_agent_defs = []

        # Choose one that was passed in function
        if ticks_per_sec is not None:
            self._ticks_per_sec = ticks_per_sec
        # otherwise use one in scenario
        elif "ticks_per_sec" in scenario:
            self._ticks_per_sec = scenario["ticks_per_sec"]
        # default to 30
        else:
            self._ticks_per_sec = 30

        # If one wasn't passed in, use one in scenario
        if frames_per_sec is None and "frames_per_sec" in scenario:
            frames_per_sec = scenario["frames_per_sec"]
        # default to true
        else:
            frames_per_sec = True

        # parse frames_per_sec
        if frames_per_sec is True:
            self._frames_per_sec = self._ticks_per_sec
        elif frames_per_sec is False:
            self._frames_per_sec = 0
        else:
            self._frames_per_sec = frames_per_sec

        self._lcm = None
        self._num_ticks = 0

        # Start world based on OS
        if start_world:
            world_key = self._scenario["world"]
            if os.name == "posix":
                self.__linux_start_process__(
                    binary_path,
                    world_key,
                    gl_version,
                    verbose=verbose,
                    show_viewport=show_viewport,
                )
            elif os.name == "nt":
                self.__windows_start_process__(
                    binary_path, world_key, verbose=verbose, show_viewport=show_viewport
                )
            else:
                raise HoloOceanException("Unknown platform: " + os.name)

        # Initialize Client
        self._client = HoloOceanClient(self._uuid)
        self._command_center = CommandCenter(self._client)
        self._client.command_center = self._command_center
        self._reset_ptr = self._client.malloc("RESET", [1], np.bool_)
        self._reset_ptr[0] = False

        # Initialize environment controller
        self.weather = WeatherController(self.send_world_command)

        # Set up agents already in the world
        self.agents = dict()
        self._state_dict = dict()
        self._agent = None

        # Set the default state function
        self.num_agents = len(self.agents)

        # Whether we need to wait for a sonar to load
        self.start_world = start_world
        self._has_sonar = False

        if self.num_agents == 1:
            self._default_state_fn = self._get_single_state
        else:
            self._default_state_fn = self._get_full_state

        self._client.acquire(self._timeout)

        if os.name == "posix" and show_viewport is False:
            self.should_render_viewport(False)

        # Flag indicates if the user has called .reset() before .tick() and .step()
        self._initial_reset = False
        self.reset()

    @property
    def _timeout(self):
        # Returns the timeout that should be processed
        # Turns off timeout when creating octrees might be made
        if self._has_sonar or not self.start_world:
            if os.name == "posix":
                return None
            elif os.name == "nt":
                import win32event

                return win32event.INFINITE

        else:
            return 60

    @property
    def action_space(self):
        """Gives the action space for the main agent.

        Returns:
            :class:`~holoocean.spaces.ActionSpace`: The action space for the main agent.
        """
        return self._agent.action_space

    def info(self):
        """Returns a string with specific information about the environment.
        This information includes which agents are in the environment and which sensors they have.

        Returns:
            :obj:`str`: Information in a string format.
        """
        result = list()
        result.append("Agents:\n")
        for agent_name in self.agents:
            agent = self.agents[agent_name]
            result.append("\tName: ")
            result.append(agent.name)
            result.append("\n\tType: ")
            result.append(type(agent).__name__)
            result.append("\n\t")
            result.append("Sensors:\n")
            for _, sensor in agent.sensors.items():
                result.append("\t\t")
                result.append(sensor.name)
                result.append("\n")
        return "".join(result)

    def _load_scenario(self):
        """Loads the scenario defined in self._scenario_key.

        Instantiates agents, sensors, and weather.

        If no scenario is defined, does nothing.
        """
        if self._scenario is None:
            return

        for agent in self._scenario["agents"]:
            sensors = []
            for sensor in agent["sensors"]:
                if "sensor_type" not in sensor:
                    raise HoloOceanException(
                        "Sensor for agent {} is missing required key "
                        "'sensor_type'".format(agent["agent_name"])
                    )

                # Default values for a sensor
                sensor_config = {
                    "location": [0, 0, 0],
                    "rotation": [0, 0, 0],
                    "socket": "",
                    "configuration": None,
                    "sensor_name": sensor["sensor_type"],
                    "existing": False,
                    "Hz": self._ticks_per_sec,
                    "lcm_channel": None,
                }
                # Overwrite the default values with what is defined in the scenario config
                sensor_config.update(sensor)

                # set up sensor rates
                if self._ticks_per_sec < sensor_config["Hz"]:
                    raise ValueError(
                        f"{sensor_config['sensor_name']} is sampled at {sensor_config['Hz']} which is greater than ticks_per_sec {self._ticks_per_sec}"
                    )

                # round sensor rate as needed
                tick_every = self._ticks_per_sec / sensor_config["Hz"]
                if int(tick_every) != tick_every:
                    print(
                        f"{sensor_config['sensor_name']} rate {sensor_config['Hz']} is not a factor of ticks_per_sec {self._ticks_per_sec}, rounding to {self._ticks_per_sec//int(tick_every)}"
                    )
                sensor_config["tick_every"] = int(tick_every)

                sensors.append(
                    SensorDefinition(
                        agent["agent_name"],
                        agent["agent_type"],
                        sensor_config["sensor_name"],
                        sensor_config["sensor_type"],
                        socket=sensor_config["socket"],
                        location=sensor_config["location"],
                        rotation=sensor_config["rotation"],
                        config=sensor_config["configuration"],
                        tick_every=sensor_config["tick_every"],
                        lcm_channel=sensor_config["lcm_channel"],
                    )
                )

                if sensor_config["sensor_type"] in SensorDefinition._sonar_sensors:
                    self._has_sonar = True

                # Import LCM if needed
                if sensor_config["lcm_channel"] is not None and self._lcm is None:
                    globals()["lcm"] = __import__("lcm")
                    self._lcm = lcm.LCM(self._scenario["lcm_provider"])

            # Default values for an agent
            agent_config = {
                "location": [0, 0, 0],
                "rotation": [0, 0, 0],
                "agent_name": agent["agent_type"],
                "existing": False,
                "location_randomization": [0, 0, 0],
                "rotation_randomization": [0, 0, 0],
            }

            agent_config.update(agent)
            is_main_agent = False

            if "main_agent" in self._scenario:
                is_main_agent = self._scenario["main_agent"] == agent["agent_name"]

            agent_location = agent_config["location"]
            agent_rotation = agent_config["rotation"]

            # Randomize the agent start location
            dx = agent_config["location_randomization"][0]
            dy = agent_config["location_randomization"][1]
            dz = agent_config["location_randomization"][2]

            agent_location[0] += random.uniform(-dx, dx)
            agent_location[1] += random.uniform(-dy, dy)
            agent_location[2] += random.uniform(-dz, dz)

            # Randomize the agent rotation
            d_pitch = agent_config["rotation_randomization"][0]
            d_roll = agent_config["rotation_randomization"][1]
            d_yaw = agent_config["rotation_randomization"][2]

            agent_rotation[0] += random.uniform(-d_pitch, d_pitch)
            agent_rotation[1] += random.uniform(-d_roll, d_roll)
            agent_rotation[2] += random.uniform(-d_yaw, d_yaw)

            agent_def = AgentDefinition(
                agent_config["agent_name"],
                agent_config["agent_type"],
                starting_loc=agent_location,
                starting_rot=agent_rotation,
                sensors=sensors,
                existing=agent_config["existing"],
                is_main_agent=is_main_agent,
            )

            self.add_agent(agent_def, is_main_agent)
            self.agents[agent["agent_name"]].set_control_scheme(agent["control_scheme"])
            self._spawned_agent_defs.append(agent_def)

        if "weather" in self._scenario:
            weather = self._scenario["weather"]
            if "hour" in weather:
                self.weather.set_day_time(weather["hour"])
            if "type" in weather:
                self.weather.set_weather(weather["type"])
            if "fog_density" in weather:
                self.weather.set_fog_density(weather["fog_density"])
            if "day_cycle_length" in weather:
                day_cycle_length = weather["day_cycle_length"]
                self.weather.start_day_cycle(day_cycle_length)

    def reset(self):
        """Resets the environment, and returns the state.
        If it is a single agent environment, it returns that state for that agent. Otherwise, it
        returns a dict from agent name to state.

        Returns:
         :obj:`tuple` or :obj:`dict`:
            Returns the same as `tick`.
        """
        # Reset level
        self._initial_reset = True
        self._reset_ptr[0] = True
        for agent in self.agents.values():
            agent.clear_action()

        # reset all sensors
        for agent_name, agent in self.agents.items():
            for sensor_name, sensor in agent.sensors.items():
                sensor.reset()

        self.tick(
            publish=False
        )  # Must tick once to send reset before sending spawning commands
        self.tick(
            publish=False
        )  # Bad fix to potential race condition. See issue BYU-PCCL/holodeck#224
        self.tick(publish=False)
        # Clear command queue
        if self._command_center.queue_size > 0:
            print(
                "Warning: Reset called before all commands could be sent. Discarding",
                self._command_center.queue_size,
                "commands.",
            )
        self._command_center.clear()

        # Load agents
        self._spawned_agent_defs = []
        self.agents = dict()
        self._state_dict = dict()
        for agent_def in self._initial_agent_defs:
            self.add_agent(agent_def, agent_def.is_main_agent)

        self._load_scenario()

        self.num_agents = len(self.agents)

        if self.num_agents == 1:
            self._default_state_fn = self._get_single_state
        else:
            self._default_state_fn = self._get_full_state

        for _ in range(self._pre_start_steps + 1):
            self.tick(publish=False)

        return self._default_state_fn()

    def step(self, action, ticks=1, publish=True):
        """Supplies an action to the main agent and tells the environment to tick once.
        Primary mode of interaction for single agent environments.

        Args:
            action (:obj:`np.ndarray`): An action for the main agent to carry out on the next tick.
            ticks (:obj:`int`): Number of times to step the environment with this action.
                If ticks > 1, this function returns the last state generated.
            publish (:obj:`bool`): Whether or not to publish as defined by scenario. Defaults to True.

        Returns:
            :obj:`dict`:
                A dictionary from agent name to its full state. The full state is another
                dictionary from :obj:`holoocean.sensors.Sensors` enum to np.ndarray, containing the
                sensors information for each sensor. The sensors always include the reward and
                terminal sensors. Reward and terminals can also be gotten through :meth:`get_reward_terminal`.

                Will return the state from the last tick executed.
        """
        if not self._initial_reset:
            raise HoloOceanException("You must call .reset() before .step()")

        for _ in range(ticks):
            if self._agent is not None:
                self._agent.act(action)

            self._command_center.handle_buffer()
            self._client.release()
            self._client.acquire(self._timeout)

            last_state = self._default_state_fn()

            self._tick_sensor()
            self._num_ticks += 1

            if publish and self._lcm is not None:
                self._publish(last_state)

        return last_state

    def act(self, agent_name, action):
        """Supplies an action to a particular agent, but doesn't tick the environment.
           Primary mode of interaction for multi-agent environments. After all agent commands are
           supplied, they can be applied with a call to `tick`.

        Args:
            agent_name (:obj:`str`): The name of the agent to supply an action for.
            action (:obj:`np.ndarray` or :obj:`list`): The action to apply to the agent. This
                action will be applied every time `tick` is called, until a new action is supplied
                with another call to act.
        """
        self.agents[agent_name].act(action)

    def get_joint_constraints(self, agent_name, joint_name):
        """Returns the corresponding swing1, swing2 and twist limit values for the
                specified agent and joint. Will return None if the joint does not exist for the agent.

        Returns:
            :obj:`np.ndarray`
        """
        return self.agents[agent_name].get_joint_constraints(joint_name)

    def tick(self, num_ticks=1, publish=True):
        """Ticks the environment once. Normally used for multi-agent environments.

        Args:
            num_ticks (:obj:`int`): Number of ticks to perform. Defaults to 1.
            publish (:obj:`bool`): Whether or not to publish as defined by scenario. Defaults to True.

        Returns:
            :obj:`dict`:
                A dictionary from agent name to its full state. The full state is another
                dictionary from :obj:`holoocean.sensors.Sensors` enum to np.ndarray, containing the
                sensors information for each sensor. The sensors always include the reward and
                terminal sensors. Reward and terminals can also be gotten through :meth:`get_reward_terminal`.

                Will return the state from the last tick executed.
        """
        if not self._initial_reset:
            raise HoloOceanException("You must call .reset() before .tick()")

        for _ in range(num_ticks):
            self._command_center.handle_buffer()

            self._client.release()
            self._client.acquire(self._timeout)

            state = self._default_state_fn()

            self._tick_sensor()
            self._num_ticks += 1

            if publish and self._lcm is not None:
                self._publish(state)

        return state

    def _publish(self, state):
        """Publishes given state to channels chosen by the scenario config."""
        # if it was a partial state
        if self._agent.name not in state:
            state = {self._agent.name: state}

        # iterate through all agents and sensors
        for agent_name, agent in self.agents.items():
            for sensor_name, sensor in agent.sensors.items():
                # check if it's a full state, or single one
                if sensor.lcm_msg is not None and sensor_name in state[agent_name]:
                    # send message if it's in the dictionary and if LCM message is turned on
                    sensor.lcm_msg.set_value(
                        int(1000 * self._num_ticks / self._ticks_per_sec),
                        state[agent_name][sensor_name],
                    )
                    self._lcm.publish(
                        sensor.lcm_msg.channel, sensor.lcm_msg.msg.encode()
                    )

    def _enqueue_command(self, command_to_send):
        self._command_center.enqueue_command(command_to_send)

    def add_agent(self, agent_def, is_main_agent=False):
        """Add an agent in the world.

        It will be spawn when :meth:`tick` or :meth:`step` is called next.

        The agent won't be able to be used until the next frame.

        Args:
            agent_def (:class:`~holoocean.agents.AgentDefinition`): The definition of the agent to
            spawn.
        """
        if agent_def.name in self.agents:
            raise HoloOceanException("Error. Duplicate agent name. ")

        self.agents[agent_def.name] = AgentFactory.build_agent(self._client, agent_def)
        self._state_dict[agent_def.name] = self.agents[agent_def.name].agent_state_dict

        if not agent_def.existing:
            command_to_send = SpawnAgentCommand(
                location=agent_def.starting_loc,
                rotation=agent_def.starting_rot,
                name=agent_def.name,
                agent_type=agent_def.type.agent_type,
                is_main_agent=agent_def.is_main_agent,
            )

            self._client.command_center.enqueue_command(command_to_send)
        self.agents[agent_def.name].add_sensors(agent_def.sensors)
        if is_main_agent:
            self._agent = self.agents[agent_def.name]

    def spawn_prop(
        self,
        prop_type,
        location=None,
        rotation=None,
        scale=1,
        sim_physics=False,
        material="",
        tag="",
    ):
        """Spawns a basic prop object in the world like a box or sphere.

        Prop will not persist after environment reset.

        Args:
            prop_type (:obj:`string`):
                The type of prop to spawn. Can be ``box``, ``sphere``, ``cylinder``, or ``cone``.

            location (:obj:`list` of :obj:`float`):
                The ``[x, y, z]`` location of the prop.

            rotation (:obj:`list` of :obj:`float`):
                The ``[roll, pitch, yaw]`` rotation of the prop.

            scale (:obj:`list` of :obj:`float`) or (:obj:`float`):
                The ``[x, y, z]`` scalars to the prop size, where the default size is 1 meter.
                If given a single float value, then every dimension will be scaled to that value.

            sim_physics (:obj:`boolean`):
                Whether the object is mobile and is affected by gravity.

            material (:obj:`string`):
                The type of material (texture) to apply to the prop. Can be ``white``, ``gold``,
                ``cobblestone``, ``brick``, ``wood``, ``grass``, ``steel``, or ``black``. If left
                empty, the prop will have the a simple checkered gray material.

            tag (:obj:`string`):
                The tag to apply to the prop. Useful for task references.
        """
        location = [0, 0, 0] if location is None else location
        rotation = [0, 0, 0] if rotation is None else rotation
        # if the given scale is an single value, then scale every dimension to that value
        if not isinstance(scale, list):
            scale = [scale, scale, scale]
        sim_physics = 1 if sim_physics else 0

        prop_type = prop_type.lower()
        material = material.lower()

        available_props = ["box", "sphere", "cylinder", "cone"]
        available_materials = [
            "white",
            "gold",
            "cobblestone",
            "brick",
            "wood",
            "grass",
            "steel",
            "black",
        ]

        if prop_type not in available_props:
            raise HoloOceanException(
                "{} not an available prop. Available prop types: {}".format(
                    prop_type, available_props
                )
            )
        if material not in available_materials and material != "":
            raise HoloOceanException(
                "{} not an available material. Available material types: {}".format(
                    material, available_materials
                )
            )

        self.send_world_command(
            "SpawnProp",
            num_params=[location, rotation, scale, sim_physics],
            string_params=[prop_type, material, tag],
        )

    def draw_line(self, start, end, color=None, thickness=10.0, lifetime=1.0):
        """Draws a debug line in the world

        Args:
            start (:obj:`list` of :obj:`float`): The start ``[x, y, z]`` location in meters of the line.
                (see :ref:`coordinate-system`)
            end (:obj:`list` of :obj:`float`): The end ``[x, y, z]`` location in meters of the line
            color (:obj:`list``): ``[r, g, b]`` color value (from 0 to 255). Defaults to [255, 0, 0] (red).
            thickness (:obj:`float`): Thickness of the line. Defaults to 10.
            lifetime (:obj:`float`): Number of simulation seconds the object should persist. If 0, makes persistent. Defaults to 1.
        """
        color = [255, 0, 0] if color is None else color
        command_to_send = DebugDrawCommand(0, start, end, color, thickness, lifetime)
        self._enqueue_command(command_to_send)

    def draw_arrow(self, start, end, color=None, thickness=10.0, lifetime=1.0):
        """Draws a debug arrow in the world

        Args:
            start (:obj:`list` of :obj:`float`): The start ``[x, y, z]`` location in meters of the line.
                (see :ref:`coordinate-system`)
            end (:obj:`list` of :obj:`float`): The end ``[x, y, z]`` location in meters of the arrow
            color (:obj:`list`): ``[r, g, b]`` color value (from 0 to 255). Defaults to [255, 0, 0] (red).
            thickness (:obj:`float`): Thickness of the arrow. Defaults to 10.
            lifetime (:obj:`float`): Number of simulation seconds the object should persist. If 0, makes persistent. Defaults to 1.
        """
        color = [255, 0, 0] if color is None else color
        command_to_send = DebugDrawCommand(1, start, end, color, thickness, lifetime)
        self._enqueue_command(command_to_send)

    def draw_box(self, center, extent, color=None, thickness=10.0, lifetime=1.0):
        """Draws a debug box in the world

        Args:
            center (:obj:`list` of :obj:`float`): The start ``[x, y, z]`` location in meters of the box.
                (see :ref:`coordinate-system`)
            extent (:obj:`list` of :obj:`float`): The ``[x, y, z]`` extent of the box
            color (:obj:`list`): ``[r, g, b]`` color value (from 0 to 255). Defaults to [255, 0, 0] (red).
            thickness (:obj:`float`): Thickness of the lines. Defaults to 10.
            lifetime (:obj:`float`): Number of simulation seconds the object should persist. If 0, makes persistent. Defaults to 1.
        """
        color = [255, 0, 0] if color is None else color
        command_to_send = DebugDrawCommand(
            2, center, extent, color, thickness, lifetime
        )
        self._enqueue_command(command_to_send)

    def draw_point(self, loc, color=None, thickness=10.0, lifetime=1.0):
        """Draws a debug point in the world

        Args:
            loc (:obj:`list` of :obj:`float`): The ``[x, y, z]`` start of the box.
                (see :ref:`coordinate-system`)
            color (:obj:`list` of :obj:`float`): ``[r, g, b]`` color value (from 0 to 255). Defaults to [255, 0, 0] (red).
            thickness (:obj:`float`): Thickness of the point. Defaults to 10.
            lifetime (:obj:`float`): Number of simulation seconds the object should persist. If 0, makes persistent. Defaults to 1.
        """
        color = [255, 0, 0] if color is None else color
        command_to_send = DebugDrawCommand(
            3, loc, [0, 0, 0], color, thickness, lifetime
        )
        self._enqueue_command(command_to_send)

    def move_viewport(self, location, rotation):
        """Teleport the camera to the given location

        By the next tick, the camera's location and rotation will be updated

        Args:
            location (:obj:`list` of :obj:`float`): The ``[x, y, z]`` location to give the camera
                (see :ref:`coordinate-system`)
            rotation (:obj:`list` of :obj:`float`): The x-axis that the camera should look down. Other 2 axes are formed
                by a horizontal y-aixs, and then the corresponding z-axis.
                (see :ref:`rotations`)

        """
        # test_viewport_capture_after_teleport
        self._enqueue_command(TeleportCameraCommand(location, rotation))

    def should_render_viewport(self, render_viewport):
        """Controls whether the viewport is rendered or not

        Args:
            render_viewport (:obj:`boolean`): If the viewport should be rendered
        """
        self._enqueue_command(RenderViewportCommand(render_viewport))

    def set_render_quality(self, render_quality):
        """Adjusts the rendering quality of HoloOcean.

        Args:
            render_quality (:obj:`int`): An integer between 0 = Low Quality and 3 = Epic quality.
        """
        self._enqueue_command(RenderQualityCommand(render_quality))

    def set_control_scheme(self, agent_name, control_scheme):
        """Set the control scheme for a specific agent.

        Args:
            agent_name (:obj:`str`): The name of the agent to set the control scheme for.
            control_scheme (:obj:`int`): A control scheme value
                (see :class:`~holoocean.agents.ControlSchemes`)
        """
        if agent_name not in self.agents:
            print("No such agent %s" % agent_name)
        else:
            self.agents[agent_name].set_control_scheme(control_scheme)

    def send_world_command(self, name, num_params=None, string_params=None):
        """Send a world command.

        A world command sends an abitrary command that may only exist in a specific world or
        package. It is given a name and any amount of string and number parameters that allow it to
        alter the state of the world.

        If a command is sent that does not exist in the world, the environment will exit.

        Args:
            name (:obj:`str`): The name of the command, ex "OpenDoor"
            num_params (obj:`list` of :obj:`int`): List of arbitrary number parameters
            string_params (obj:`list` of :obj:`string`): List of arbitrary string parameters
        """
        num_params = [] if num_params is None else num_params
        string_params = [] if string_params is None else string_params

        command_to_send = CustomCommand(name, num_params, string_params)
        self._enqueue_command(command_to_send)

    def __linux_start_process__(
        self, binary_path, task_key, gl_version, verbose, show_viewport=True
    ):
        import posix_ipc

        out_stream = sys.stdout if verbose else open(os.devnull, "w")
        loading_semaphore = posix_ipc.Semaphore(
            "/HOLODECK_LOADING_SEM" + self._uuid,
            os.O_CREAT | os.O_EXCL,
            initial_value=0,
        )
        arguments = [
            binary_path,
            task_key,
            "-HolodeckOn",
            "-LOG=HolodeckLog.txt",
            "-ForceRes",
            "-ResX=" + str(self._window_size[1]),
            "-ResY=" + str(self._window_size[0]),
            "--HolodeckUUID=" + self._uuid,
            "-TicksPerSec=" + str(self._ticks_per_sec),
            "-FramesPerSec=" + str(self._frames_per_sec),
            "-EnvMinX=" + str(self._env_min[0]),
            "-EnvMinY=" + str(self._env_min[1]),
            "-EnvMinZ=" + str(self._env_min[2]),
            "-EnvMaxX=" + str(self._env_max[0]),
            "-EnvMaxY=" + str(self._env_max[1]),
            "-EnvMaxZ=" + str(self._env_max[2]),
            "-OctreeMin=" + str(self._octree_min),
            "-OctreeMax=" + str(self._octree_max),
        ]

        if not show_viewport:
            arguments.append("-RenderOffScreen")

        self._world_process = subprocess.Popen(
            arguments, stdout=out_stream, stderr=out_stream
        )

        atexit.register(self.__on_exit__)

        try:
            loading_semaphore.acquire(30)
        except posix_ipc.BusyError:
            raise HoloOceanException(
                "Timed out waiting for binary to load. Ensure that holoocean "
                "is not being run with root priveleges."
            )
        loading_semaphore.unlink()

    def __windows_start_process__(
        self, binary_path, task_key, verbose, show_viewport=True
    ):
        import win32event

        out_stream = sys.stdout if verbose else open(os.devnull, "w")
        loading_semaphore = win32event.CreateSemaphore(
            None, 0, 1, "Global\\HOLODECK_LOADING_SEM" + self._uuid
        )

        arguments = [
            binary_path,
            task_key,
            "-HolodeckOn",
            "-LOG=HolodeckLog.txt",
            "-ForceRes",
            "-ResX=" + str(self._window_size[1]),
            "-ResY=" + str(self._window_size[0]),
            "--HolodeckUUID=" + self._uuid,
            "-TicksPerSec=" + str(self._ticks_per_sec),
            "-FramesPerSec=" + str(self._frames_per_sec),
            "-EnvMinX=" + str(self._env_min[0]),
            "-EnvMinY=" + str(self._env_min[1]),
            "-EnvMinZ=" + str(self._env_min[2]),
            "-EnvMaxX=" + str(self._env_max[0]),
            "-EnvMaxY=" + str(self._env_max[1]),
            "-EnvMaxZ=" + str(self._env_max[2]),
            "-OctreeMin=" + str(self._octree_min),
            "-OctreeMax=" + str(self._octree_max),
        ]

        if not show_viewport:
            arguments.append("-RenderOffScreen")

        self._world_process = subprocess.Popen(
            arguments, stdout=out_stream, stderr=out_stream
        )

        atexit.register(self.__on_exit__)
        response = win32event.WaitForSingleObject(
            loading_semaphore, 100000
        )  # 100 second timeout
        if response == win32event.WAIT_TIMEOUT:
            raise HoloOceanException("Timed out waiting for binary to load")

    def __on_exit__(self):
        if hasattr(self, "_exited"):
            return

        if hasattr(self,"_client"):
            self._client.unlink()
        if hasattr(self, "_world_process"):
            self._world_process.kill()
            self._world_process.wait(10)

        self._exited = True

    # Context manager APIs, allows `with` statement to be used
    def __enter__(self):
        return self

    def __exit__(self, exc_type, exc_value, traceback):
        # TODO: Surpress exceptions?
        self.__on_exit__()

    def _tick_sensor(self):
        for agent_name, agent in self.agents.items():
            for sensor_name, sensor in agent.sensors.items():
                # if it's not time, tick the sensor
                if sensor.tick_count != sensor.tick_every:
                    sensor.tick_count += 1
                # otherwise remove, it and reset count
                else:
                    sensor.tick_count = 1

    def _get_single_state(self):
        if self._agent is not None:
            # rebuild state dictionary to drop/change data as needed
            if self._copy_state:
                state = dict()
                for sensor_name, sensor in self.agents[
                    self._agent.name
                ].sensors.items():
                    data = sensor.sensor_data
                    if isinstance(data, np.ndarray):
                        state[sensor_name] = np.copy(data)
                    elif data is not None:
                        state[sensor_name] = data

                state["t"] = self._num_ticks / self._ticks_per_sec

            else:
                state = self._state_dict[self._agent.name]

            return state

        return self._get_full_state()

    def _get_full_state(self):
        # rebuild state dictionary to drop/change data as needed
        if self._copy_state:
            state = dict()
            for agent_name, agent in self.agents.items():
                state[agent_name] = dict()
                for sensor_name, sensor in agent.sensors.items():
                    data = sensor.sensor_data
                    if isinstance(data, np.ndarray):
                        state[agent_name][sensor_name] = np.copy(data)
                    elif data is not None:
                        state[agent_name][sensor_name] = data

            state["t"] = self._num_ticks / self._ticks_per_sec

        else:
            state = self._state_dict

        return state

    def get_reward_terminal(self):
        """Returns the reward and terminal state

        Returns:
            (:obj:`float`, :obj:`bool`):
                A 2tuple:

                - Reward (:obj:`float`): Reward returned by the environment.
                - Terminal: The bool terminal signal returned by the environment.

        """
        reward = None
        terminal = None
        if self._agent is not None:
            for sensor in self._state_dict[self._agent.name]:
                if "Task" in sensor:
                    reward = self._state_dict[self._agent.name][sensor][0]
                    terminal = self._state_dict[self._agent.name][sensor][1] == 1
        return reward, terminal

    def _create_copy(self, obj):
        if isinstance(obj, dict):  # Deep copy dictionary
            copy = dict()
            for k, v in obj.items():
                if isinstance(v, dict):
                    copy[k] = self._create_copy(v)
                else:
                    copy[k] = np.copy(v)
            return copy
        return None  # Not implemented for other types

    ######################### HOLOOCEAN CUSTOM #############################

    ######################## ACOUSTIC BEACON HELPERS ###########################

    def send_acoustic_message(self, id_from, id_to, msg_type, msg_data):
        """Send a message from one beacon to another.

        Args:
            id_from (:obj:`int`): The integer ID of the transmitting modem.
            id_to (:obj:`int`): The integer ID of the receiving modem.
            msg_type (:obj:`str`): The message type. See :class:`holoocean.sensors.AcousticBeaconSensor` for a list.
            msg_data : The message to be transmitted. Currently can be any python object.
        """
        AcousticBeaconSensor.instances[id_from].send_message(id_to, msg_type, msg_data)

    @property
    def beacons(self):
        """Gets all instances of AcousticBeaconSensor in the environment.

        Returns:
            (:obj:`list` of :obj:`AcousticBeaconSensor`): List of all AcousticBeaconSensor in environment
        """
        return AcousticBeaconSensor.instances

    @property
    def beacons_id(self):
        """Gets all ids of AcousticBeaconSensor in the environment.

        Returns:
            (:obj:`list` of :obj:`int`): List of all AcousticBeaconSensor ids in environment
        """
        return list(AcousticBeaconSensor.instances.keys())

    @property
    def beacons_status(self):
        """Gets all status of AcousticBeaconSensor in the environment.

        Returns:
            (:obj:`list` of :obj:`str`): List of all AcousticBeaconSensor status in environment
        """
        return [i.status for i in AcousticBeaconSensor.instances.values()]

    ####################### OPTICAL MODEM HELPERS ###############################

    def send_optical_message(self, id_from, id_to, msg_data):
        """Sends data between various instances of OpticalModemSensor

        Args:
            id_from (:obj:`int`): The integer ID of the transmitting modem.
            id_to (:obj:`int`): The integer ID of the receiving modem.
            msg_data : The message to be transmitted. Currently can be any python object.
        """

        OpticalModemSensor.instances[id_from].send_message(id_to, msg_data)

    @property
    def modems(self):
        """Gets all instances of OpticalModemSensor in the environment.

        Returns:
            (:obj:`list` of :obj:`OpticalModemSensor`): List of all OpticalModemSensor in environment
        """
        return OpticalModemSensor.instances

    @property
    def modems_id(self):
        """Gets all ids of OpticalModemSensor in the environment.

        Returns:
            (:obj:`list` of :obj:`int`): List of all OpticalModemSensor ids in environment
        """
        return list(OpticalModemSensor.instances.keys())
