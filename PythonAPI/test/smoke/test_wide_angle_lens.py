# Copyright (c) 2025 Computer Vision Center (CVC) at the Universitat Autonoma de
# Barcelona (UAB).
#
# This work is licensed under the terms of the MIT license.
# For a copy, see <https://opensource.org/licenses/MIT>.

from . import SyncSmokeTest

import carla
import time

WIDE_ANGLE_MODELS = [
    "perspective",
    "stereographic",
    "equidistant",
    "equisolid",
    "orthographic",
    "kannala-brandt",
    "brown-conrady",
    "brown_conrady", # Upstream (UE5) spelling must be accepted too.
]

class TestWideAngleLens(SyncSmokeTest):
  def spawn_and_capture(self, bp_sensor, frames=10):
    received = []
    sensor = self.world.spawn_actor(
        bp_sensor,
        carla.Transform(carla.Location(z=50.0), carla.Rotation(pitch=-90.0)))
    try:
      sensor.listen(lambda image: received.append(image))
      for _ in range(0, frames):
        self.world.tick()
      time.sleep(0.5)
    finally:
      sensor.stop()
      sensor.destroy()
    return received

  def test_wide_angle_lens_camera_models(self):
    print("TestWideAngleLens.test_wide_angle_lens_camera_models")

    bp_lib = self.world.get_blueprint_library()

    for model in WIDE_ANGLE_MODELS:
      print("  camera_model={}".format(model))

      bp_sensor = bp_lib.find("sensor.camera.rgb.wide_angle_lens")
      bp_sensor.set_attribute("camera_model", model)
      bp_sensor.set_attribute("fov", "120.0")
      bp_sensor.set_attribute("image_size_x", "320")
      bp_sensor.set_attribute("image_size_y", "240")

      if model == "brown-conrady":
        bp_sensor.set_attribute(
            "distortion_coeffs", "0.1,-0.05,0.01,0.0,0.0")
      elif model == "kannala-brandt":
        bp_sensor.set_attribute(
            "distortion_coeffs", "0.0831,0.0111,0.00858,0.000854")

      received = self.spawn_and_capture(bp_sensor)

      self.assertGreater(
          len(received), 0,
          "\n\n camera_model={} did not produce any image".format(model))

      image = received[0]
      self.assertEqual(image.width, 320, "camera_model=" + model)
      self.assertEqual(image.height, 240, "camera_model=" + model)
      self.assertEqual(
          len(image.raw_data), 320 * 240 * 4, "camera_model=" + model)
