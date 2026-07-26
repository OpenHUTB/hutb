import cv2, os
from tests.utils.equality import mean_square_err
import holoocean
import pytest


@pytest.mark.skipif(
    "TestWorlds" not in holoocean.installed_packages(),
    reason="Ocean package not installed",
)
def test_sensor_rotation(rotation_env, request):
    """Validates that calling rotate actually rotates the sensor using the RGBCamera.

    Positions the SphereAgent above the target cube so that if it rotates down, it will capture the test
    pattern.
    """
    # Re-use the screenshot for test_rgb_camera
    rotation_env.agents["sphere0"].sensors["RGBCamera"].rotate([0, 0, 0])
    pixels = rotation_env.tick(10)["RGBCamera"][:, :, 0:3]

    filepath = str("/baseline_images/baseline_256.png")
    baseline = cv2.imread(str(request.fspath.dirname + filepath))

    err = mean_square_err(pixels, baseline)
    assert err < 2000, "The sensor appeared to not rotate!"


@pytest.mark.skipif(
    "TestWorlds" not in holoocean.installed_packages(),
    reason="Ocean package not installed",
)
def test_sensor_rotation_resets_after_reset(rotation_env):
    """Validates that the sensor rotation is reset back to the starting position after calling ``.reset()``."""

    # Re-use the screenshot for test_rgb_camera
    rotation_env.agents["sphere0"].sensors["RGBCamera"].rotate([0, 0, 0])
    pixels_before = rotation_env.tick(5)["RGBCamera"][:, :, 0:3]

    rotation_env.reset()

    pixels_after = rotation_env.tick(5)["RGBCamera"][:, :, 0:3]

    err = mean_square_err(pixels_before, pixels_after)
    assert err > 2000, "The images were too similar! Did the sensor not rotate back?"
