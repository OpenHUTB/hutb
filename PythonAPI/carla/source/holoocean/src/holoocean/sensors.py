"""Definition of all of the sensor information. The UE settings for sensors are such that the motion ticks happen before the sensor ticks."""
import json

import numpy as np
import holoocean

from holoocean.command import (
    RGBCameraRateCommand,
    RotateSensorCommand,
    CustomCommand,
    SendAcousticMessageCommand,
    SendOpticalMessageCommand,
)
from holoocean.exceptions import HoloOceanConfigurationException
from holoocean.lcm import SensorData


class HoloOceanSensor:
    """Base class for a sensor

    Args:
        client (:class:`~holoocean.holooceanclient.HoloOceanClient`): Client
            attached to a sensor
        agent_name (:obj:`str`): Name of the parent agent
        agent_type (:obj:`str`): Type of the parent agent
        name (:obj:`str`): Name of the sensor
        config (:obj:`dict`): Configuration dictionary to pass to the engine
    """

    default_config = {}

    def __init__(
        self,
        client,
        agent_name=None,
        agent_type=None,
        name="DefaultSensor",
        config=None,
    ):
        self.name = name
        self._client = client
        self.agent_name = agent_name
        self.agent_type = agent_type
        self._buffer_name = self.agent_name + "_" + self.name

        self._sensor_data_buffer = self._client.malloc(
            self._buffer_name + "_sensor_data", self.data_shape, self.dtype
        )

        self.config = {} if config is None else config

    @property
    def sensor_data(self):
        """Get the sensor data buffer

        Returns:
            :obj:`np.ndarray` of size :obj:`self.data_shape`: Current sensor data

        """
        if self.tick_count == self.tick_every:
            return self._sensor_data_buffer
        else:
            return None

    @property
    def dtype(self):
        """The type of data in the sensor

        Returns:
            numpy dtype: Type of sensor data
        """
        raise NotImplementedError("Child class must implement this property")

    @property
    def data_shape(self):
        """The shape of the sensor data

        Returns:
            :obj:`tuple`: Sensor data shape
        """
        raise NotImplementedError("Child class must implement this property")

    def rotate(self, rotation):
        """Rotate the sensor. It will be applied in approximately three ticks.
        :meth:`~holoocean.environments.HoloOceanEnvironment.step` or
        :meth:`~holoocean.environments.HoloOceanEnvironment.tick`.)

        This will not persist after a call to reset(). If you want a persistent rotation for a sensor,
        specify it in your scenario configuration.

        Args:
            rotation (:obj:`list` of :obj:`float`): rotation for sensor (see :ref:`rotations`).
        """
        command_to_send = RotateSensorCommand(self.agent_name, self.name, rotation)
        self._client.command_center.enqueue_command(command_to_send)

    def reset(self):
        pass


class DistanceTask(HoloOceanSensor):
    sensor_type = "DistanceTask"

    @property
    def dtype(self):
        return np.float32

    @property
    def data_shape(self):
        return [2]


class LocationTask(HoloOceanSensor):
    sensor_type = "LocationTask"

    @property
    def dtype(self):
        return np.float32

    @property
    def data_shape(self):
        return [2]


class FollowTask(HoloOceanSensor):
    sensor_type = "FollowTask"

    @property
    def dtype(self):
        return np.float32

    @property
    def data_shape(self):
        return [2]


class AvoidTask(HoloOceanSensor):
    sensor_type = "AvoidTask"

    @property
    def dtype(self):
        return np.float32

    @property
    def data_shape(self):
        return [2]


class CupGameTask(HoloOceanSensor):
    sensor_type = "CupGameTask"

    @property
    def dtype(self):
        return np.float32

    @property
    def data_shape(self):
        return [2]

    def start_game(self, num_shuffles, speed=3, seed=None):
        """Start the cup game and set its configuration. Do not call if the config file contains a cup task configuration
        block, as it will override the configuration and cause undefined behavior.

        Args:
            num_shuffles (:obj:`int`): Number of shuffles
            speed (:obj:`int`): Speed of the shuffle. Works best between 1-10
            seed (:obj:`int`): Seed to rotate the cups the same way every time. If none is given, a seed will not be used.
        """
        use_seed = seed is not None
        if seed is None:
            seed = 0  # have to pass a value
        config_command = CustomCommand(
            "CupGameConfig", num_params=[speed, num_shuffles, int(use_seed), seed]
        )
        start_command = CustomCommand("StartCupGame")
        self._client.command_center.enqueue_command(config_command)
        self._client.command_center.enqueue_command(start_command)


class CleanUpTask(HoloOceanSensor):
    sensor_type = "CleanUpTask"

    @property
    def dtype(self):
        return np.float32

    @property
    def data_shape(self):
        return [2]

    def start_task(self, num_trash, use_table=False):
        """Spawn trash around the trash can. Do not call if the config file contains a clean up task configuration
        block.

        Args:
            num_trash (:obj:`int`): Amount of trash to spawn
            use_table (:obj:`bool`, optional): If True a table will spawn next to the trash can, all trash will be on
                the table, and the trash can lid will be absent. This makes the task significantly easier. If False,
                all trash will spawn on the ground. Defaults to False.
        """

        if self.config is not None or self.config is not {}:
            raise HoloOceanConfigurationException(
                "Called CleanUpTask start_task when configuration block already \
                specified. Must remove configuration block before calling."
            )

        config_command = CustomCommand(
            "CleanUpConfig", num_params=[num_trash, int(use_table)]
        )
        self._client.command_center.enqueue_command(config_command)


class ViewportCapture(HoloOceanSensor):
    """Captures what the viewport is seeing.

    The ViewportCapture is faster than the RGB camera, but there can only be one camera
    and it must capture what the viewport is capturing. If performance is
    critical, consider this camera instead of the RGBCamera.

    It may be useful
    to position the camera with
    :meth:`~holoocean.environments.HoloOceanEnvironment.teleport_camera`.

    **Configuration**

    The ``configuration`` block (see :ref:`configuration-block`) accepts the following
    options:

    - ``CaptureWidth``: Width of captured image
    - ``CaptureHeight``: Height of captured image

    **THESE DIMENSIONS MUST MATCH THE VIEWPORT DIMENSTIONS**

    If you have configured the size of the  viewport (``window_height/width``), you must
    make sure that ``CaptureWidth/Height`` of this configuration block is set to the same
    dimensions.

    The default resolution is ``1280x720``, matching the default Viewport resolution.
    """

    sensor_type = "ViewportCapture"

    def __init__(
        self, client, agent_name, agent_type, name="ViewportCapture", config=None
    ):
        self.config = {} if config is None else config

        width = 1280
        height = 720

        if "CaptureHeight" in self.config:
            height = self.config["CaptureHeight"]

        if "CaptureWidth" in self.config:
            width = self.config["CaptureWidth"]

        self.shape = (height, width, 4)

        super(ViewportCapture, self).__init__(
            client, agent_name, agent_type, name=name, config=config
        )

    @property
    def dtype(self):
        return np.uint8

    @property
    def data_shape(self):
        return self.shape


class RGBCamera(HoloOceanSensor):
    """Captures agent's view.

    The default capture resolution is 256x256x256x4, corresponding to the RGBA channels.
    The resolution can be increased, but will significantly impact performance.

    **Configuration**

    The ``configuration`` block (see :ref:`configuration-block`) accepts the following
    options:

    - ``CaptureWidth``: Width of captured image
    - ``CaptureHeight``: Height of captured image

    """

    sensor_type = "RGBCamera"

    def __init__(self, client, agent_name, agent_type, name="RGBCamera", config=None):
        self.config = {} if config is None else config

        width = 256
        height = 256

        if "CaptureHeight" in self.config:
            height = self.config["CaptureHeight"]

        if "CaptureWidth" in self.config:
            width = self.config["CaptureWidth"]

        self.shape = (height, width, 4)

        super(RGBCamera, self).__init__(
            client, agent_name, agent_type, name=name, config=config
        )

    @property
    def dtype(self):
        return np.uint8

    @property
    def data_shape(self):
        return self.shape

    def set_ticks_per_capture(self, ticks_per_capture):
        """Sets this RGBCamera to capture a new frame every ticks_per_capture.

        The sensor's image will remain unchanged between captures.

        This method must be called after every call to env.reset.

        Args:
            ticks_per_capture (:obj:`int`): The amount of ticks to wait between camera captures.
        """
        if not isinstance(ticks_per_capture, int) or ticks_per_capture < 1:
            raise HoloOceanConfigurationException(
                "Invalid ticks_per_capture value " + str(ticks_per_capture)
            )

        command_to_send = RGBCameraRateCommand(
            self.agent_name, self.name, ticks_per_capture
        )
        self._client.command_center.enqueue_command(command_to_send)
        self.tick_every = ticks_per_capture


class OrientationSensor(HoloOceanSensor):
    """Gets the forward, right, and up vector for the agent.
    Note that this is based on the sensor's frame, not the agent's frame so in the IMU socket, 
    it will be NED, and in COM socket it will NWU. Our provided configurations have the sensor in the IMU socket.
    
    Returns a 2D numpy array of

    ::

       [ [forward_x, right_x, up_x],
         [forward_y, right_y, up_y],
         [forward_z, right_z, up_z] ]

    """

    sensor_type = "OrientationSensor"

    @property
    def dtype(self):
        return np.float32

    @property
    def data_shape(self):
        return [3, 3]


class IMUSensor(HoloOceanSensor):
    """Inertial Measurement Unit sensor.

    Returns a 2D numpy array of::

       [ [accel_x, accel_y, accel_z],
         [ang_vel_roll,  ang_vel_pitch, ang_vel_yaw],
         [accel_bias_x, accel_bias_y, accel_bias_z],
         [ang_vel_bias_roll,  ang_vel_bias_pitch, ang_vel_bias_yaw]    ]


    where the accleration components are in m/s and the angular velocity is in rad/s.

    **Configuration**

    The ``configuration`` block (see :ref:`configuration-block`) accepts the
    following options:

    - ``AccelSigma``/``AccelCov``: Covariance/Std for acceleration component. Can be scalar, 3-vector or 3x3-matrix. Set one or the other. Defaults to 0 => no noise.
    - ``AngVelSigma``/``AngVelCov``: Covariance/Std for angular velocity component. Can be scalar, 3-vector or 3x3-matrix. Set one or the other. Defaults to 0 => no noise.
    - ``AccelBiasSigma``/``AccelCBiasov``: Covariance/Std for acceleration bias component. Can be scalar, 3-vector or 3x3-matrix. Set one or the other. Defaults to 0 => no noise.
    - ``AngVelBiasSigma``/``AngVelBiasCov``: Covariance/Std for acceleration bias component. Can be scalar, 3-vector or 3x3-matrix. Set one or the other. Defaults to 0 => no noise.
    - ``ReturnBias``: Whether the sensor should return the bias along with accel/ang. vel. Defaults to false.

    """

    sensor_type = "IMUSensor"

    def __init__(self, client, agent_name, agent_type, name="IMUSensor", config=None):
        self.config = {} if config is None else config

        return_bias = self.config.get("ReturnBias", False)

        if "AccelSigma" in self.config and "AccelCov" in self.config:
            raise ValueError(
                "Can't set both AccelSigma and AccelCov in IMUSensor, use one of them in your configuration"
            )

        if "AngVelSigma" in self.config and "AngVelCov" in self.config:
            raise ValueError(
                "Can't set both AngVelSigma and AngVelCov in IMUSensor, use one of them in your configuration"
            )

        if "AccelBiasSigma" in self.config and "AccelBiasCov" in self.config:
            raise ValueError(
                "Can't set both AccelBiasSigma and AccelBiasCov in IMUSensor, use one of them in your configuration"
            )

        if "AngVelBiasSigma" in self.config and "AngVelBiasCov" in self.config:
            raise ValueError(
                "Can't set both AngVelBiasSigma and AngVelBiasCov in IMUSensor, use one of them in your configuration"
            )

        self.shape = [4, 3] if return_bias else [2, 3]

        super(IMUSensor, self).__init__(
            client, agent_name, agent_type, name=name, config=config
        )

    @property
    def dtype(self):
        return np.float32

    @property
    def data_shape(self):
        return self.shape


class JointRotationSensor(HoloOceanSensor):
    """Returns the state of the :class:`~holoocean.agents.AndroidAgent`'s or the
    :class:`~holoocean.agents.HandAgent`'s joints.

    """

    sensor_type = "JointRotationSensor"

    def __init__(self, client, agent_name, agent_type, name="RGBCamera", config=None):
        if holoocean.agents.AndroidAgent.agent_type in agent_type:
            # Should match AAndroid::TOTAL_DOF
            self.elements = 94
        elif agent_type == holoocean.agents.HandAgent.agent_type:
            # AHandAgent::TOTAL_JOINT_DOF
            self.elements = 23
        else:
            raise HoloOceanConfigurationException(
                "Attempting to use JointRotationSensor with unsupported"
                "agent type '{}'!".format(agent_type)
            )

        super(JointRotationSensor, self).__init__(
            client, agent_name, agent_type, name, config
        )

    @property
    def dtype(self):
        return np.float32

    @property
    def data_shape(self):
        return [self.elements]


class PressureSensor(HoloOceanSensor):
    """For each joint on the :class:`~holoocean.agents.AndroidAgent` or the
    :class:`~holoocean.agents.HandAgent`, returns the pressure on the
    joint.

    For each joint, returns ``[x_loc, y_loc, z_loc, force]``.

    """

    sensor_type = "PressureSensor"

    def __init__(self, client, agent_name, agent_type, name="RGBCamera", config=None):
        if holoocean.agents.AndroidAgent.agent_type in agent_type:
            # Should match AAndroid::NUM_JOINTS
            self.elements = 48
        elif agent_type == holoocean.agents.HandAgent.agent_type:
            # AHandAgent::NUM_JOINTS
            self.elements = 16
        else:
            raise HoloOceanConfigurationException(
                "Attempting to use PressureSensor with unsupported"
                "agent type '{}'!".format(agent_type)
            )

        super(PressureSensor, self).__init__(
            client, agent_name, agent_type, name, config
        )

    @property
    def dtype(self):
        return np.float32

    @property
    def data_shape(self):
        return [self.elements * (3 + 1)]


class RelativeSkeletalPositionSensor(HoloOceanSensor):
    """Gets the position of each bone in a skeletal mesh as a quaternion.

    Returns a numpy array with four entries for each bone.
    """

    def __init__(self, client, agent_name, agent_type, name="RGBCamera", config=None):
        if holoocean.agents.AndroidAgent.agent_type in agent_type:
            # Should match AAndroid::NumBones
            self.elements = 60
        elif agent_type == holoocean.agents.HandAgent.agent_type:
            # AHandAgent::NumBones
            self.elements = 17
        else:
            raise HoloOceanConfigurationException(
                "Attempting to use RelativeSkeletalPositionSensor with unsupported"
                "agent type {}!".format(agent_type)
            )
        super(RelativeSkeletalPositionSensor, self).__init__(
            client, agent_name, agent_type, name, config
        )

    sensor_type = "RelativeSkeletalPositionSensor"

    @property
    def dtype(self):
        return np.float32

    @property
    def data_shape(self):
        return [self.elements, 4]


class LocationSensor(HoloOceanSensor):
    """Gets the location of the agent in the world.

    Returns coordinates in ``[x, y, z]`` format (see :ref:`coordinate-system`)

    **Configuration**

    The ``configuration`` block (see :ref:`configuration-block`) accepts the
    following options:

    - ``Sigma``/``Cov``: Covariance/Std. Can be scalar, 3-vector or 3x3-matrix. Set one or the other. Defaults to 0 => no noise.

    """

    sensor_type = "LocationSensor"

    def __init__(
        self, client, agent_name, agent_type, name="LocationSensor", config=None
    ):
        self.config = {} if config is None else config

        if "Sigma" in self.config and "Cov" in self.config:
            raise ValueError(
                "Can't set both Sigma and Cov in LocationSensor, use one of them in your configuration"
            )

        super(LocationSensor, self).__init__(
            client, agent_name, agent_type, name=name, config=config
        )

    @property
    def dtype(self):
        return np.float32

    @property
    def data_shape(self):
        return [3]


class RotationSensor(HoloOceanSensor):
    """Gets the rotation of the agent in the world, with rotation XYZ about the fixed frame, in degrees.

    Returns ``[roll, pitch, yaw]`` (see :ref:`rotations`)
    """

    sensor_type = "RotationSensor"

    @property
    def dtype(self):
        return np.float32

    @property
    def data_shape(self):
        return [3]


class VelocitySensor(HoloOceanSensor):
    """Returns the x, y, and z velocity of the sensor in the global frame."""

    sensor_type = "VelocitySensor"

    @property
    def dtype(self):
        return np.float32

    @property
    def data_shape(self):
        return [3]


class DynamicsSensor(HoloOceanSensor):
    """Gives all relevant information needed for implementing custom dynamics. Returns are all in the global frame and are as follows:

       [acceleration, velocity, position, angular accel., angular velocity, rpy]

    all of which are 3-vectors and rpy is roll, pitch, and yaw. This is the ONLY sensor that by default
    doesn't operate in its socket, but rather in the COM. This is mostly for convenience since 99% of the
    time when doing custom dynamics the information is wanted at the COM.

    **Configuration**

    The ``configuration`` block (see :ref:`configuration-block`) accepts the
    following options:

    - ``UseCOM``: Whether to return data relative to the COM or the specified socket. Defaults to true.
    - ``UseRPY``: Whether to return orientation as roll, pitch, yaw, or as a quaternion. Defaults to true. When true, sensor returns an 18 vector, when false sensor returns as 19 vector. Quaternion is specified with scalar as last value ie x,y,z,w.
    """

    sensor_type = "DynamicsSensor"

    def __init__(
        self, client, agent_name, agent_type, name="DynamicsSensor", config=None
    ):
        self.config = {} if config is None else config

        use_rpy = self.config.get("UseRPY", True)

        self.shape = [18] if use_rpy else [19]

        super(DynamicsSensor, self).__init__(
            client, agent_name, agent_type, name=name, config=config
        )

    @property
    def dtype(self):
        return np.float32

    @property
    def data_shape(self):
        return self.shape


class CollisionSensor(HoloOceanSensor):
    """Returns true if the agent is colliding with anything (including the ground)."""

    sensor_type = "CollisionSensor"

    @property
    def dtype(self):
        return np.bool_

    @property
    def data_shape(self):
        return [1]


class RangeFinderSensor(HoloOceanSensor):
    """Returns distances to nearest collisions in the directions specified by
    the parameters. For example, if an agent had two range sensors at different
    angles with 24 lasers each, the LaserDebug traces would look something like
    this:

    .. image:: ../../docs/images/UAVRangeFinder.PNG
    
    That is, for 1 laser, you'd have 1 laser facing forward, for 3, you'd have one forward,
    with the other 2 distributed evenly along the circle, at 120 degree intervals, and for
    24, you'd have a laser spaced every 15 degrees.
    

    **Configuration**

    The ``configuration`` block (see :ref:`configuration-block`) accepts the
    following options:

    - ``LaserMaxDistance``: Max Distance in meters of RangeFinder. (default 10)
    - ``LaserCount``: Number of lasers in sensor. Lasers are distributed evenly along the cone defined by laser angle. (default 1)
    - ``LaserAngle``: Angle of elevation of lasers from origin. Measured in degrees. Straight up would be 90 degrees, down would be -90. (default 0)
    - ``LaserDebug``: Show debug traces. (default false)
    """

    sensor_type = "RangeFinderSensor"

    def __init__(
        self, client, agent_name, agent_type, name="RangeFinderSensor", config=None
    ):
        config = {} if config is None else config
        self.laser_count = config["LaserCount"] if "LaserCount" in config else 1

        super().__init__(client, agent_name, agent_type, name, config)

    @property
    def dtype(self):
        return np.float32

    @property
    def data_shape(self):
        return [self.laser_count]


class WorldNumSensor(HoloOceanSensor):
    """Returns any numeric value from the world corresponding to a given key. This is
    world specific.

    """

    sensor_type = "WorldNumSensor"

    @property
    def dtype(self):
        return np.float32

    @property
    def data_shape(self):
        return [1]


class BallLocationSensor(WorldNumSensor):
    """For the CupGame task, returns which cup the ball is underneath.

    The cups are numbered 0-2, from the agents perspective, left to right. As soon
    as a swap begins, the number returned by this sensor is updated to the balls new
    position after the swap ends.

    Only works in the CupGame world.

    """

    default_config = {"Key": "BallLocation"}

    @property
    def dtype(self):
        return np.int8


class AbuseSensor(HoloOceanSensor):
    """Returns True if the agent has been abused. Abuse is calculated differently for
    different agents. The Sphere and Hand agent cannot be abused. The Uav, Android,
    and Turtle agents can be abused by experiencing high levels of acceleration.
    The Uav is abused when its blades collide with another object, and the Turtle
    agent is abused when it's flipped over.

    **Configuration**

    - ``AccelerationLimit``: Maximum acceleration the agent can endure before
      being considered abused. The default depends on the agent, usually around 300 m/s^2.

    """

    sensor_type = "AbuseSensor"

    @property
    def dtype(self):
        return np.int8

    @property
    def data_shape(self):
        return [1]


######################## HOLOOCEAN CUSTOM SENSORS ###########################
# Make sure to also add your new sensor to SensorDefinition below


class SidescanSonar(HoloOceanSensor):
    """Simulates a sidescan sonar. See :ref:`configure-octree` for more on
    how to configure the octree that is used.

    The ``configuration`` block (see :ref:`configuration-block`) accepts any of
    the options in the following sections.

    **Basic Configuration**

    - ``Azimuth``: Azimuth (side to side) angle visible in degrees, defaults to 170.
    - ``Elevation``: Elevation angle (up and down) visible in degrees, defaults to 0.25.
    - ``RangeMin``: Minimum range visible in meters, defaults to 0.5.
    - ``RangeMax``: Maximum range visible in meters, defaults to 35.
    - ``RangeBins``/``RangeRes``: Number of range bins of resulting image, or resolution (length in meters) of each bin. Set one or the other. Defaults to 0.05 m.

    **Noise Configuration**

    - ``AddSigma``/``AddCov``: Additive noise std/covariance from a Rayleigh distribution. Needs to be a float. Set one or the other. Defaults to 0, or off.
    - ``MultSigma``/``MultCov``: Multiplication noise std/covariance from a normal distribution. Needs to be a float. Set one or the other. Defaults to 0, or off.

    **Advanced Configuration**

    - ``ShowWarning``: Whether to show on screen warning about sonar computation happening. Defaults to True.
    - ``AzimuthBins``/``AzimuthRes``: Number of azimuth bins of resulting image, or resolution (length in degrees) of each bin. Set one or the other. By default this is computed based on the OctreeMin.
    - ``ElevationBins``/``ElevationRes``: Number of elevation bins used when shadowing is done, or resolution (length in degrees) of each bin. Set one or the other. By default this is computed based on the octree size and the min range. Should only be set if shadowing isn't working.
    - ``InitOctreeRange``: Upon startup, all mid-level octrees within this distance of the agent will be created.
    - ``ViewRegion``: Turns on green lines to see visible region. Defaults to False.
    - ``ViewOctree``: What octree leaves to show. Less than -1 means none, -1 means all, and anything greater than or equal to 0 shows the corresponding beam index. Defaults to -10.
    - ``ShadowEpsilon``: What constitutes a break between clusters when shadowing. Defaults to 4*OctreeMin.
    - ``WaterDensity``: Density of water in kg/m^3. Defaults to 997.
    - ``WaterSpeedSound``: Speed of sound in water in m/s. Defaults to 1480.
    - ``UseApprox``: Whether to use the faster approximation of Atan2 or the slower exact implementation. Defaults to True.

    """

    sensor_type = "SidescanSonar"

    def __init__(
        self, client, agent_name, agent_type, name="SidescanSonar", config=None
    ):
        self.config = {} if config is None else config

        range_min = self.config.get("RangeMin", 0.5)
        range_max = self.config.get("RangeMax", 35)
        range_res = 0.05

        if "RangeBins" in self.config and "RangeRes" in self.config:
            raise ValueError(
                "Can't set both RangeBins and RangeRes in SidescanSonar, use one of them in your configuration"
            )
        elif "RangeBins" in self.config:
            range_bins = self.config["RangeBins"]
        elif "RangeRes" in self.config:
            range_bins = int((range_max - range_min) // self.config["RangeRes"])
        else:
            range_bins = int((range_max - range_min) // range_res)

        if "AzimuthBins" in self.config and "AzimuthRes" in self.config:
            raise ValueError(
                "Can't set both AzimuthBins and AzimuthRes in SidescanSonar, use one of them in your configuration"
            )

        if "ElevationBins" in self.config and "ElevationRes" in self.config:
            raise ValueError(
                "Can't set both ElevationBins and ElevationRes in SidescanSonar, use one of them in your configuration"
            )

        if "AddSigma" in self.config and "AddCov" in self.config:
            raise ValueError(
                "Can't set both AddSigma and AddCov in SidescanSonar, use one of them in your configuration"
            )

        if "MultSigma" in self.config and "MultCov" in self.config:
            raise ValueError(
                "Can't set both MultSigma and MultCov in SidescanSonar, use one of them in your configuration"
            )

        # Ensure shape of python variable matches what will be sent from the c++ side
        self.shape = [range_bins]

        super(SidescanSonar, self).__init__(
            client, agent_name, agent_type, name=name, config=config
        )

    @property
    def dtype(self):
        return np.float32

    @property
    def data_shape(self):
        return self.shape


class ImagingSonar(HoloOceanSensor):
    """Simulates an imaging sonar. See :ref:`configure-octree` for more on
    how to configure the octree that is used.

    The ``configuration`` block (see :ref:`configuration-block`) accepts any of
    the options in the following sections.

    **Basic Configuration**

    - ``Azimuth``: Azimuth (side to side) angle visible in degrees, defaults to 120.
    - ``Elevation``: Elevation angle (up and down) visible in degrees, defaults to 20.
    - ``RangeMin``: Minimum range visible in meters, defaults to 0.1.
    - ``RangeMax``: Maximum range visible in meters, defaults to 10.
    - ``RangeBins``/``RangeRes``: Number of range bins of resulting image, or resolution (length in meters) of each bin. Set one or the other. Defaults to 512 bins.
    - ``AzimuthBins``/``AzimuthRes``: Number of azimuth bins of resulting image, or resolution (length in degrees) of each bin. Set one or the other. Defaults to 512 bins.

    **Noise Configuration**

    - ``AddSigma``/``AddCov``: Additive noise std/covariance from a Rayleigh distribution. Needs to be a float. Set one or the other. Defaults to 0, or off.
    - ``MultSigma``/``MultCov``: Multiplication noise std/covariance from a normal distribution. Needs to be a float. Set one or the other. Defaults to 0, or off.
    - ``MultiPath``: Whether to compute multipath or not. Defaults to False.
    - ``ClusterSize``: Size of cluster when multipath is enabled. Defaults to 5.
    - ``ScaleNoise``: Whether to scale the returned intensities or not. Defaults to False.
    - ``AzimuthStreaks``: What sort of azimuth artifacts to introduce. -1 is a removal artifact, 0 is no artifact, and 1 is increased gain artifact. Defaults to 0.
    - ``RangeSigma``: Additive noise std from an exponential distribution that will be added to the range measurements, and the intensities will be scaled by the pdf. Needs to be a float. Defaults to 0, or off.

    **Advanced Configuration**

    - ``ShowWarning``: Whether to show on screen warning about sonar computation happening. Defaults to True.
    - ``ElevationBins``/``ElevationRes``: Number of elevation bins used when shadowing is done, or resolution (length in degrees) of each bin. Set one or the other. By default this is computed based on the octree size and the min/max range. Should only be set if shadowing isn't working.
    - ``InitOctreeRange``: Upon startup, all mid-level octrees within this distance of the agent will be created.
    - ``ViewRegion``: Turns on green lines to see visible region. Defaults to False.
    - ``ViewOctree``: What octree leaves to show. Less than -1 means none, -1 means all, and anything greater than or equal to 0 shows the corresponding beam index. Defaults to -10.
    - ``ShadowEpsilon``: What constitutes a break between clusters when shadowing. Defaults to 4*OctreeMin.
    - ``WaterDensity``: Density of water in kg/m^3. Defaults to 997.
    - ``WaterSpeedSound``: Speed of sound in water in m/s. Defaults to 1480.
    - ``UseApprox``: Whether to use the faster approximation of Atan2 or the slower exact implementation. Defaults to True.

    """

    sensor_type = "ImagingSonar"

    def __init__(
        self, client, agent_name, agent_type, name="ImagingSonar", config=None
    ):
        self.config = {} if config is None else config

        b_range = 512
        b_azimuth = 512
        min_range = self.config.get("RangeMin", 0.1)
        max_range = self.config.get("RangeMax", 10)
        azimuth = self.config.get("Azimuth", 120)

        if "RangeBins" in self.config and "RangeRes" in self.config:
            raise ValueError(
                "Can't set both RangeBins and RangeRes in ImagingSonar, use one of them in your configuration"
            )
        elif "RangeBins" in self.config:
            b_range = self.config["RangeBins"]
        elif "RangeRes" in self.config:
            b_range = int((max_range - min_range) // self.config["RangeRes"])

        if "AzimuthBins" in self.config and "AzimuthRes" in self.config:
            raise ValueError(
                "Can't set both AzimuthBins and AzimuthRes in ImagingSonar, use one of them in your configuration"
            )
        elif "AzimuthBins" in self.config:
            b_azimuth = self.config["AzimuthBins"]
        elif "AzimuthRes" in self.config:
            b_azimuth = int(azimuth // self.config["AzimuthRes"])

        if "ElevationBins" in self.config and "ElevationRes" in self.config:
            raise ValueError(
                "Can't set both ElevationBins and ElevationRes in ImagingSonar, use one of them in your configuration"
            )

        if "AddSigma" in self.config and "AddCov" in self.config:
            raise ValueError(
                "Can't set both AddSigma and AddCov in ImagingSonar, use one of them in your configuration"
            )

        if "MultSigma" in self.config and "MultCov" in self.config:
            raise ValueError(
                "Can't set both MultSigma and MultCov in ImagingSonar, use one of them in your configuration"
            )

        self.shape = (b_range, b_azimuth)

        super(ImagingSonar, self).__init__(
            client, agent_name, agent_type, name=name, config=config
        )

    @property
    def dtype(self):
        return np.float32

    @property
    def data_shape(self):
        return self.shape


class SinglebeamSonar(HoloOceanSensor):
    """Simulates an echosounder, which is a sonar sensor with a single cone shaped beam. See :ref:`configure-octree` for more on
    how to configure the octree that is used.

    Returns a 1D numpy array of the average intensities held in each range bin of the sensor. The length of the array is specified by the number of range bins chosen for the sensor.

    **Configuration**

    The ``configuration`` block (see :ref:`configuration-block`) accepts the following
    options:

    The ``configuration`` block (see :ref:`configuration-block`) accepts any of
    the options in the following sections.

    **Basic Configuration**

    - ``OpeningAngle``: Opening angle of the cone visible in degrees, defaults to 30. In this documentation, the opening angle would be 2 times the semi-vertical angle of the cone.
    - ``RangeMin``: Minimum range visible in meters, defaults to 0.5.
    - ``RangeMax``: Maximum range visible in meters, defaults to 10.
    - ``RangeBins``/``RangeRes``: Number of range bins of resulting image, or resolution (length in meters) of each bin. Set one or the other. Defaults to 200 bins.

    **Noise Configuration**

    - ``AddSigma``/``AddCov``: Additive noise std/covariance from a Rayleigh distribution. Needs to be a float. Set one or the other. Defaults to 0, or off.
    - ``MultSigma``/``MultCov``: Multiplication noise std/covariance from a normal distribution. Needs to be a float. Set one or the other. Defaults to 0, or off.
    - ``RangeSigma``: Additive noise std from an exponential distribution that will be added to the range measurements. Needs to be a float. Defaults to 0/off.

    **Advanced Configuration**

    - ``ShowWarning``: Whether to show on screen warning about sonar computation happening. Defaults to True.
    - ``OpeningAngleBins``/``OpeningAngleRes``: Number of OpeningAngle bins used when shadowing is done, or resolution (length in degrees) of each bin. Set one or the other. By default this is computed based on the octree size and the min/max range. Should only be set if shadowing isn't working.
    - ``CentralAngleBins``/``CentralAngleRes``: Number of CentralAngle bins used when shadowing is done, or resolution (length in degrees) of each bin. Set one or the other. By default this is computed based on the octree size and the min/max range. Should only be set if shadowing isn't working.
    - ``InitOctreeRange``: Upon startup, all mid-level octrees within this distance of the agent will be created.
    - ``ViewRegion``: Turns on green lines to see visible region. Defaults to False.
    - ``ViewOctree``: What octree leaves to show. Less than -1 means none, -1 means all, and anything greater than or equal to 0 shows the corresponding beam index. Defaults to -10.
    - ``ShadowEpsilon``: What constitutes a break between clusters when shadowing. Defaults to 4*OctreeMin.
    - ``WaterDensity``: Density of water in kg/m^3. Defaults to 997.
    - ``WaterSpeedSound``: Speed of sound in water in m/s. Defaults to 1480.
    - ``UseApprox``: Whether to use the faster approximation of Atan2 or the slower exact implementation. Defaults to True.
    """

    sensor_type = "SinglebeamSonar"

    def __init__(
        self, client, agent_name, agent_type, name="SinglebeamSonar", config=None
    ):
        self.config = {} if config is None else config

        # default range for bins
        b_range = 200

        if "BinsRange" in self.config:
            b_range = self.config["BinsRange"]

        b_range = 200
        min_range = self.config.get("RangeMin", 0.5)
        max_range = self.config.get("RangeMax", 10)

        if "RangeBins" in self.config and "RangeRes" in self.config:
            raise ValueError(
                "Can't set both RangeBins and RangeRes in SinglebeamSonar, use one of them in your configuration"
            )
        elif "RangeBins" in self.config:
            b_range = self.config["RangeBins"]
        elif "RangeRes" in self.config:
            b_range = int((max_range - min_range) // self.config["RangeRes"])

        if "OpeningAngleBins" in self.config and "OpeningAngleRes" in self.config:
            raise ValueError(
                "Can't set both OpeningAngleBins and OpeningAngleRes in SinglebeamSonar, use one of them in your configuration"
            )

        if "CentralAngleBins" in self.config and "CentralAngleRes" in self.config:
            raise ValueError(
                "Can't set both CentralAngleBins and CentralAngleRes in SinglebeamSonar, use one of them in your configuration"
            )

        if "AddSigma" in self.config and "AddCov" in self.config:
            raise ValueError(
                "Can't set both AddSigma and AddCov in SinglebeamSonar, use one of them in your configuration"
            )

        if "MultSigma" in self.config and "MultCov" in self.config:
            raise ValueError(
                "Can't set both MultSigma and MultCov in SinglebeamSonar, use one of them in your configuration"
            )

        self.shape = [b_range]

        super(SinglebeamSonar, self).__init__(
            client, agent_name, agent_type, name=name, config=config
        )

    @property
    def dtype(self):
        return np.float32

    @property
    def data_shape(self):
        return self.shape


class ProfilingSonar(ImagingSonar):
    """Simulates a multibeam profiling sonar. This is largely based off of the imaging sonar (:class:`~holoocean.sensors.ImagingSonar`), just with
    different defaults. See :ref:`configure-octree` for more on how to configure the octree that is used.

    The ``configuration`` block (see :ref:`configuration-block`) accepts any of
    the options in the following sections.

    **Basic Configuration**

    - ``Azimuth``: Azimuth (side to side) angle visible in degrees, defaults to 120.
    - ``Elevation``: Elevation angle (up and down) visible in degrees, defaults to 1.
    - ``RangeMin``: Minimum range visible in meters, defaults to 0.5.
    - ``RangeMax``: Maximum range visible in meters, defaults to 75.
    - ``RangeBins``/``RangeRes``: Number of range bins of resulting image, or resolution (length in meters) of each bin. Set one or the other. Defaults to 750 bins.
    - ``AzimuthBins``/``AzimuthRes``: Number of azimuth bins of resulting image, or resolution (length in degrees) of each bin. Set one or the other. Defaults to 480 bins.

    **Noise Configuration**

    - ``AddSigma``/``AddCov``: Additive noise std/covariance from a Rayleigh distribution. Needs to be a float. Set one or the other. Defaults to 0, or off.
    - ``MultSigma``/``MultCov``: Multiplication noise std/covariance from a normal distribution. Needs to be a float. Set one or the other. Defaults to 0, or off.
    - ``MultiPath``: Whether to compute multipath or not. Defaults to False.
    - ``ClusterSize``: Size of cluster when multipath is enabled. Defaults to 5.
    - ``ScaleNoise``: Whether to scale the returned intensities or not. Defaults to False.
    - ``AzimuthStreaks``: What sort of azimuth artifacts to introduce. -1 is a removal artifact, 0 is no artifact, and 1 is increased gain artifact. Defaults to 0.

    **Advanced Configuration**

    - ``ShowWarning``: Whether to show on screen warning about sonar computation happening. Defaults to True.
    - ``ElevationBins``/``ElevationRes``: Number of elevation bins used when shadowing is done, or resolution (length in degrees) of each bin. Set one or the other. By default this is computed based on the octree size and the min/max range. Should only be set if shadowing isn't working.
    - ``InitOctreeRange``: Upon startup, all mid-level octrees within this distance of the agent will be created.
    - ``ViewRegion``: Turns on green lines to see visible region. Defaults to False.
    - ``ViewOctree``: What octree leaves to show. Less than -1 means none, -1 means all, and anything greater than or equal to 0 shows the corresponding beam index. Defaults to -10.
    - ``ShadowEpsilon``: What constitutes a break between clusters when shadowing. Defaults to 4*OctreeMin.
    - ``WaterDensity``: Density of water in kg/m^3. Defaults to 997.
    - ``WaterSpeedSound``: Speed of sound in water in m/s. Defaults to 1480.
    - ``UseApprox``: Whether to use the faster approximation of Atan2 or the slower exact implementation. Defaults to True.

    """

    sensor_type = "ProfilingSonar"

    def __init__(
        self, client, agent_name, agent_type, name="ProfilingSonar", config=None
    ):
        if "RangeMin" not in config:
            config["RangeMin"] = 0.5

        if "RangeMax" not in config:
            config["RangeMax"] = 75

        if "RangeBins" not in config and "RangeRes" not in config:
            config["RangeBins"] = 750

        if "AzimuthBins" not in config and "AzimuthRes" not in config:
            config["AzimuthBins"] = 480

        super(ProfilingSonar, self).__init__(
            client, agent_name, agent_type, name=name, config=config
        )


class DVLSensor(HoloOceanSensor):
    """Doppler Velocity Log Sensor.

    Returns a 1D numpy array of::

       [velocity_x, velocity_y, velocity_z, range_x_forw, range_y_forw, range_x_back, range_y_back]


    With the range potentially not returning if ``ReturnRange`` is set to false.

    **Configuration**

    The ``configuration`` block (see :ref:`configuration-block`) accepts the
    following options:

    - ``Elevation``: Angle of each acoustic beam off z-axis pointing down. Only used for noise/visualization. Defaults to 22.5 degrees => 45 field of view.
    - ``DebugLines``: Whether to show lines of each beam. Defaults to false.
    - ``VelSigma``/``VelCov``: Covariance/Std to be applied to each beam velocity. Can be scalar, 4-vector or 4x4-matrix. Set one or the other. Defaults to 0 => no noise.
    - ``ReturnRange``: Boolean of whether range of beams should also be returned. Defaults to true.
    - ``MaxRange``: Maximum range that can be returned by the beams.
    - ``RangeSigma``/``RangeCov``: Covariance/Std to be applied to each beam range. Can be scalar, 4-vector or 4x4-matrix. Set one or the other. Defaults to 0 => no noise.

    """

    sensor_type = "DVLSensor"

    def __init__(self, client, agent_name, agent_type, name="DVLSensor", config=None):
        self.config = {} if config is None else config

        return_range = self.config.get("ReturnRange", True)

        if "VelSigma" in self.config and "VelCov" in self.config:
            raise ValueError(
                "Can't set both VelSigma and VelCov in DVLSensor, use one of them in your configuration"
            )

        if "RangeSigma" in self.config and "RangeCov" in self.config:
            raise ValueError(
                "Can't set both RangeSigma and RangeCov in DVLSensor, use one of them in your configuration"
            )

        self.shape = [7] if return_range else [3]

        super(DVLSensor, self).__init__(
            client, agent_name, agent_type, name=name, config=config
        )

    @property
    def dtype(self):
        return np.float32

    @property
    def data_shape(self):
        return self.shape


class DepthSensor(HoloOceanSensor):
    """Pressure/Depth Sensor.

    Returns a 1D numpy array of::

       [position_z]


    **Configuration**

    The ``configuration`` block (see :ref:`configuration-block`) accepts the
    following options:

    - ``Sigma``/``Cov``: Covariance/Std to be applied, a scalar. Defaults to 0 => no noise.

    """

    sensor_type = "DepthSensor"

    def __init__(self, client, agent_name, agent_type, name="DepthSensor", config=None):
        self.config = {} if config is None else config

        if "Sigma" in self.config and "Cov" in self.config:
            raise ValueError(
                "Can't set both Sigma and Cov in DepthSensor, use one of them in your configuration"
            )

        super(DepthSensor, self).__init__(
            client, agent_name, agent_type, name=name, config=config
        )

    @property
    def dtype(self):
        return np.float32

    @property
    def data_shape(self):
        return [1]


class GPSSensor(HoloOceanSensor):
    """Gets the location of the agent in the world if the agent is close enough to the surface.

    Returns coordinates in ``[x, y, z]`` format (see :ref:`coordinate-system`)

    **Configuration**

    The ``configuration`` block (see :ref:`configuration-block`) accepts the
    following options:

    - ``Sigma``/``Cov``: Covariance/Std of measurement. Can be scalar, 3-vector or 3x3-matrix. Set one or the other. Defaults to 0 => no noise.
    - ``Depth``: How deep in the water we can still receive GPS messages in meters. Defaults to 2m.
    - ``DepthSigma``/``DepthCov``: Covariance/Std of depth. Must be a scalar. Set one or the other. Defaults to 0 => no noise.

    """

    sensor_type = "GPSSensor"

    def __init__(self, client, agent_name, agent_type, name="GPSSensor", config=None):
        self.config = {} if config is None else config

        if "Sigma" in self.config and "Cov" in self.config:
            raise ValueError(
                "Can't set both Sigma and Cov in GPSSensor, use one of them in your configuration"
            )

        if "DepthSigma" in self.config and "DepthCov" in self.config:
            raise ValueError(
                "Can't set both DepthSigma and DepthCov in GPSSensor, use one of them in your configuration"
            )

        super(GPSSensor, self).__init__(
            client, agent_name, agent_type, name=name, config=config
        )

    @property
    def dtype(self):
        return np.float32

    @property
    def data_shape(self):
        return [3]

    @property
    def sensor_data(self):
        if (
            ~np.any(np.isnan(self._sensor_data_buffer))
            and self.tick_count == self.tick_every
        ):
            return self._sensor_data_buffer
        else:
            return None


class MagnetometerSensor(HoloOceanSensor):
    """Gets the global x-axis (or given vector) in the local frame.

    **Configuration**

    The ``configuration`` block (see :ref:`configuration-block`) accepts the
    following options:

    - ``Sigma``/``Cov``: Covariance/Std of measurement. Can be scalar, 3-vector or 3x3-matrix. Set one or the other. Defaults to 0 => no noise.
    - ``MagneticVector``: The given 3-vector to measure in the global frame. Defaults to [1,0,0].

    """

    sensor_type = "MagnetometerSensor"

    def __init__(
        self, client, agent_name, agent_type, name="MagnetometerSensor", config=None
    ):
        self.config = {} if config is None else config

        if "Sigma" in self.config and "Cov" in self.config:
            raise ValueError(
                "Can't set both Sigma and Cov in MagnetometerSensor, use one of them in your configuration"
            )

        super(MagnetometerSensor, self).__init__(
            client, agent_name, agent_type, name=name, config=config
        )

    @property
    def dtype(self):
        return np.float32

    @property
    def data_shape(self):
        return [3]


class PoseSensor(HoloOceanSensor):
    """Gets the forward, right, and up vector for the agent.
    Note that this is based on the sensor's frame, not the agent's frame so in the IMU socket, 
    it will be NED, and in COM socket it will NWU. Our provided configurations have the sensor in the IMU socket.
    
    Returns a 2D numpy array of

    ::

       [ [R, p],
         [0, 1] ]

    where R is the rotation matrix (See OrientationSensor) and p is the robot world location (see LocationSensor)
    """

    sensor_type = "PoseSensor"

    @property
    def dtype(self):
        return np.float32

    @property
    def data_shape(self):
        return [4, 4]


class AcousticBeaconSensor(HoloOceanSensor):
    """Acoustic Beacon Sensor. Can send message to other beacon from the :meth:`~holoocean.HoloOceanEnvironment.send_acoustic_message` command.

    Returning array depends on sent message type. Note received message will be delayed due to time of
    acoustic wave traveling. Possibly message types are, with ϕ representing the azimuth, ϴ
    elevation, r range, and d depth in water,

    - ``OWAY``: One way message that sends ``["OWAY", from_sensor, payload]``
    - ``OWAYU``: One way message that sends ``["OWAYU", from_sensor, payload, ϕ, ϴ]``
    - ``MSG_REQ``: Requests a return message of MSG_RESP and sends ``["MSG_REQ", from_sensor, payload]``
    - ``MSG_RESP``: Return message that sends ``["MSG_RESP", from_sensor, payload]``
    - ``MSG_REQU``: Requests a return message of MSG_RESPU and sends ``["MSG_REQU", from_sensor, payload, ϕ, ϴ]``
    - ``MSG_RESPU``: Return message that sends ``["MSG_RESPU", from_sensor, payload, ϕ, ϴ, r]``
    - ``MSG_REQX``: Requests a return message of MSG_RESPX and sends ``["MSG_REQX", from_sensor, payload, ϕ, ϴ, d]``
    - ``MSG_RESPX``: Return message that sends ``["MSG_RESPX", from_sensor, payload, ϕ, ϴ, r, d]``

    These messages types are based on the `Blueprint Subsea SeaTrac X150 <https://www.blueprintsubsea.com/pages/product.php?PN=BP00795>`_

    **Configuration**

    The ``configuration`` block (see :ref:`configuration-block`) accepts the
    following options:

    - ``id``: Id of this sensor. If not given, they are numbered sequentially.
    - ``CheckVisible``: Whether to check for linear line of sight to vehicle when verify message will be received. Used when sending data. Defaults to false.
    - ``MaxDistance``: Max Distance in meters of AcousticBeacon. Used when sending data. Defaults to no max distance.
    - ``DistanceSigma``/``DistanceCov``: Determines the standard deviation/covariance of the noise on MaxDistance. Must be scalar value. (default 0 => no noise)
    """

    sensor_type = "AcousticBeaconSensor"
    instances = dict()

    def __init__(
        self, client, agent_name, agent_type, name="AcousticBeaconSensor", config=None
    ):
        self.sending_to = []

        # assign an id
        # TODO This could possibly assign a later to be used id
        # For safety either give all beacons id's or none of them
        curr_ids = set(i.id for i in self.__class__.instances.values())
        if "id" in config and config["id"] not in curr_ids:
            self.id = config["id"]
        elif len(curr_ids) == 0:
            self.id = 0
        else:
            all_ids = set(range(max(curr_ids) + 2))
            self.id = min(all_ids - curr_ids)

        # keep running list of all beacons
        self.__class__.instances[self.id] = self

        super(AcousticBeaconSensor, self).__init__(
            client, agent_name, agent_type, name=name, config=config
        )

    @property
    def dtype(self):
        return np.float32

    @property
    def data_shape(self):
        return [4]

    @property
    def status(self):
        if len(self.sending_to) == 0:
            return "Idle"
        else:
            return "Transmitting"

    def send_message(self, id_to, msg_type, msg_data):
        # Clean out id_to parameters
        if id_to == -1 or id_to == "all":
            id_to = list(self.__class__.instances.keys())
            id_to.remove(self.id)
        if isinstance(id_to, int):
            id_to = [id_to]

        # Don't transmit if a message is still waiting to be received
        if self.status == "Transmitting":
            print(f"Beacon {self.id} is still transmitting")
        # otherwise transmit
        else:
            for i in id_to:
                beacon = self.__class__.instances[i]
                command = SendAcousticMessageCommand(
                    self.agent_name, self.name, beacon.agent_name, beacon.name
                )
                self._client.command_center.enqueue_command(command)

                self.sending_to.append(i)

                beacon.msg_data = msg_data
                beacon.msg_type = msg_type

    @property
    def sensor_data(self):
        if ~np.any(np.isnan(self._sensor_data_buffer)):
            # get all beacons sending messages
            sending = [
                i
                for i, val in self.__class__.instances.items()
                if val.status == "Transmitting"
            ]

            # make sure exactly one person is sending messages
            if len(sending) != 1:
                data = None
                # reset all sending beacons since they got muddled
                for i in sending:
                    self.__class__.instances[i].sending_to = []

            # If the message failed to be received b/c of obstacles
            elif np.all(self._sensor_data_buffer == -1):
                data = None
                self.__class__.instances[sending[0]].sending_to = []

            # otherwise parse through type
            else:
                from_sensor = sending[0]
                # stop sending to this beacon
                self.__class__.instances[from_sensor].sending_to.remove(self.id)

                data = [self.msg_type, from_sensor, self.msg_data]

                phi, theta, dist, depth = self._sensor_data_buffer

                if self.msg_type == "OWAY":
                    pass
                elif self.msg_type == "OWAYU":
                    data.extend([phi, theta])
                elif self.msg_type == "MSG_REQ":
                    self.send_message(from_sensor, "MSG_RESP", None)
                elif self.msg_type == "MSG_RESP":
                    pass
                elif self.msg_type == "MSG_REQU":
                    self.send_message(from_sensor, "MSG_RESPU", None)
                    data.extend([phi, theta])
                elif self.msg_type == "MSG_RESPU":
                    data.extend([phi, theta, dist])
                elif self.msg_type == "MSG_REQX":
                    self.send_message(from_sensor, "MSG_RESPX", None)
                    data.extend([phi, theta, depth])
                elif self.msg_type == "MSG_RESPX":
                    data.extend([phi, theta, dist, depth])
                else:
                    raise ValueError("Invalid Acoustic MSG type")

            # reset buffer
            self.msg_data = None
            self.msg_type = None

            return data

        else:
            return None

    def reset(self):
        self.__class__.instances = dict()


class OpticalModemSensor(HoloOceanSensor):
    """Handles communication between agents using an optical modem. Can send message to other modem from the :meth:`~holoocean.HoloOceanEnvironment.send_optical_message` command.

    **Configuration**

    The ``configuration`` block (see :ref:`configuration-block`) accepts the
    following options:

    - ``id``: Id of this sensor. If not given, they are numbered sequentially.
    - ``MaxDistance``: Max Distance in meters of OpticalModem. Used when sending data. (default 50)
    - ``DistanceSigma``/``DistanceCov``: Determines the standard deviation/covariance of the noise on MaxDistance. Must be scalar value. (default 0 => no noise)
    - ``LaserAngle``: Angle of lasers from origin. Measured in degrees. Used when sending data. (default 60)
    - ``AngleSigma``/``AngleCov``: Determines the standard deviation of the noise on LaserAngle. Must be scalar value. (default 0 => no noise)
    - ``LaserDebug``: Show debug traces. (default false)
    - ``DebugNumSides``: Number of sides on the debug cone. (default 72)

    """

    sensor_type = "OpticalModemSensor"
    instances = dict()

    def __init__(
        self, client, agent_name, agent_type, name="OpticalModemSensor", config=None
    ):
        self.config = {} if config is None else config

        if "DistanceSigma" in self.config and "DistanceCov" in self.config:
            raise ValueError(
                "Can't set both DistanceSigma and DistanceCov in OpticalModemSensor, use one of them in your configuration"
            )

        if "AngleSigma" in self.config and "AngleCov" in self.config:
            raise ValueError(
                "Can't set both AngleSigma and AngleCov in OpticalModemSensor, use one of them in your configuration"
            )

        self.sending_to = []

        # assign an id
        # TODO This could possibly assign a later to be used id
        # For safety either give all beacons id's or none of them
        curr_ids = set(i.id for i in self.__class__.instances.values())
        if "id" in config and config["id"] not in curr_ids:
            self.id = config["id"]
        elif len(curr_ids) == 0:
            self.id = 0
        else:
            all_ids = set(range(max(curr_ids) + 2))
            self.id = min(all_ids - curr_ids)

        # keep running list of all beacons
        self.__class__.instances[self.id] = self

        super(OpticalModemSensor, self).__init__(
            client, agent_name, agent_type, name=name, config=config
        )

    def send_message(self, id_to, msg_data):
        # Clean out id_to parameters
        if id_to == -1 or id_to == "all":
            id_to = list(self.__class__.instances.keys())
            id_to.remove(self.id)
        if isinstance(id_to, int):
            id_to = [id_to]

        for i in id_to:
            modem = self.__class__.instances[i]
            command = SendOpticalMessageCommand(
                self.agent_name, self.name, modem.agent_name, modem.name
            )
            self._client.command_center.enqueue_command(command)

            modem.msg_data = msg_data

    @property
    def sensor_data(self):
        if len(self._sensor_data_buffer) > 0 and self._sensor_data_buffer:
            data = self.msg_data
        else:
            data = None

        # reset buffer
        self.msg_data = None
        return data

    @property
    def dtype(self):
        return np.bool_

    @property
    def data_shape(self):
        return [1]

    def reset(self):
        self.__class__.instances = dict()


######################################################################################
class SensorDefinition:
    """A class for new sensors and their parameters, to be used for adding new sensors.

    Args:
        agent_name (:obj:`str`): The name of the parent agent.
        agent_type (:obj:`str`): The type of the parent agent
        sensor_name (:obj:`str`): The name of the sensor.
        sensor_type (:obj:`str` or :class:`HoloOceanSensor`): The type of the sensor.
        socket (:obj:`str`, optional): The name of the socket to attach sensor to.
        location (Tuple of :obj:`float`, optional): ``[x, y, z]`` coordinates to place sensor
            relative to agent (or socket) (see :ref:`coordinate-system`).
        rotation (Tuple of :obj:`float`, optional): ``[roll, pitch, yaw]`` to rotate sensor
            relative to agent (see :ref:`rotations`)
        config (:obj:`dict`): Configuration dictionary for the sensor, to pass to engine
    """

    _sensor_keys_ = {
        "RGBCamera": RGBCamera,
        "DistanceTask": DistanceTask,
        "LocationTask": LocationTask,
        "FollowTask": FollowTask,
        "AvoidTask": AvoidTask,
        "CupGameTask": CupGameTask,
        "CleanUpTask": CleanUpTask,
        "ViewportCapture": ViewportCapture,
        "OrientationSensor": OrientationSensor,
        "IMUSensor": IMUSensor,
        "JointRotationSensor": JointRotationSensor,
        "RelativeSkeletalPositionSensor": RelativeSkeletalPositionSensor,
        "LocationSensor": LocationSensor,
        "RotationSensor": RotationSensor,
        "VelocitySensor": VelocitySensor,
        "DynamicsSensor": DynamicsSensor,
        "PressureSensor": PressureSensor,
        "CollisionSensor": CollisionSensor,
        "RangeFinderSensor": RangeFinderSensor,
        "WorldNumSensor": WorldNumSensor,
        "BallLocationSensor": BallLocationSensor,
        "AbuseSensor": AbuseSensor,
        "DVLSensor": DVLSensor,
        "PoseSensor": PoseSensor,
        "AcousticBeaconSensor": AcousticBeaconSensor,
        "DepthSensor": DepthSensor,
        "OpticalModemSensor": OpticalModemSensor,
        "ImagingSonar": ImagingSonar,
        "SidescanSonar": SidescanSonar,
        "ProfilingSonar": ProfilingSonar,
        "GPSSensor": GPSSensor,
        "MagnetometerSensor": MagnetometerSensor,
        "SinglebeamSonar": SinglebeamSonar,
    }

    # Sensors that need timeout turned off
    _sonar_sensors = [
        "ImagingSonar",
        "ProfilingSonar",
        "SidescanSonar",
        "SinglebeamSonar",
    ]

    # Sensors that are ticked at their rate on the C++ too
    # Generally sensors with a heavy computational cost
    _heavy_sensors = _sonar_sensors + ["RGBCamera"]

    def get_config_json_string(self):
        """Gets the configuration dictionary as a string ready for transport

        Returns:
            (:obj:`str`): The configuration as an escaped json string

        """
        param_str = json.dumps(self.config)
        # Prepare configuration string for transport to the engine
        param_str = param_str.replace('"', '\\"')
        return param_str

    def __init__(
        self,
        agent_name,
        agent_type,
        sensor_name,
        sensor_type,
        socket="",
        location=(0, 0, 0),
        rotation=(0, 0, 0),
        config=None,
        existing=False,
        lcm_channel=None,
        tick_every=None,
    ):
        self.agent_name = agent_name
        self.agent_type = agent_type
        self.sensor_name = sensor_name

        if isinstance(sensor_type, str):
            self.type = SensorDefinition._sensor_keys_[sensor_type]
        else:
            self.type = sensor_type

        self.socket = socket
        self.location = location
        self.rotation = rotation
        self.config = self.type.default_config if config is None else config
        # hacky way to get heavy sensors to capture lined up with python rate
        if sensor_type in SensorDefinition._heavy_sensors:
            self.config["TicksPerCapture"] = tick_every
        self.existing = existing

        if lcm_channel is not None:
            self.lcm_msg = SensorData(sensor_type, lcm_channel)
        else:
            self.lcm_msg = None
        self.tick_every = tick_every


class SensorFactory:
    """Given a sensor definition, constructs the appropriate HoloOceanSensor object."""

    @staticmethod
    def _default_name(sensor_class):
        return sensor_class.sensor_type

    @staticmethod
    def build_sensor(client, sensor_def):
        """Constructs a given sensor associated with client

        Args:
            client (:obj:`str`): Name of the agent this sensor is attached to
            sensor_def (:class:`SensorDefinition`): Sensor definition to construct

        Returns:

        """
        if sensor_def.sensor_name is None:
            sensor_def.sensor_name = SensorFactory._default_name(sensor_def.type)

        result = sensor_def.type(
            client,
            sensor_def.agent_name,
            sensor_def.agent_type,
            sensor_def.sensor_name,
            config=sensor_def.config,
        )

        # TODO: Make this part of the constructors rather than hacking it on
        # Wanted to make sure this is what we want before making large changes
        result.lcm_msg = sensor_def.lcm_msg
        result.tick_every = sensor_def.tick_every
        result.tick_count = sensor_def.tick_every

        return result
