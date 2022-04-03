import sys
import argparse
import socket
import struct
from collections import namedtuple
import cv2
import numpy as np
import time

# Each port corresponds to a single stream type
STREAM_PORTS = {"color": 10080, "depth": 10081}

"""
# ImageEncoding: 
# ref: https://docs.microsoft.com/en-us/uwp/api/Windows.Graphics.Imaging.BitmapPixelFormat?view=winrt-17763#fields
"""
# refer to https://docs.python.org/3/library/struct.html#format-characters
SENSOR_STREAM_HEADER_FORMAT = "@qIIII"
FLOAT_4X4_FORMAT = "@ffffffffffffffff"
SENSOR_FRAME_STREAM_HEADER = namedtuple(
    "SensorFrameStreamHeader",
    [
        "Timestamp",  # Int64: q
        "ImageWidth",  # UInt32: l
        "ImageHeight",  # UInt32: l
        "ImageStep",  # UInt32: l
        "ImageEcoding",  # Uint32: l
    ],
)


def parse_args():
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--host", help="Host address to connect", default="192.168.50.202"
    )
    parser.add_argument("--type", help="sensor type", default="color")
    args = parser.parse_args()
    return args


def receive_data(ss, buffer_size):
    data = b""
    while len(data) < buffer_size:
        remaining_bytes = buffer_size - len(data)
        data_chunk = ss.recv(remaining_bytes)
        if not data_chunk:
            print("=> [ERROR]: Failed to receive data")
        data += data_chunk
    return data


def create_socket():
    # Create a TCP Stream socket
    try:
        ss = socket.socket(family=socket.AF_INET, type=socket.SOCK_STREAM)
        return ss
    except socket.error as msg:
        print("=> [ERROR] Failed to create socket!!!")
        print("  *" + msg)
        sys.exit()


def main(host, sensor_type):
    """Receiver main"""
    port = STREAM_PORTS[sensor_type]
    timeout_counter = 0
    socket.setdefaulttimeout(3)
    pre_timestamp = 0
    try:
        while True:
            ss = create_socket()
            try:
                ss.connect((host, port))
                print("=> [INFO] Connection success... ({}:{})".format(host, port))
                pass
            except Exception:
                ss.close()
                timeout_counter += 1
                print(
                    "=> [ERROR]: Connection failed ({})!!! ({}:{})".format(
                        timeout_counter, host, port
                    )
                )
                print("  *Try to reconnect 3 seconds later")
                continue

            while True:
                # Receive the header
                try:
                    data = receive_data(
                        ss, struct.calcsize(SENSOR_STREAM_HEADER_FORMAT)
                    )
                except Exception:
                    ss.close()
                    break
                header = SENSOR_FRAME_STREAM_HEADER(
                    *struct.unpack(SENSOR_STREAM_HEADER_FORMAT, data)
                )

                if header.Timestamp == pre_timestamp:
                    print("same timestamp!!!")
                    continue
                pre_timestamp = header.Timestamp
                # print("Header", header)

                # Receive the FrameToOrigin Matrix
                try:
                    data = receive_data(ss, struct.calcsize(FLOAT_4X4_FORMAT))
                except Exception:
                    ss.close()
                    break
                FrameToOrigin = (
                    np.frombuffer(data, dtype=np.float32).reshape((4, 4)).transpose()
                )

                # print("FrameToOrigin", FrameToOrigin, sep="\n")

                # Receive the FrameToOrigin Matrix
                try:
                    data = receive_data(ss, struct.calcsize(FLOAT_4X4_FORMAT))
                except Exception:
                    ss.close()
                    break
                CameraViewTransform = (
                    np.frombuffer(data, dtype=np.float32).reshape((4, 4)).transpose()
                )

                # print("CameraViewTransform", CameraViewTransform, sep="\n")

                # Receive the FrameToOrigin Matrix
                try:
                    data = receive_data(ss, struct.calcsize(FLOAT_4X4_FORMAT))
                except Exception:
                    ss.close()
                    break
                CameraProjectionTransform = (
                    np.frombuffer(data, dtype=np.float32).reshape((4, 4)).transpose()
                )

                # print("CameraProjectionTransform", CameraProjectionTransform, sep="\n")
                # sys.exit(0)

                # Read the image in chunks
                image_data = receive_data(ss, header.ImageHeight * header.ImageStep)

                # Depth image
                if header.ImageEcoding == 57:  # Gray16
                    # print("encoding: Gray16")
                    image_array = (
                        np.frombuffer(image_data, dtype=np.uint16)
                        .copy()
                        .reshape(header.ImageHeight, header.ImageWidth, -1)
                    )

                    image_array = cv2.applyColorMap(
                        cv2.convertScaleAbs(image_array, alpha=0.03), cv2.COLORMAP_HOT
                    )

                # Color image BGRA8
                if header.ImageEcoding == 87:  # Bgra8
                    # print("encoding: Bgra8")
                    image_array = (
                        np.frombuffer(image_data, dtype=np.uint8)
                        .copy()
                        .reshape(header.ImageHeight, header.ImageWidth, -1)
                    )
                    image_array = image_array.reshape(
                        (header.ImageHeight, header.ImageWidth, -1)
                    )

                # Display image
                cv2.imshow("Stream Preview", image_array)
                if cv2.waitKey(1) & 0xFF == ord("q"):
                    # break
                    cv2.destroyAllWindows()
                    ss.close()
                    print("=> [INFO]: Socket close success")
                    sys.exit()
    except KeyboardInterrupt:
        cv2.destroyAllWindows()
        ss.close()
        print("=> [INFO]: Socket close success")


if __name__ == "__main__":
    args = parse_args()
    host = args.host
    sensor_type = args.type.lower()
    main(host=host, sensor_type=sensor_type)
