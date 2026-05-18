import logging
import threading
import time
from concurrent.futures import ThreadPoolExecutor

import msgpack
import zmq
from zed_odometry import ZedOdometry

sensor_enabled = False
sensor_thread = None
DATA_FPS = 50

logger = logging.getLogger()
logger.addHandler(logging.StreamHandler())
logger.setLevel(logging.DEBUG)

def sensor_loop(socket_pub):
    zed_odometry = ZedOdometry()
    last_data_time = time.time()
    logger.info("[sensor_loop] init done")

    while sensor_enabled:
        last_data_time = time.time()
        data = zed_odometry.get_status()
        data_bytes = msgpack.packb(data)
        socket_pub.send(data_bytes)
        time_lapse = time.time() - last_data_time
        # print(f"Sensor data at {last_data_time} with lapse {time_lapse}")
        if time_lapse < 1 / DATA_FPS:
            time.sleep(1 / DATA_FPS - time_lapse)
        else:
            # print("Frame Drop")
            pass

    zed_odometry.close()
    logger.info("[sensor_loop] zed closed")


def handle_control(socket_rep, socket_pub):
    global sensor_enabled, sensor_thread

    while True:
        message = socket_rep.recv_string()
        if message == "ENABLE":
            if not sensor_enabled:
                sensor_enabled = True
                sensor_thread = threading.Thread(target=sensor_loop, args=(socket_pub,), daemon=True)
                sensor_thread.start()
                # print("wait for init...")
                time.sleep(1)
                socket_rep.send_string("ENABLED")
            else:
                time.sleep(0.2)
                socket_rep.send_string("ALREADY ENABLED")
        elif message == "DISABLE":
            if sensor_enabled:
                sensor_enabled = False
                socket_rep.send_string("DISABLED")
            else:
                socket_rep.send_string("ALREADY DISABLED")
        elif message == "STATUS":
            status = "ENABLED" if sensor_enabled else "DISABLED"
            socket_rep.send_string(status)
        else:
            socket_rep.send_string("UNKNOWN COMMAND")


def main():
    context = zmq.Context()

    # (REQ/REP)
    socket_rep = context.socket(zmq.REP)
    socket_rep.bind("tcp://*:5555")

    # (PUB/SUB)
    socket_pub = context.socket(zmq.PUB)
    socket_pub.bind("tcp://*:5556")

    with ThreadPoolExecutor(max_workers=1) as executor:
        executor.submit(handle_control, socket_rep, socket_pub)


if __name__ == "__main__":
    main()
