# Copyright (c) 2026 Jonathan Embley-Riches. All rights reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
# --- LEGAL DISCLAIMER ---
# UnrealRoboticsLab is an independent software plugin. It is NOT affiliated with, 
# endorsed by, or sponsored by Epic Games, Inc. "Unreal" and "Unreal Engine" are 
# trademarks or registered trademarks of Epic Games, Inc. in the US and elsewhere.
#
# This plugin incorporates third-party software: MuJoCo (Apache 2.0), 
# CoACD (MIT), and libzmq (MPL 2.0). See ThirdPartyNotices.txt for details.

import zmq
import struct
import cv2
import rclpy
from rclpy.node import Node
from sensor_msgs.msg import JointState, Image
from std_msgs.msg import Float64MultiArray
from cv_bridge import CvBridge

class URLabBridge(Node):
    def __init__(self, zmq_sub_endpoint="tcp://127.0.0.1:5555", zmq_pub_endpoint="tcp://127.0.0.1:5556", zmq_camera_endpoint="tcp://127.0.0.1:5558"):
        super().__init__('urlab_ros2_bridge')
        
        # ZeroMQ Setup
        self.context = zmq.Context()
        
        # Subscriber (Low Latency State Data)
        self.zmq_sub = self.context.socket(zmq.SUB)
        self.zmq_sub.connect(zmq_sub_endpoint)
        self.zmq_sub.setsockopt_string(zmq.SUBSCRIBE, "") # Listen to all robots
        
        # Subscriber (High Bandwidth Camera Data)
        self.zmq_cam = self.context.socket(zmq.SUB)
        self.zmq_cam.connect(zmq_camera_endpoint)
        self.zmq_cam.setsockopt_string(zmq.SUBSCRIBE, "")

        # Publisher (Sends Controls to UE)
        self.zmq_pub = self.context.socket(zmq.PUB)
        self.zmq_pub.connect(zmq_pub_endpoint)
        
        # Caches
        self.publishers_cache = {}
        self.subscribers_cache = {}
        self.known_robots = set()
        self.cv_bridge = CvBridge()
        
        # Setup poller
        self.poller = zmq.Poller()
        self.poller.register(self.zmq_sub, zmq.POLLIN)
        self.poller.register(self.zmq_cam, zmq.POLLIN)

        # Timer
        self.create_timer(0.001, self.poll_zmq) 
        
        self.get_logger().info(f"Bridge initialized. Listening to State at {zmq_sub_endpoint} and Camera at {zmq_camera_endpoint}")
        
    def poll_zmq(self):
        try:
            while True:
                socks = dict(self.poller.poll(0)) # Non-blocking poll
                if not socks:
                    break

                if self.zmq_sub in socks:
                    topic_bytes = self.zmq_sub.recv()
                    if self.zmq_sub.getsockopt(zmq.RCVMORE):
                        payload_bytes = self.zmq_sub.recv()
                        self.handle_telemetry(topic_bytes.decode('utf-8').strip(), payload_bytes)
                
                if self.zmq_cam in socks:
                    topic_bytes = self.zmq_cam.recv()
                    if self.zmq_cam.getsockopt(zmq.RCVMORE):
                        payload_bytes = self.zmq_cam.recv()
                        self.handle_camera(topic_bytes.decode('utf-8').strip(), payload_bytes)

        except zmq.Again:
            pass # No more messages

    def handle_camera(self, topic: str, payload: bytes):
        parts = topic.split('/', 1)
        if len(parts) < 2: return
            
        robot_prefix = parts[0]
        cam_name = parts[1].replace('camera/', '')
        
        # Assuming Unreal resolution of 640x480 for demo. We can pack the res in the struct later.
        expected_size = 640 * 480 * 4
        if len(payload) == expected_size:
            ros_topic = f"/{robot_prefix}/camera/{cam_name}/image_raw"
            if ros_topic not in self.publishers_cache:
                self.publishers_cache[ros_topic] = self.create_publisher(Image, ros_topic, 10)
            
            img_array = np.frombuffer(payload, dtype=np.uint8).reshape((480, 640, 4))
            
            # Unreal's SceneCapture is BGRA -> Convert to BGR for ROS
            img_bgr = cv2.cvtColor(img_array, cv2.COLOR_BGRA2BGR)
            
            msg = self.cv_bridge.cv2_to_imgmsg(img_bgr, encoding="bgr8")
            msg.header.stamp = self.get_clock().now().to_msg()
            msg.header.frame_id = f"{robot_prefix}_{cam_name}_optical_frame"
            
            self.publishers_cache[ros_topic].publish(msg)

    def handle_telemetry(self, topic: str, payload: bytes):
        # Topic format: "robotPrefix/sensorName"
        parts = topic.split('/', 1)
        if len(parts) < 2:
            return
            
        robot_prefix = parts[0]
        sub_topic = parts[1]
        
        # Auto-discover robots and listen for their controls
        if robot_prefix not in self.known_robots:
            self.known_robots.add(robot_prefix)
            self.create_ros_control_subscriber(robot_prefix)
        
        if sub_topic.startswith('joint/'):
            # It's a joint state payload: [Int32 ID, Float Pos, Float Vel, Float Acc]
            if len(payload) == 16:
                joint_id, pos, vel, acc = struct.unpack('<Ifff', payload)
                joint_name = sub_topic[6:] # remove 'joint/'
                
                ros_topic = f"/{robot_prefix}/joint_states"
                if ros_topic not in self.publishers_cache:
                    self.publishers_cache[ros_topic] = self.create_publisher(JointState, ros_topic, 10)
                
                msg = JointState()
                msg.header.stamp = self.get_clock().now().to_msg()
                msg.name = [joint_name]
                msg.position = [pos]
                msg.velocity = [vel]
                msg.effort = [acc]
                self.publishers_cache[ros_topic].publish(msg)
                
        elif sub_topic.startswith('sensor/'):
            # It's a sensor payload: [Int32 ID, Int32 Dim, Floats...]
            if len(payload) >= 8:
                sensor_id, dim = struct.unpack('<II', payload[:8])
                if len(payload) == 8 + dim * 4:
                    vals = struct.unpack(f'<{dim}f', payload[8:])
                    
                    ros_topic = f"/{robot_prefix}/{sub_topic}"
                    if ros_topic not in self.publishers_cache:
                        self.publishers_cache[ros_topic] = self.create_publisher(Float64MultiArray, ros_topic, 10)
                        
                    msg = Float64MultiArray()
                    msg.data = list(vals)
                    self.publishers_cache[ros_topic].publish(msg)

    def create_ros_control_subscriber(self, robot_prefix):
        topic = f"/{robot_prefix}/control"
        sub = self.create_subscription(
            Float64MultiArray,
            topic,
            lambda msg, rp=robot_prefix: self.ros_control_callback(msg, rp),
            10
        )
        self.subscribers_cache[topic] = sub
        self.get_logger().info(f"Created ROS2 subscriber for {topic}")
        
    def ros_control_callback(self, msg, robot_prefix):
        # Pack the binary payload: [Int32 NumControls, (Int32 ActuatorID, Float Value)...]
        num_controls = len(msg.data)
        payload = struct.pack('<I', num_controls)
        
        # For this PoC, we map array index -> Actuator ID. 
        # In a full implementation, you'd map from Actuator names (via info broadcast)
        for idx, val in enumerate(msg.data):
            payload += struct.pack('<If', idx, float(val))
            
        # Send via ZMQ PUB
        zmq_topic = f"{robot_prefix}/control ".encode('utf-8')
        self.zmq_pub.send(zmq_topic, zmq.SNDMORE)
        self.zmq_pub.send(payload)

def main(args=None):
    rclpy.init(args=args)
    bridge = URLabBridge()
    try:
        rclpy.spin(bridge)
    except KeyboardInterrupt:
        pass
    finally:
        bridge.destroy_node()
        rclpy.shutdown()

if __name__ == '__main__':
    main()
