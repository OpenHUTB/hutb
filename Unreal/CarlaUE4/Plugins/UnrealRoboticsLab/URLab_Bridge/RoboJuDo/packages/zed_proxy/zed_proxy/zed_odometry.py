import time

import numpy as np
import pyzed.sl as sl

# sys.path.append("../")
# from utils.lpf import ActionFilterButter, ActionFilterButterTorch


# def compute_xyz_vel_in_camera_frame(vel_xyz_world, world_rotation):
#     vel_x_in_world = vel_xyz_world[0]
#     vel_y_in_world = vel_xyz_world[1]
#     rotation_z_in_world = world_rotation[2]

#     vel_x_in_camera = vel_x_in_world * np.cos(rotation_z_in_world) + vel_y_in_world * np.sin(rotation_z_in_world)
#     vel_y_in_camera = -vel_x_in_world * np.sin(rotation_z_in_world) + vel_y_in_world * np.cos(rotation_z_in_world)

#     compute_xy_vel_in_camera_frame = np.array([vel_x_in_camera, vel_y_in_camera, vel_xyz_world[2]])
#     return compute_xy_vel_in_camera_frame


class ZedOdometry:
    def __init__(self) -> None:
        self.zed = sl.Camera()

        # Create a InitParameters object and set configuration parameters
        init_params = sl.InitParameters()
        init_params.coordinate_units = sl.UNIT.METER
        init_params.coordinate_system = sl.COORDINATE_SYSTEM.RIGHT_HANDED_Z_UP_X_FWD
        init_params.depth_maximum_distance = 6  # Set the maximum depth perception distance to 40m
        init_params.depth_mode = sl.DEPTH_MODE.PERFORMANCE  # Use ULTRA depth mode
        # Open the camera
        status = self.zed.open(init_params)
        if status != sl.ERROR_CODE.SUCCESS:  # Ensure the camera has opened succesfully
            # print("Camera Open : "+repr(status)+". Exit program.")
            # exit()
            self.close()
            raise Exception("Camera Open : " + repr(status) + ".")

        # Create and set RuntimeParameters after opening the camera
        self.runtime_parameters = sl.RuntimeParameters()
        self.runtime_parameters.enable_fill_mode = True

        self.i = 0
        depth = sl.Mat()

        mirror_ref = sl.Transform()
        mirror_ref.set_translation(sl.Translation(2.75, 4.0, 0))

        tracking_params = sl.PositionalTrackingParameters()  # set parameters for Positional Tracking
        tracking_params.enable_imu_fusion = True
        tracking_params.enable_area_memory = False
        tracking_params.enable_pose_smoothing = True
        status = self.zed.enable_positional_tracking(tracking_params)  # enable Positional Tracking
        if status != sl.ERROR_CODE.SUCCESS:
            # print("Enable Positional Tracking : "+repr(status)+". Exit program.")
            self.close()
            raise Exception("Enable Positional Tracking : " + repr(status) + ".")
            # exit()

        self.camera_pose = sl.Pose()
        camera_info = self.zed.get_camera_information()

        self.py_translation = sl.Translation()
        self.py_orientation = sl.Orientation()
        # pose_data = sl.Transform()

        self.last_vel_xyz_world = np.array([0, 0, 0])
        # self.last_vel_xyz_camera = np.array([0, 0, 0])
        self.last_position_xyz = np.array([0, 0, 0])
        self.last_rpy = np.array([0, 0, 0])
        self.last_quat = np.array([0, 0, 0, 1])
        self.last_timestamp = 0

    def get_status(self):
        flag_ok = False
        rpy = self.last_rpy
        quat = self.last_quat
        position_xyz = self.last_position_xyz
        vel_xyz_world = self.last_vel_xyz_world
        # vel_xyz_camera = self.last_vel_xyz_camera

        # A new image is available if grab() returns SUCCESS
        if self.zed.grab(self.runtime_parameters) != sl.ERROR_CODE.SUCCESS:
            print("Error grabbing images")
            flag_ok = False
        else:
            tracking_state = self.zed.get_position(self.camera_pose, sl.REFERENCE_FRAME.WORLD)  # Get the position of the camera in a fixed reference frame (the World Frame)
            if tracking_state != sl.POSITIONAL_TRACKING_STATE.OK:
                print("Tracking state is not OK")
                flag_ok = False
            else:
                flag_ok = True
                # Get rotation and translation and displays it
                rpy = self.camera_pose.get_rotation_vector()
                translation = self.camera_pose.get_translation(self.py_translation)
                orientation = self.camera_pose.get_orientation(self.py_orientation)
                timestamp = self.camera_pose.timestamp.get_nanoseconds()

                position_xyz = np.array([translation.get()[0], translation.get()[1], translation.get()[2]])
                quat = np.array([orientation.get()[0], orientation.get()[1], orientation.get()[2], orientation.get()[3]])

                if self.i > 0:
                    dt = (timestamp - self.last_timestamp) / 1e9
                    vel_xyz_world = (
                        np.array(
                            [
                                position_xyz[0] - self.last_position_xyz[0],
                                position_xyz[1] - self.last_position_xyz[1],
                                position_xyz[2] - self.last_position_xyz[2],
                            ]
                        )
                        / dt
                    )
                    # vel_xyz_camera = compute_xyz_vel_in_camera_frame(vel_xyz_world, rpy)

                    smooth_vel_xyz_world = 0.5 * self.last_vel_xyz_world + 0.5 * vel_xyz_world
                    # smooth_vel_xyz_camera = 0.5 * self.last_vel_xyz_camera + 0.5 * vel_xyz_camera

                    self.last_vel_xyz_world = smooth_vel_xyz_world
                    # self.last_vel_xyz_camera = smooth_vel_xyz_camera
                    self.last_position_xyz = position_xyz
                    self.last_rpt = rpy
                    self.last_quat = quat
                    self.last_timestamp = timestamp

                self.i += 1

        result = {
            "flag_ok": flag_ok,
            "position_xyz": position_xyz.tolist(),
            "vel_xyz_world": vel_xyz_world.tolist(),
            # "vel_xyz_camera": vel_xyz_camera.tolist(),
            "quat": quat.tolist(),
            "rpy": rpy.tolist(),
        }
        return result

    def close(self):
        self.zed.close()

    def __del__(self):
        self.close()


if __name__ == "__main__":
    zed_odometry = ZedOdometry()

    time_start = time.time()
    frame_cnt = 0

    while True:
        odometry = zed_odometry.get_status()
        print(odometry)
        frame_cnt += 1
        print(f"FPS: {frame_cnt/(time.time()-time_start)}")
        time.sleep(1 / 50)
