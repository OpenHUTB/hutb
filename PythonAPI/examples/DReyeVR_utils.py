from typing import Optional, Any, Dict, List
import carla
import numpy as np
import time

import sys, os
# sys.path.append(os.path.join(os.getenv("CARLA_ROOT"), "PythonAPI"))
# import examples  # calls ./__init__.py to add all the necessary things to path


def find_ego_vehicle(world: carla.libcarla.World) -> Optional[carla.libcarla.Vehicle]:
    DReyeVR_vehicles: str = "harplab.dreyevr_vehicle.*"
    ego_vehicles_in_world = list(world.get_actors().filter(DReyeVR_vehicles))
    if len(ego_vehicles_in_world) >= 1:
        print(
            f"Found a DReyeVR EgoVehicle in the world ({ego_vehicles_in_world[0].id})"
        )
        return ego_vehicles_in_world[0]

    DReyeVR_vehicle: Optional[carla.libcarla.Vehicle] = None
    available_ego_vehicles = world.get_blueprint_library().filter(DReyeVR_vehicles)
    if len(available_ego_vehicles) == 1:
        bp = available_ego_vehicles[0]
        print(f'Spawning only available EgoVehicle: "{bp.id}"')
    # else:
    #     print(
    #         f"Found {len(available_ego_vehicles)} available EgoVehicles. Which one to use?"
    #     )
    #     for i, ego in enumerate(available_ego_vehicles):
    #         print(f"\t[{i}] - {ego.id}")
    #     print()
    #     ego_choice = f"Pick EgoVehicle to spawn [0-{len(available_ego_vehicles) - 1}]: "
    #     i: int = int(input(ego_choice))
    #     assert 0 <= i < len(available_ego_vehicles)
    #     bp = available_ego_vehicles[i]
    # i: int = 0
    # spawn_pts = world.get_map().get_spawn_points()
    # while DReyeVR_vehicle is None:
    #     print(f'Spawning DReyeVR EgoVehicle: "{bp.id}" at {spawn_pts[i]}')
    #     DReyeVR_vehicle = world.spawn_actor(bp, transform=spawn_pts[i])
    #     i = (i + 1) % len(spawn_pts)
    return DReyeVR_vehicle


def find_ego_sensor(world: carla.libcarla.World) -> Optional[carla.libcarla.Sensor]:
    def get_world_sensors() -> list:
        return list(world.get_actors().filter("harplab.dreyevr_sensor.*"))

    ego_sensors: list = get_world_sensors()
    if len(ego_sensors) == 0:
        # no EgoSensors found in world, trying to spawn EgoVehicle (which spawns an EgoSensor)
        if find_ego_vehicle(world) is None:  # tries to spawn an EgoVehicle
            raise Exception(
                "No EgoVehicle (nor EgoSensor) found in the world! EgoSensor needs EgoVehicle as parent"
            )
    # in case we had to spawn the EgoVehicle, this effect is not instant and might take some time
    # to account for this, we allow some time (max_wait_sec) to continuously ping the server for
    # an updated actor list with the EgoSensor in it.

    start_t: float = time.time()
    # maximum time to keep checking for EgoSensor spawning after EgoVehicle
    maximum_wait_sec: float = 10.0  # might take a while to spawn EgoVehicle (esp in VR)
    while len(ego_sensors) == 0 and time.time() - start_t < maximum_wait_sec:
        # EgoVehicle should now be present, so we can try again
        ego_sensors = get_world_sensors()
        time.sleep(0.1)  # tick to allow the server some time to breathe
    if len(ego_sensors) == 0:
        raise Exception("Unable to find EgoSensor in the world!")
    assert len(ego_sensors) > 0  # should have spawned with EgoVehicle at least
    if len(ego_sensors) > 1:
        print("[WARN] There are >1 EgoSensors in the world! Defaulting to first")
    return ego_sensors[0]  # always return the first one?


class DReyeVRSensor:
    def __init__(self, world: carla.libcarla.World):
        self.ego_sensor: carla.sensor.dreyevrsensor = find_ego_sensor(world)
        self.data: Dict[str, Any] = {}
        print("initialized DReyeVRSensor PythonAPI client")

    def preprocess(self, obj: Any) -> Any:
        if isinstance(obj, carla.libcarla.Vector3D):
            return np.array([obj.x, obj.y, obj.z])
        if isinstance(obj, carla.libcarla.Vector2D):
            return np.array([obj.x, obj.y])
        if isinstance(obj, carla.libcarla.Transform):
            return [
                np.array([obj.location.x, obj.location.y, obj.location.z]),
                np.array([obj.rotation.pitch, obj.rotation.yaw, obj.rotation.roll]),
            ]
        return obj

    def update(self, data) -> None:
        # update local variables
        elements: List[str] = [key for key in dir(data) if "__" not in key]
        for key in elements:
            self.data[key] = self.preprocess(getattr(data, key))

    @classmethod
    def spawn(cls, world: carla.libcarla.World):
        # TODO: check if dreyevr sensor already exsists, then use it
        # spawn a DReyeVR sensor and begin listening
        if find_ego_sensor(world) is None:
            bp = list(world.get_blueprint_library().filter("harplab.dreyevr_sensor.*"))
            try:
                bp = bp[0]
            except IndexError:
                print("no eye tracker in blueprint library?!")
                return None
            ego_vehicle = find_ego_vehicle()
            ego_sensor = world.spawn_actor(
                bp, ego_vehicle.get_transform(), attach_to=ego_vehicle
            )
            print("Spawned DReyeVR sensor: " + ego_sensor.type_id)
        return cls(world)

    def calc_vergence_from_dir(self, L0, R0, LDir, RDir):
        # Calculating shortest line segment intersecting both lines
        # Implementation sourced from http://paulbourke.net/geometry/Ptlineplane/

        L0R0 = L0 - R0  # segment between L origin and R origin
        epsilon = 0.00000001  # small positive real number

        # Calculating dot-product equation to find perpendicular shortest-line-segment
        d1343 = L0R0[0] * RDir[0] + L0R0[1] * RDir[1] + L0R0[2] * RDir[2]
        d4321 = RDir[0] * LDir[0] + RDir[1] * LDir[1] + RDir[2] * LDir[2]
        d1321 = L0R0[0] * LDir[0] + L0R0[1] * LDir[1] + L0R0[2] * LDir[2]
        d4343 = RDir[0] * RDir[0] + RDir[1] * RDir[1] + RDir[2] * RDir[2]
        d2121 = LDir[0] * LDir[0] + LDir[1] * LDir[1] + LDir[2] * LDir[2]
        denom = d2121 * d4343 - d4321 * d4321
        if abs(denom) < epsilon:
            return 1.0  # no intersection, would cause div by 0 err (potentially)
        numer = d1343 * d4321 - d1321 * d4343

        # calculate scalars (mu) that scale the unit direction XDir to reach the desired points
        # variable scale of direction vector for LEFT ray
        muL = numer / denom
        # variable scale of direction vector for RIGHT ray
        muR = (d1343 + d4321 * (muL)) / d4343

        # calculate the points on the respective rays that create the intersecting line
        ptL = L0 + muL * LDir  # the point on the Left ray
        ptR = R0 + muR * RDir  # the point on the Right ray

        # calculate the vector between the middle of the two endpoints and return its magnitude
        # middle point between two endpoints of shortest-line-segment
        ptM = (ptL + ptR) / 2.0
        oM = (L0 + R0) / 2.0  # midpoint between two (L & R) origins
        FinalRay = ptM - oM  # Combined ray between midpoints of endpoints
        # returns the magnitude of the vector (length)
        return np.linalg.norm(FinalRay) / 100.0
