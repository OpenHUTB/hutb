import os
from typing import List, Tuple, Optional

import cv2

from tests.utils.equality import mean_square_err


def display_multiple(images: List[Tuple[List, Optional[str]]]):
    """Displays one or more captures in a CV2 window. Useful for debugging

    Args:
        images: List of tuples containing MxNx3 pixel arrays and optional titles OR
            list of image data
    """
    for image in images:
        if isinstance(image, tuple):
            image_data = image[0]
        else:
            image_data = image

        if isinstance(image, tuple) and len(image) > 1:
            title = image[1]
        else:
            title = "Camera Output"

        cv2.namedWindow(title)
        cv2.moveWindow(title, 500, 500)
        cv2.imshow(title, image_data)
    cv2.waitKey(0)
    cv2.destroyAllWindows()


def display(pixels, title="Camera Output"):
    """Displays the given capture in a CV2 window. Useful for debugging

    Args:
        pixels: MxNx3 array of pixels
        title: The title of the window

    """
    display_multiple([(pixels, title)])


def compare_rgb_sensor_data_with_baseline(
    sensor_data, base_path, baseline_name, show_images=False
) -> float:
    """
    Compare data from RGB sensor with baseline file in `expected` folder and
    return mean squared error
    """
    pixels = sensor_data[:, :, 0:3]
    baseline = cv2.imread(
        os.path.join(base_path, "expected", "{}.png".format(baseline_name))
    )
    if show_images:
        # Show images when debugging--this will block tests until user input
        # is provided
        display_multiple([(pixels, "pixels"), (baseline, "baseline")])
    return mean_square_err(pixels, baseline)


def compare_rgb_sensor_data(sensor_data_1, sensor_data_2, show_images=False) -> float:
    """
    Compare data from RGB sensors
    return mean squared error
    """
    pixels_1 = sensor_data_1[:, :, 0:3]
    pixels_2 = sensor_data_2[:, :, 0:3]
    if show_images:
        # Show images when debugging--this will block tests until user input
        # is provided
        display_multiple([(pixels_1, "image 1"), (pixels_2, "image 2")])
    return mean_square_err(pixels_1, pixels_2)


def write_image_from_rgb_sensor_data(sensor_data, base_path, name):
    """For use in saving `expected` images from RGB camera sensor data

    Args:
        sensor_data: Data from RGB sensor tick
        base_path: Path to test directory containing `expected` directory
        name: Name of file to save, not including `png` file extension
    """
    pixels = sensor_data[:, :, 0:3]
    cv2.imwrite(os.path.join(base_path, "expected", "{}.png".format(name)), pixels)
