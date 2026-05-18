import threading
import time

import msgpack
import zmq


class ZedOdometryProxy:
    def __init__(self, server_ip="127.0.0.1", control_port=5555, data_port=5556):
        self.server_ip = server_ip
        self.control_port = control_port
        self.data_port = data_port

        # self.message_queue = Queue(maxsize=100)

        self.context = zmq.Context()
        self.running = False
        self.latest_data = {}

        self.start_subscription()

    def start_subscription(self):
        self.running = True
        self.sub_thread = threading.Thread(target=self._subscribe_data, daemon=True)
        self.sub_thread.start()

    def stop_subscription(self):
        self.running = False
        if self.sub_thread.is_alive():
            self.sub_thread.join()

    def _subscribe_data(self):
        socket = self.context.socket(zmq.SUB)
        socket.connect(f"tcp://{self.server_ip}:{self.data_port}")
        socket.setsockopt_string(zmq.SUBSCRIBE, "")

        while self.running:
            try:
                message = socket.recv(flags=zmq.NOBLOCK)

                data = msgpack.unpackb(message)
                self.latest_data = data
                # if not self.message_queue.full():
                #     self.message_queue.put(message)
                # print("Update")
            except zmq.Again:
                time.sleep(0.01)

        socket.close()

    def _send_command(self, command):
        socket = self.context.socket(zmq.REQ)
        socket.connect(f"tcp://{self.server_ip}:{self.control_port}")
        socket.setsockopt(zmq.RCVTIMEO, 3000)
        socket.setsockopt(zmq.LINGER, 0)
        socket.send_string(command)
        response = socket.recv_string()
        socket.close()
        return response

    def enable_sensor(self):
        try:
            return self._send_command("ENABLE")
        except zmq.error.Again:
            return "no response from server"

    def disable_sensor(self):
        return self._send_command("DISABLE")

    def get_status(self):
        return self.latest_data

    # def consume_offline_data(self):
    #     try:
    #         data = self.message_queue.get_nowait()
    #         return data
    #     except Empty:
    #         return None

    def close(self):
        self.stop_subscription()
        self.disable_sensor()
        # self.context.term()
        
    def __del__(self):
        self.close()

if __name__ == "__main__":
    client = ZedOdometryProxy(server_ip="192.168.24.102")
    print("client created")
    # client.start_subscription()

    result_msg = client.enable_sensor() # sync block
    print(result_msg)

    try:
        while True:
            print(client.get_status())
            time.sleep(0.02)
    finally:
        client.close()
