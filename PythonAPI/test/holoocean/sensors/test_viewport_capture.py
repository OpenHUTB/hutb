import copy
import uuid
import cv2
import os
import time
import pytest

from tests.utils.equality import mean_square_err

import holoocean

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


@pytest.mark.skipif(
    "TestWorlds" not in holoocean.installed_packages(),
    reason="Ocean package not installed",
)
def test_viewport_capture(resolution, request):
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

        pixels = env.tick()["ViewportCapture"][:, :, 0:3]

        assert pixels.shape == (resolution, resolution, 3)
        filepath = str("/baseline_images/baseline_viewport_" + str(resolution) + ".png")
        baseline = cv2.imread(str(request.fspath.dirname + filepath))
        # baseline = cv2.imread(os.path.join(request.fspath.dirname, "expected", "{}_viewport.png".format(resolution)))

        err = mean_square_err(pixels, baseline)

        assert (
            err < 1000
        ), "The expected screenshot did not match the actual screenshot!"


@pytest.mark.skipif(
    "TestWorlds" not in holoocean.installed_packages(),
    reason="Ocean package not installed",
)
def test_viewport_capture_after_teleport(env_1024, request):
    """Validates that the ViewportCapture is updated after teleporting the camera
    to a different location.

    Incidentally tests HoloOceanEnvironment.teleport_camera as well
    """

    # Other tests muck with this. Set it to true just in case
    env_1024.should_render_viewport(True)
    env_1024.move_viewport([0.9, -1.75, 0.5], [0, 0, 0])

    for _ in range(5):
        env_1024.tick()

    pixels = env_1024.tick()["ViewportCapture"][:, :, 0:3]

    # baseline = cv2.imread(os.path.join(request.fspath.dirname, "expected", "teleport_viewport_test.png"))
    filepath = str("/baseline_images/baseline_viewport_moved_1024.png")
    baseline = cv2.imread(str(request.fspath.dirname + filepath))
    err = mean_square_err(pixels, baseline)

    assert err < 1000, "The captured viewport differed from the expected screenshot!"


def validate_rendering_viewport_disabled(env_1024, between_tests_callback):
    """Helper function for a few tests. Validates that rendering the viewport is actually disabled
    by teleporting the agent and comparing the before/after RGBCamera captures.

    Args:
        env_1024: environment to use
        between_tests_callback: callback to call before teleporting the camera and taking a 2nd screenshot

    """
    start = time.perf_counter()

    for _ in range(10):
        env_1024.tick()
    elapsed_with_viewport = time.perf_counter() - start

    initial_screenshot = env_1024.tick()["ViewportCapture"][:, :, 0:3]

    between_tests_callback(env_1024)

    env_1024.move_viewport([781, 643, 4376], [381, 403, 3839])

    start = time.perf_counter()
    for _ in range(10):
        env_1024.tick()
    elapsed_without_viewport = time.perf_counter() - start

    final_screenshot = env_1024.tick()["ViewportCapture"][:, :, 0:3]

    err = mean_square_err(initial_screenshot, final_screenshot)

    assert err < 1000, "The screenshots were not identical after disabling rendering"

    # This is unreliable 👇 :/
    # assert elapsed_without_viewport < elapsed_with_viewport, \
    #     "Holodeck did not tick faster without rendering the viewport. Was it actually " \
    #     "disabled?"

    # cleanup environment
    env_1024.should_render_viewport(True)
    env_1024.tick()


def test_viewport_capture_stops_after_disabling_rendering(env_1024):
    """Validates that the viewport render stops updating after disabling viewport
    rendering. Check to make sure it renders faster, and that the images are
    identical after teleporting the camera
    """

    def callback(env_1024):
        env_1024.should_render_viewport(False)

    env_1024.should_render_viewport(True)

    validate_rendering_viewport_disabled(env_1024, callback)


def test_disabling_viewport_persists_after_reset(env_1024):
    """Make sure that after disabling the viewport and calling .reset(), that rendering is still disabled"""

    def callback(env_1024):
        pass

    env_1024.should_render_viewport(False)

    for _ in range(10):
        env_1024.tick()

    env_1024.reset()

    validate_rendering_viewport_disabled(env_1024, callback)
