#!/usr/bin/env python3

import ssl, base64, os, tempfile
from _ssl import enum_certificates

# 修复 Python 3.8 和 3.9 运行的错误: ssl.SSLError: [ASN1: NOT_ENOUGH_DATA] not enough data (_ssl.c:4192)
# 当 cadata 加载失败时将 DER 转为 PEM 写入临时文件，再通过 cafile 参数加载
# 参考：https://github.com/agentscope-ai/QwenPaw/issues/5086
_orig_lv = ssl.SSLContext.load_verify_locations
def _patched(self, *args, **kwargs):
    try:
        return _orig_lv(self, *args, **kwargs)
    except ssl.SSLError:
        cadata = kwargs.get("cadata") or (args[2] if len(args) > 2 else None)
        if isinstance(cadata, (bytes, bytearray)):
            purpose = ssl.Purpose.SERVER_AUTH
            pem_certs = []
            for storename in getattr(self, "_windows_cert_stores", ["ROOT", "CA"]):
                try:
                    for cert, encoding, trust in enum_certificates(storename):
                        if encoding == "x509_asn" and (trust is True or purpose.oid in trust):
                            pem_certs.append(b"-----BEGIN CERTIFICATE-----\n" + base64.encodebytes(cert) + b"-----END CERTIFICATE-----\n")
                except PermissionError:
                    pass
            if pem_certs:
                fd, path = tempfile.mkstemp(suffix=".pem")
                try:
                    os.write(fd, b"".join(pem_certs))
                    os.close(fd)
                    return _orig_lv(self, cafile=path)
                finally:
                    os.unlink(path)
        raise
ssl.SSLContext.load_verify_locations = _patched


"""CarlaAir Quick Start - Step 1: 连接验证"""
import carla
import airsim
import argparse


argparser = argparse.ArgumentParser(
    description=__doc__)
argparser.add_argument(
    '--host',
    metavar='H',
    default='localhost',
    help='IP of the host CARLA Simulator (default: localhost)')
argparser.add_argument(
    '-p', '--port',
    metavar='P',
    default=2000,
    type=int,
    help='TCP port of CARLA Simulator (default: 2000)')

args = argparser.parse_args()

# 连接 CARLA（地面仿真）
client = carla.Client(args.host, args.port)
client.set_timeout(10)
world = client.get_world()
print(f"CARLA connection successful: {world.get_map().name}")
print(f"Number of available spawn points:{len(world.get_map().get_spawn_points())}")

# 连接 AirSim（空中仿真）
air = airsim.MultirotorClient(port=41451)
air.confirmConnection()
print("AirSim connection successful: Drone is ready to fly.")

weather = world.get_weather()
print(f"Current weather: Sun altitude={weather.sun_altitude_angle:.1f} degree, Cloudiness={weather.cloudiness:.1f}%")
print("\nCarlaAir is ready! Both ground and aerial APIs are available.")
