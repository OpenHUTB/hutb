"""Definitions for different agents that can be controlled from HoloOcean"""
from functools import reduce

import numpy as np
from . import joint_constraints

from holoocean.spaces import ContinuousActionSpace, DiscreteActionSpace
from holoocean.sensors import SensorDefinition, SensorFactory, RGBCamera
from holoocean.command import AddSensorCommand, RemoveSensorCommand


class ControlSchemes:
    """All allowed control schemes.

    Attributes:
        ANDROID_TORQUES (int): Default Android control scheme. Specify a torque for each joint.
        CONTINUOUS_SPHERE_DEFAULT (int): Default ContinuousSphere control scheme.
            Takes two commands, [forward_delta, turn_delta].
        DISCRETE_SPHERE_DEFAULT (int): Default DiscreteSphere control scheme. Takes a value, 0-4,
            which corresponds with forward, backward, right, and left.
        NAV_TARGET_LOCATION (int): Default NavAgent control scheme. Takes a target xyz coordinate.
        UAV_TORQUES (int): Default UAV control scheme. Takes torques for roll, pitch, and yaw, as
            well as thrust.
        UAV_ROLL_PITCH_YAW_RATE_ALT (int): Control scheme for UAV. Takes roll, pitch, yaw rate, and
            altitude targets.
        HAND_AGENT_MAX_TORQUES (int): Default Android control scheme. Specify a torque for each joint.
        AUV_THRUSTERS (int): Default HoveringAUV control scheme. Specify 8-vector of forces for each thruster.
        AUV_CONTROL (int): Implemented PD controller. Specify 6-vector of position and roll,pitch,yaw to go too.
        AUV_FORCES (int): Used for custom dynamics. All internal dynamics (except collisions) are turned off including
            buoyancy, gravity, and damping. Specify 6-vector of linear and angular acceleration in the global frame.
        TAUV_FINS (int): Default TorpedoAUV control scheme. Specify 5-vector of fin rotations in degrees and propeller value in Newtons.
        TAUV_FORCES (int): Used for custom dynamics. All internal dynamics (except collisions) are turned off including
            buoyancy, gravity, and damping. Specify 6-vector of linear and angular acceleration in the global frame.
        SV_THRUSTERS (int): Default SurfaceVessel control scheme. Specify 2-vector of forces for left and right thruster.
        SV_CONTROL (int): Implemented PD controller. Specify 2-vector of x and y position to go too.
        SV_FORCES (int): Used for custom dynamics. All internal dynamics (except collisions) are turned off including
            buoyancy, gravity, and damping. Specify 6-vector of linear and angular acceleration in the global frame.
    """
    # Android Control Schemes
    ANDROID_DIRECT_TORQUES = 0
    ANDROID_MAX_SCALED_TORQUES = 1

    # Sphere Agent Control Schemes
    SPHERE_DISCRETE = 0
    SPHERE_CONTINUOUS = 1

    # Nav Agent Control Schemes
    NAV_TARGET_LOCATION = 0

    # Turtle agent
    TURTLE_DIRECT_TORQUES = 0

    # UAV Control Schemes
    UAV_TORQUES = 0
    UAV_ROLL_PITCH_YAW_RATE_ALT = 1

    # HandAgent Control Schemes
    HAND_AGENT_MAX_TORQUES = 0
    HAND_AGENT_MAX_SCALED_TORQUES = 1
    HAND_AGENT_MAX_TORQUES_FLOAT = 2

    # Hovering AUV Control Schemes
    AUV_THRUSTERS = 0
    AUV_CONTROL = 1
    AUV_FORCES = 2

    # Torpedo AUV Control Schemes
    TAUV_FINS = 0
    TAUV_FORCES = 1

    # Surface Vessel Control Schemes
    SV_THRUSTERS = 0
    SV_CONTROL = 1
    SV_FORCES = 2

class HoloOceanAgent:
    """A learning agent in HoloOcean

    Agents can act, receive rewards, and receive observations from their sensors.
    Examples include the Android, UAV, and SphereRobot.

    Args:
        client (:class:`~holoocean.holooceanclient.HoloOceanClient`): The HoloOceanClient that this
            agent belongs with.
        name (:obj:`str`, optional): The name of the agent. Must be unique from other agents in
            the same environment.
        sensors (:obj:`dict` of (:obj:`str`, :class:`~holoocean.sensors.HoloOceanSensor`)): A list
            of HoloOceanSensors to read from this agent.

    Attributes:
        name (:obj:`str`): The name of the agent.
        sensors (dict of (string, :class:`~holoocean.sensors.HoloOceanSensor`)): List of
            HoloOceanSensors on this agent.
        agent_state_dict (dict): A dictionary that maps sensor names to sensor observation data.
    """

    def __init__(self, client, name="DefaultAgent"):
        self.name = name
        self._client = client
        self.agent_state_dict = dict()
        self.sensors = dict()

        self._num_control_schemes = len(self.control_schemes)

        self._max_control_scheme_length = \
            max(map(lambda x: reduce(lambda i, j: i * j, x[1].buffer_shape),
                    self.control_schemes))

        self._action_buffer = \
            self._client.malloc(name, [self._max_control_scheme_length], np.float32)
        # Teleport flag: 0: do nothing, 1: teleport, 2: rotate, 3: teleport and rotate
        self._teleport_type_buffer = self._client.malloc(name + "_teleport_flag", [1], np.uint8)
        self._teleport_buffer = self._client.malloc(name + "_teleport_command", [12], np.float32)
        self._control_scheme_buffer = self._client.malloc(name + "_control_scheme", [1],
                                                          np.uint8)
        self._current_control_scheme = 0
        self.set_control_scheme(0)

    def act(self, action):
        """Sets the command for the agent. Action depends on the agent type and current control
        scheme.

        Args:
            action(:obj:`np.ndarray`): The action to take.
        """
        self.__act__(action)

    def clear_action(self):
        """Sets the action to zeros, effectively removing any previous actions.
        """
        np.copyto(self._action_buffer, np.zeros(self._action_buffer.shape))

    def set_control_scheme(self, index):
        """Sets the control scheme for the agent. See :class:`ControlSchemes`.

        Args:
            index (:obj:`int`): The control scheme to use. Should be set with an enum from
                :class:`ControlSchemes`.
        """
        self._current_control_scheme = index % self._num_control_schemes
        self._control_scheme_buffer[0] = self._current_control_scheme

    def teleport(self, location=None, rotation=None):
        """Teleports the agent to a specific location, with a specific rotation.

        Args:
            location (np.ndarray, optional): An array with three elements specifying the target
                world coordinates ``[x, y, z]`` in meters (see :ref:`coordinate-system`).
                
                If ``None`` (default), keeps the current location.
            rotation (np.ndarray, optional): An array with three elements specifying roll, pitch,
                and yaw in degrees of the agent.
                
                If ``None`` (default), keeps the current rotation.

        """
        val = 0
        if location is not None:
            val += 1
            np.copyto(self._teleport_buffer[0:3], location)
        if rotation is not None:
            np.copyto(self._teleport_buffer[3:6], rotation)
            val += 2
        self._teleport_type_buffer[0] = val

    def set_physics_state(self, location, rotation, velocity, angular_velocity):
        """Sets the location, rotation, velocity and angular velocity of an agent.

        Args:
            location (np.ndarray): New location (``[x, y, z]`` (see :ref:`coordinate-system`))
            rotation (np.ndarray): New rotation (``[roll, pitch, yaw]``, see (see :ref:`rotations`))
            velocity (np.ndarray): New velocity (``[x, y, z]`` (see :ref:`coordinate-system`))
            angular_velocity (np.ndarray): New angular velocity (``[x, y, z]`` in **degrees** 
                (see :ref:`coordinate-system`))

        """
        np.copyto(self._teleport_buffer[0:3], location)
        np.copyto(self._teleport_buffer[3:6], rotation)
        np.copyto(self._teleport_buffer[6:9], velocity)
        np.copyto(self._teleport_buffer[9:12], angular_velocity)
        self._teleport_type_buffer[0] = 15

    def add_sensors(self, sensor_defs):
        """Adds a sensor to a particular agent object and attaches an instance of the sensor to the
        agent in the world.

        Args:
            sensor_defs (:class:`~holoocean.sensors.HoloOceanSensor` or
                         list of :class:`~holoocean.sensors.HoloOceanSensor`):
                Sensors to add to the agent.
        """
        if not isinstance(sensor_defs, list):
            sensor_defs = [sensor_defs]

        for sensor_def in sensor_defs:
            if sensor_def.agent_name == self.name:
                sensor = SensorFactory.build_sensor(self._client, sensor_def)
                self.sensors[sensor_def.sensor_name] = sensor
                self.agent_state_dict[sensor_def.sensor_name] = sensor.sensor_data

                if not sensor_def.existing:
                    command_to_send = AddSensorCommand(sensor_def)
                    self._client.command_center.enqueue_command(command_to_send)

    def remove_sensors(self, sensor_defs):
        """Removes a sensor from a particular agent object and detaches it from the agent in the
        world.

        Args:
            sensor_defs (:class:`~holoocean.sensors.HoloOceanSensor` or
                         list of :class:`~holoocean.sensors.HoloOceanSensor`):
                Sensors to remove from the agent.
        """
        if not isinstance(sensor_defs, list):
            sensor_defs = [sensor_defs]

        for sensor_def in sensor_defs:
            self.sensors.pop(sensor_def.sensor_name, None)
            self.agent_state_dict.pop(sensor_def.sensor_name, None)
            command_to_send = RemoveSensorCommand(self.name, sensor_def.sensor_name)
            self._client.command_center.enqueue_command(command_to_send)

    def has_camera(self):
        """Indicatates whether this agent has a camera or not.

        Returns:
            :obj:`bool`: If the agent has a sensor or not
        """
        for sensor_type in self.sensors.items():
            if sensor_type is RGBCamera:
                return True

        return False

    @property
    def action_space(self):
        """Gets the action space for the current agent and control scheme.

        Returns:
            :class:`~holoocean.spaces.ActionSpace`: The action space for this agent and control
                scheme."""
        return self.control_schemes[self._current_control_scheme][1]

    @property
    def control_schemes(self):
        """A list of all control schemes for the agent. Each list element is a 2-tuple, with the
        first element containing a short description of the control scheme, and the second
        element containing the :obj:`ActionSpace` for the control scheme.

        Returns:
            (:obj:`str`, :class:`~holoocean.spaces.ActionSpace`):
                Each tuple contains a short description and the ActionSpace
        """
        raise NotImplementedError("Child class must implement this function")

    def get_joint_constraints(self, joint_name):
        """Returns the corresponding swing1, swing2 and twist limit values for the
        specified joint. Will return None if the joint does not exist for the agent.
        """
        raise NotImplementedError("Child class must implement this function")

    def __act__(self, action):
        # Allow for smaller arrays to be provided as input
        if len(self._action_buffer) > len(action):
            action = np.copy(action)
            action.resize(self._action_buffer.shape)

        # The default act function is to copy the data,
        # but if needed it can be overridden
        np.copyto(self._action_buffer, action)

    def __repr__(self):
        return self.name


class UavAgent(HoloOceanAgent):
    """A UAV (quadcopter) agent

    **Action Space:**

    Has two possible continuous action control schemes

    1. [pitch_torque, roll_torque, yaw_torque, thrust] and
    2. [pitch_target, roll_target, yaw_rate_target, altitude_target]

    See :ref:`uav-agent` for more details.

    Inherits from :class:`HoloOceanAgent`.
    """
    # constants in Uav.h in holoocean-engine
    __MAX_ROLL = 6.5080
    __MIN_ROLL = -__MAX_ROLL

    __MAX_PITCH = 5.087
    __MIN_PITCH = -__MAX_PITCH

    __MAX_YAW_RATE = .8
    __MIN_YAW_RATE = -__MAX_YAW_RATE

    __MAX_FORCE = 59.844
    __MIN_FORCE = -__MAX_FORCE

    agent_type = "UAV"

    @property
    def control_schemes(self):
        torques_min = [self.__MIN_PITCH, self.__MIN_ROLL, self.__MIN_YAW_RATE, self.__MIN_FORCE]
        torques_max = [self.__MAX_PITCH, self.__MAX_ROLL, self.__MAX_YAW_RATE, self.__MAX_FORCE]
        no_min_max = [None, None, None, None]
        return [("[pitch_torque, roll_torque, yaw_torque, thrust]",
                 ContinuousActionSpace([4], low=torques_min, high=torques_max)),
                ("[pitch_target, roll_target, yaw_rate_target, altitude_target]",
                 ContinuousActionSpace([4], low=no_min_max, high=no_min_max))]

    def __repr__(self):
        return "UavAgent " + self.name

    def get_joint_constraints(self, joint_name):
        return None


class SphereAgent(HoloOceanAgent):
    """A basic sphere robot.

    See :ref:`sphere-agent` for more details.

    **Action Space:**

    Has two possible control schemes, one discrete and one continuous:

    +-------------------+---------+----------------------+
    | Control Scheme    | Value   | Action               |
    +-------------------+---------+----------------------+
    | Discrete (``0``)  | ``[0]`` | Move forward         |
    |                   +---------+----------------------+
    |                   | ``[1]`` | Move backward        |
    |                   +---------+----------------------+
    |                   | ``[2]`` | Turn right           |
    |                   +---------+----------------------+
    |                   | ``[3]`` | Turn left            |
    +-------------------+---------+----------------------+
    | Continuous (``1``)| ``[forward_speed, rot_speed]`` |
    +-------------------+--------------------------------+

    Inherits from :class:`HoloOceanAgent`.
    """
    # constants in SphereRobot.h in holoocean-engine
    __DISCRETE_MIN = 0
    __DISCRETE_MAX = 4

    __MAX_ROTATION_SPEED = 20
    __MIN_ROTATION_SPEED = -__MAX_ROTATION_SPEED

    __MAX_FORWARD_SPEED = 20
    __MIN_FORWARD_SPEED = -__MAX_FORWARD_SPEED

    agent_type = "SphereRobot"

    @property
    def control_schemes(self):
        cont_min = [self.__MIN_FORWARD_SPEED, self.__MIN_ROTATION_SPEED]
        cont_max = [self.__MAX_FORWARD_SPEED, self.__MAX_ROTATION_SPEED]
        return [("0: Move forward\n1: Move backward\n2: Turn right\n3: Turn left",
                 DiscreteActionSpace([1], low=self.__DISCRETE_MIN, high=self.__DISCRETE_MAX, buffer_shape=[2])),
                ("[forward_movement, rotation]", ContinuousActionSpace([2], low=cont_min, high=cont_max))]

    def get_joint_constraints(self, joint_name):
        return None

    def __act__(self, action):
        if self._current_control_scheme is ControlSchemes.SPHERE_CONTINUOUS:
            np.copyto(self._action_buffer, action)
        elif self._current_control_scheme is ControlSchemes.SPHERE_DISCRETE:
            # Move forward .185 meters (to match initial release)
            # Turn 10/-10 degrees (to match initial release)
            actions = np.array([[.185, 0], [-.185, 0], [0, 10], [0, -10]])
            to_act = np.array(actions[action, :])
            np.copyto(self._action_buffer, to_act)

    def __repr__(self):
        return "SphereAgent " + self.name


class AndroidAgent(HoloOceanAgent):
    """An humanoid android agent.

    Can be controlled via torques supplied to its joints.

    **Action Space:**

    94 dimensional vector of continuous values representing torques to be
    applied at each joint. The layout of joints can be found here:

    There are 18 joints with 3 DOF, 10 with 2 DOF, and 20 with 1 DOF.

    Inherits from :class:`HoloOceanAgent`."""
    # constants in Android.h in holoocean-engine
    __MAX_TORQUE = 20
    __MIN_TORQUE = -__MAX_TORQUE
    __JOINTS_VECTOR_SIZE = 94

    agent_type = "Android"

    @property
    def control_schemes(self):
        direct_min = [self.__MIN_TORQUE for _ in range(self.__JOINTS_VECTOR_SIZE)]
        direct_max = [self.__MAX_TORQUE for _ in range(self.__JOINTS_VECTOR_SIZE)]
        scaled_min = [-1 for _ in range(self.__JOINTS_VECTOR_SIZE)]
        scaled_max = [1 for _ in range(self.__JOINTS_VECTOR_SIZE)]

        return [("[Raw Bone Torques] * 94",
                 ContinuousActionSpace([self.__JOINTS_VECTOR_SIZE], low=direct_min, high=direct_max)),
                ("[-1 to 1] * 94, where 1 is the maximum torque for a given "
                 "joint (based on mass of bone)",
                 ContinuousActionSpace([self.__JOINTS_VECTOR_SIZE], low=scaled_min, high=scaled_max))]

    def __repr__(self):
        return "AndroidAgent " + self.name

    @staticmethod
    def joint_ind(joint_name):
        """Gets the joint indices for a given name

        Args:
            joint_name (:obj:`str`): Name of the joint to look up

        Returns:
            (int): The index into the state array
        """
        return AndroidAgent._joint_indices[joint_name]

    def get_joint_constraints(self, joint_name):
        if joint_name in joint_constraints.android_agent_joints_constraints:
            return joint_constraints.android_agent_joints_constraints[joint_name]
        return None

    _joint_indices = {
        # Head, Spine, and Arm joints. Each has[swing1, swing2, twist]
        "head": 0,
        "neck_01": 3,
        "spine_02": 6,
        "spine_01": 9,
        "upperarm_l": 12,
        "lowerarm_l": 15,
        "hand_l": 18,
        "upperarm_r": 21,
        "lowerarm_r": 24,
        "hand_r": 27,

        # Leg Joints. Each has[swing1, swing2, twist]
        "thigh_l": 30,
        "calf_l": 33,
        "foot_l": 36,
        "ball_l": 39,
        "thigh_r": 42,
        "calf_r": 45,
        "foot_r": 48,
        "ball_r": 51,

        # First joint of each finger. Has only [swing1, swing2]
        "thumb_01_l": 54,
        "index_01_l": 56,
        "middle_01_l": 58,
        "ring_01_l": 60,
        "pinky_01_l": 62,
        "thumb_01_r": 64,
        "index_01_r": 66,
        "middle_01_r": 68,
        "ring_01_r": 70,
        "pinky_01_r": 72,

        # Second joint of each finger. Has only[swing1]
        "thumb_02_l": 74,
        "index_02_l": 75,
        "middle_02_l": 76,
        "ring_02_l": 77,
        "pinky_02_l": 78,
        "thumb_02_r": 79,
        "index_02_r": 80,
        "middle_02_r": 81,
        "ring_02_r": 82,
        "pinky_02_r": 83,

        # Third joint of each finger. Has only[swing1]
        "thumb_03_l": 84,
        "index_03_l": 85,
        "middle_03_l": 86,
        "ring_03_l": 87,
        "pinky_03_l": 88,
        "thumb_03_r": 89,
        "index_03_r": 90,
        "middle_03_r": 91,
        "ring_03_r": 92,
        "pinky_03_r": 93
    }


class HandAgent(HoloOceanAgent):
    """A floating hand agent.

    Can be controlled via torques supplied to its joints and moved around in
    three dimensions.
    
    **Action Space:**

    23 or 26 dimensional vector of continuous values representing torques to be
    applied at each joint.

    Inherits from :class:`HoloOceanAgent`.
    
    """
    # constants in HandAgent.h in holoocean-engine
    __MAX_MOVEMENT_METERS = 0.5
    __MIN_MOVEMENT_METERS = -__MAX_MOVEMENT_METERS

    __MAX_TORQUE = 20
    __MIN_TORQUE = -__MAX_TORQUE

    __JOINTS_DOF = 23
    __JOINTS_AND_DIST = 26

    agent_type = "HandAgent"

    @property
    def control_schemes(self):
        raw_min = [self.__MIN_TORQUE for _ in range(self.__JOINTS_DOF)]
        raw_max = [self.__MAX_TORQUE for _ in range(self.__JOINTS_DOF)]
        joint_min = [-1 for _ in range(self.__JOINTS_DOF)]
        joint_max = [1 for _ in range(self.__JOINTS_DOF)]

        scaled_min = [-1.0 if i < self.__JOINTS_DOF
                      else self.__MIN_MOVEMENT_METERS for i in range(self.__JOINTS_AND_DIST)]
        scaled_max = [1.0 if i < self.__JOINTS_DOF
                      else self.__MAX_MOVEMENT_METERS for i in range(self.__JOINTS_AND_DIST)]

        return [("[Raw Bone Torques] * 23", ContinuousActionSpace([self.__JOINTS_DOF], low=raw_min, high=raw_max)),
                ("[-1 to 1] * 23, where 1 is the maximum torque for the given index"
                 "joint (based on mass of bone)",
                 ContinuousActionSpace([self.__JOINTS_DOF], low=joint_min, high=joint_max)),
                ("[-1 to 1] * 23, scaled torques, then [x, y, z] transform",
                 ContinuousActionSpace([self.__JOINTS_AND_DIST], low=scaled_min, high=scaled_max))]

    def __repr__(self):
        return "HandAgent " + self.name

    @staticmethod
    def joint_ind(joint_name):
        """Gets the joint indices for a given name

        Args:
            joint_name (:obj:`str`): Name of the joint to look up

        Returns:
            (int): The index into the state array
        """
        return HandAgent._joint_indices[joint_name]

    def get_joint_constraints(self, joint_name):
        if joint_name in joint_constraints.hand_agent_joints_constraints:
            return joint_constraints.hand_agent_joints_constraints[joint_name]
        return None

    _joint_indices = {
        # Head, Spine, and Arm joints. Each has[swing1, swing2, twist]
        "hand_r": 0,

        # First joint of each finger. Has only [swing1, swing2]
        "thumb_01_r": 3,
        "index_01_r": 5,
        "middle_01_r": 7,
        "ring_01_r": 9,
        "pinky_01_r": 11,

        # Second joint of each finger. Has only[swing1]
        "thumb_02_r": 12,
        "index_02_r": 13,
        "middle_02_r": 14,
        "ring_02_r": 15,
        "pinky_02_r": 16,

        # Third joint of each finger. Has only[swing1]
        "thumb_03_r": 17,
        "index_03_r": 18,
        "middle_03_r": 19,
        "ring_03_r": 20,
        "pinky_03_r": 21
    }


class NavAgent(HoloOceanAgent):
    """A humanoid character capable of intelligent navigation.

       **Action Space:**

       Continuous control scheme of the form ``[x_target, y_target, z_target]``. 
       (see :ref:`coordinate-system`)

       Inherits from :class:`HoloOceanAgent`.
       
    """

    # constants in NavAgent.h in holoocean-engine
    __MAX_DISTANCE = 0.5
    __MIN_DISTANCE = -__MAX_DISTANCE

    agent_type = "NavAgent"

    @property
    def control_schemes(self):
        low = [self.__MIN_DISTANCE for _ in range(3)]
        high = [self.__MAX_DISTANCE for _ in range(3)]
        return [("[x_target, y_target, z_target]", ContinuousActionSpace([3], low=low, high=high))]

    def get_joint_constraints(self, joint_name):
        return None

    def __repr__(self):
        return "NavAgent " + self.name

    def __act__(self, action):
        np.copyto(self._action_buffer, np.array(action))


class TurtleAgent(HoloOceanAgent):
    """A simple turtle bot.

    **Action Space**:

    ``[forward_force, rot_force]``
    
    - ``forward_force`` is capped at 160 in either direction
    - ``rot_force`` is capped at 35 either direction

    Inherits from :class:`HoloOceanAgent`."""
    # constants in TurtleAgent.h in holoocean-engine
    __MAX_THRUST = 160.0
    __MIN_THRUST = -__MAX_THRUST

    __MAX_YAW = 35.0
    __MIN_YAW = -__MAX_YAW

    agent_type = "TurtleAgent"

    @property
    def control_schemes(self):
        low = [self.__MIN_THRUST, self.__MIN_YAW]
        high = [self.__MAX_THRUST, self.__MAX_YAW]
        return [("[forward_force, rot_force]", ContinuousActionSpace([2], low=low, high=high))]

    def get_joint_constraints(self, joint_name):
        return None

    def __repr__(self):
        return "TurtleAgent " + self.name

    def __act__(self, action):
        np.copyto(self._action_buffer, np.array(action))
        np.copyto(self._action_buffer, action)


class HoveringAUV(HoloOceanAgent):
    """A simple autonomous underwater vehicle. All variables are not actually used in simulation,
    modifying them will have no effect on results. They are exposed for convenience in implementing custom
    dynamics.

    **Action Space**

    Has three possible control schemes, as follows


    #. Thruster Forces: ``[Vertical Front Starboard, Vertical Front Port, Vertical Back Port, Vertical Back Starboard, Angled Front Starboard, Angled Front Port, Angled Back Port, Angled Back Starboard]``

    #. PD Controller: ``[des_pos_x, des_pos_y, des_pos_z, roll, pitch, yaw]``

    #. Accelerations, in global frame: ``[lin_accel_x, lin_accel_y, lin_accel_z, ang_accel_x, ang_accel_y, ang_accel_x]``

    Inherits from :class:`HoloOceanAgent`.
    
        
    :cvar mass: (:obj:`float`): Mass of the vehicle in kg.
    :cvar water_density: (:obj:`float`): Water density in kg / m^3.
    :cvar volume: (:obj:`float`): Volume of vehicle in m^3.
    :cvar cob: (:obj:`np.ndarray`): 3-vecter Center of buoyancy from the center of mass in m.
    :cvar I: (:obj:`np.ndarray`): 3x3 Inertia matrix.
    :cvar thruster_d: (:obj:`np.ndarray`): 8x3 matrix of unit vectors in the direction of thruster propulsion
    :cvar thruster_p: (:obj:`np.ndarray`): 8x3 matrix of positions in local frame of thrusters positions in m."""
    # constants in HoveringAUV.h in holoocean-engine
    __MAX_LIN_ACCEL = 10
    __MAX_ANG_ACCEL = 2
    __MAX_THRUST = __MAX_LIN_ACCEL*31.02/4

    agent_type = "HoveringAUV"

    mass = 31.02
    water_density = 997
    volume = mass / water_density
    cob = np.array([0,0,.05])
    I = np.eye(3)

    thruster_d = np.array([[0, 0, 1],
                    [0, 0, 1],
                    [0, 0, 1],
                    [0, 0, 1],
                    [1/np.sqrt(2), 1/np.sqrt(2), 0],
                    [1/np.sqrt(2), -1/np.sqrt(2), 0],
                    [1/np.sqrt(2), 1/np.sqrt(2), 0],
                    [1/np.sqrt(2), -1/np.sqrt(2), 0]])

    thruster_p = np.array([[0.25, -0.22, -0.04],
                            [0.25, 0.22, -0.04],
                            [-0.25, 0.22, -0.04],
                            [-0.25, -0.22, -0.04],
                            [0.14, -0.18, 0],
                            [0.14, 0.18, 0],
                            [-0.14, 0.18, 0],
                            [-0.14, -0.18, 0]])

    @property
    def control_schemes(self):
        scheme_thrusters = "[Vertical Front Starboard, Vertical Front Port, Vertical Back Port, Vertical Back Starboard, Angled Front Starboard, Angled Front Port, Angled Back Port, Angled Back Starboard]"
        
        scheme_accel = "[lin_accel_x, lin_accel_y, lin_accel_z, ang_accel_x, ang_accel_y, ang_accel_x]"
        limits_accel = [self.__MAX_LIN_ACCEL, self.__MAX_LIN_ACCEL, self.__MAX_LIN_ACCEL, self.__MAX_ANG_ACCEL, self.__MAX_ANG_ACCEL, self.__MAX_ANG_ACCEL]
        
        scheme_control = "[des_x, des_y, des_z, des_roll, des_pitch, des_yaw]"
        limits_control = [np.NaN, np.NaN, np.NaN, 180, 90, 180]
        
        return [(scheme_thrusters, ContinuousActionSpace([8], low=[-self.__MAX_THRUST]*8, high=[self.__MAX_THRUST]*8)),
                (scheme_accel, ContinuousActionSpace([6], low=[-i for i in limits_accel], high=limits_accel)),
                (scheme_control, ContinuousActionSpace([6], low=[-i for i in limits_control], high=limits_control))]

    def get_joint_constraints(self, joint_name):
        return None

    def __repr__(self):
        return "HoveringAUV " + self.name
        

class SurfaceVessel(HoloOceanAgent):
    """A simple surface vessel. All variables are not actually used in simulation,
    modifying them will have no effect on results. They are exposed for convenience in implementing custom
    dynamics.

    **Action Space**

    Has three possible control schemes, as follows


    #. Thruster Forces: ``[Left thruster, Right thruster]``

    #. PD Controller: ``[des_x, des_y, des_yaw]``

    #. Accelerations, in global frame: ``[lin_accel_x, lin_accel_y, lin_accel_z, ang_accel_x, ang_accel_y, ang_accel_x]``

    Inherits from :class:`HoloOceanAgent`.
    
        
    :cvar mass: (:obj:`float`): Mass of the vehicle in kg.
    :cvar water_density: (:obj:`float`): Water density in kg / m^3.
    :cvar volume: (:obj:`float`): Volume of vehicle in m^3.
    :cvar cob: (:obj:`np.ndarray`): 3-vecter Center of buoyancy from the center of mass in m.
    :cvar I: (:obj:`np.ndarray`): 3x3 Inertia matrix.
    :cvar thruster_p: (:obj:`np.ndarray`): 2x3 matrix of positions in local frame of thrusters positions in m."""
    # constants in SurfaceVessel.h in holoocean-engine
    __MAX_LIN_ACCEL = 20
    __MAX_ANG_ACCEL = 2
    __MAX_THRUST = 1500

    agent_type = "SurfaceVessel"

    mass = 200
    water_density = 997
    volume = 6 * mass / water_density
    cob = np.array([0,0,.1])
    I = np.diag([2,2,1])

    thruster_p = np.array([[-250, -100, -0], [-250, 100, -0]]) / 100

    @property
    def control_schemes(self):
        scheme_thrusters = "[Left thruster, Right thruster]"
        
        scheme_accel = "[lin_accel_x, lin_accel_y, lin_accel_z, ang_accel_x, ang_accel_y, ang_accel_x]"
        limits_accel = [self.__MAX_LIN_ACCEL, self.__MAX_LIN_ACCEL, self.__MAX_LIN_ACCEL, self.__MAX_ANG_ACCEL, self.__MAX_ANG_ACCEL, self.__MAX_ANG_ACCEL]
        
        scheme_control = "[des_x, des_y]"
        limits_control = [np.NaN, np.NaN]
        
        return [(scheme_thrusters, ContinuousActionSpace([2], low=[-self.__MAX_THRUST]*8, high=[self.__MAX_THRUST]*8)),
                (scheme_accel, ContinuousActionSpace([6], low=[-i for i in limits_accel], high=limits_accel)),
                (scheme_control, ContinuousActionSpace([2], low=[-i for i in limits_control], high=limits_control))]

    def get_joint_constraints(self, joint_name):
        return None

    def __repr__(self):
        return "SurfaceVessel " + self.name


class TorpedoAUV(HoloOceanAgent):
    """A simple foward motion autonomous underwater vehicle. All variables are not actually used in simulation,
    modifying them will have no effect on results. They are exposed for convenience in implementing custom
    dynamics.

    **Action Space**

    Has two possible action spaces, as follows:

    #. Fins & Propeller: ``[left_fin, top_fin, right_fin, bottom_fin, thrust]``

    #. Accelerations, in global frame: ``[lin_accel_x, lin_accel_y, lin_accel_z, ang_accel_x, ang_accel_y, ang_accel_x]``

    Inherits from :class:`HoloOceanAgent`.
    
    :cvar mass: (:obj:`float`): Mass of the vehicle in kg.
    :cvar water_density: (:obj:`float`): Water density in kg / m^3.
    :cvar volume: (:obj:`float`): Volume of vehicle in m^3.
    :cvar cob: (:obj:`np.ndarray`): 3-vecter Center of buoyancy from the center of mass in m.
    :cvar I: (:obj:`np.ndarray`): 3x3 Inertia matrix.
    :cvar thruster_p: (:obj:`np.ndarray`): 3 matrix of positions in local frame of propeller position in m.
    :cvar fin_p: (:obj:`np.ndarray`): 4x3 matrix of positions in local frame of fin positions in m."""
    # constants in TorpedoAUV.h in holoocean-engine
    __MAX_THRUST = 100
    __MAX_FIN = 45
    __MAX_LIN_ACCEL = 10
    __MAX_ANG_ACCEL = 2

    agent_type = "TorpedoAUV"

    mass = 36
    water_density = 997
    volume = mass / water_density
    cob = np.array([0,0,.07])
    I = np.diag([2, 1.2, 1.2])

    thruster_p = np.array([-120, 0, 0]) / 100

    fin_p = np.array(  [[-105,-7.07,    0],
                        [-105,    0, 7.07],
                        [-105, 7.07,    0],
                        [-105,    0,-7.07]]) / 100

    @property
    def control_schemes(self):
        scheme_fins = "[right_fin, top_fin, left_fin, bottom_fin, thrust]"
        limits_fins = [self.__MAX_FIN]*4 + [self.__MAX_THRUST]

        scheme_accel = "[lin_accel_x, lin_accel_y, lin_accel_z, ang_accel_x, ang_accel_y, ang_accel_x]"
        limits_accel = [self.__MAX_LIN_ACCEL, self.__MAX_LIN_ACCEL, self.__MAX_LIN_ACCEL, self.__MAX_ANG_ACCEL, self.__MAX_ANG_ACCEL, self.__MAX_ANG_ACCEL]
        
        return [(scheme_fins, ContinuousActionSpace([5], low=[-i for i in limits_fins], high=limits_fins)),
                (scheme_accel, ContinuousActionSpace([6], low=[-i for i in limits_accel], high=limits_accel))]

    def get_joint_constraints(self, joint_name):
        return None

    def __repr__(self):
        return "TorpedoAUV " + self.name


class AgentDefinition:
    """Represents information needed to initialize agent.

    Args:
        agent_name (:obj:`str`): The name of the agent to control.
        agent_type (:obj:`str` or type): The type of HoloOceanAgent to control, string or class
            reference.
        sensors (:class:`~holoocean.sensors.SensorDefinition` or class type (if no duplicate sensors)): A list of
            HoloOceanSensors to read from this agent.
        starting_loc (:obj:`list` of :obj:`float`): Starting ``[x, y, z]`` location for agent 
            (see :ref:`coordinate-system`)
        starting_rot (:obj:`list` of :obj:`float`): Starting ``[roll, pitch, yaw]`` rotation for agent 
            (see :ref:`rotations`)
        existing (:obj:`bool`): If the agent exists in the world or not (deprecated)
    """

    _type_keys = {
        "SphereAgent": SphereAgent,
        "UavAgent": UavAgent,
        "NavAgent": NavAgent,
        "AndroidAgent": AndroidAgent,
        "HandAgent": HandAgent,
        "TurtleAgent": TurtleAgent,
        "HoveringAUV": HoveringAUV,
        "TorpedoAUV": TorpedoAUV,
        "SurfaceVessel": SurfaceVessel,
    }

    def __init__(self, agent_name, agent_type, sensors=None, starting_loc=(0, 0, 0),
                 starting_rot=(0, 0, 0), existing=False, is_main_agent=False):
        self.starting_loc = starting_loc
        self.starting_rot = starting_rot
        self.existing = existing
        self.sensors = sensors or list()
        self.is_main_agent = is_main_agent
        for i, sensor_def in enumerate(self.sensors):
            if not isinstance(sensor_def, SensorDefinition):
                self.sensors[i] = \
                    SensorDefinition(agent_name, agent_type,
                                     sensor_def.sensor_type, sensor_def)
        self.name = agent_name

        if isinstance(agent_type, str):
            self.type = AgentDefinition._type_keys[agent_type]
        else:
            self.type = agent_type


class AgentFactory:
    """Creates an agent object
    """

    @staticmethod
    def build_agent(client, agent_def):
        """Constructs an agent

        Args:
            client (:class:`holoocean.holooceanclient.HoloOceanClient`): HoloOceanClient agent is
                associated with
            agent_def (:class:`AgentDefinition`): Definition of the agent to instantiate

        Returns:

        """
        return agent_def.type(client, agent_def.name)
