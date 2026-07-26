import holoocean
import cv2
import copy
import os
import uuid

# from tests.utils.equality import mean_square_err

base_cfg = {
    "name": "test_viewport_capture",
    "world": "TestWorld",
    "main_agent": "sphere0",
    "frames_per_sec": False,
    "agents": [
        {
            "agent_name": "sphere0",
            "agent_type": "SphereAgent",
            "sensors": [{"sensor_type": "ViewportCapture"}],
            "control_scheme": 0,
            "location": [0.95, -1.75, 0.5],
        }
    ],
}


def test_viewport_capture(resolution):
    """Validates that the ViewportCapture camera is working at the expected resolutions

    Also incidentally validates that the viewport can be sized correctly
    """

    global base_cfg

    cfg = copy.deepcopy(base_cfg)

    cfg["window_width"] = resolution
    cfg["window_height"] = resolution

    cfg["agents"][0]["sensors"][0]["configuration"] = {
        "CaptureWidth": resolution,
        "CaptureHeight": resolution,
    }

    binary_path = holoocean.packagemanager.get_binary_path_for_package("TestWorlds")
    with holoocean.environments.HoloOceanEnvironment(
        scenario=cfg,
        binary_path=binary_path,
        show_viewport=False,
        verbose=True,
        uuid=str(uuid.uuid4()),
    ) as env:
        env.should_render_viewport(True)
        env.tick(5)
        env.should_render_viewport(True)
        env.tick(5)

        pixels = env.tick()["ViewportCapture"][:, :, 0:3]
        filename = str("baseline_viewport_" + str(resolution) + ".png")

        cv2.imwrite(filename, pixels)
        cv2.waitKey(0)


def test_rgb_camera(resolution):
    """Makes sure that the RGB camera is positioned and capturing correctly.

    Capture pixel data, and load from disk the baseline of what it should look like.
    Then, use mse() to see how different the images are.

    """

    global base_cfg

    cfg = copy.deepcopy(base_cfg)

    cfg["agents"][0]["sensors"][0]["sensor_type"] = "RGBCamera"
    cfg["agents"][0]["sensors"][0]["socket"] = "CameraSocket"
    cfg["agents"][0]["sensors"][0]["configuration"] = {
        "CaptureWidth": resolution,
        "CaptureHeight": resolution,
    }

    binary_path = holoocean.packagemanager.get_binary_path_for_package("TestWorlds")

    with holoocean.environments.HoloOceanEnvironment(
        scenario=cfg,
        binary_path=binary_path,
        show_viewport=False,
        verbose=True,
        uuid=str(uuid.uuid4()),
    ) as env:
        for _ in range(5):
            env.tick()

        pixels = env.tick()["TestCamera"][:, :, 0:3]
        filepath = str("baseline_temp_" + str(resolution) + ".png")
        cv2.imwrite(filepath, pixels)
        cv2.imshow("image", pixels)
        cv2.waitKey(0)


if __name__ == "__main__":
    print("Hello")
    # test_viewport_capture(256)
    test_viewport_capture(500)
    test_viewport_capture(1000)

    # test_sensor_rotation(rotation_env())
