import sys
import argparse
import socket
import struct
from collections import namedtuple
import cv2
import numpy as np
import multiprocessing

# Each port corresponds to a single stream type
STREAM_PORTS = {
    "color": 10080,
    "depth": 10081
}

SENSOR_STREAM_HEADER_FORMAT = "@qIIIIffffffffffffffff"
SENSOR_FRAME_STREAM_HEADER = namedtuple(
    'SensorFrameStreamHeader',
    [ 'Timestamp', 'ImageWidth', 'ImageHeight', 'PixelStride', 'BytesLength',
    'CameraPoseM11', 'CameraPoseM12', 'CameraPoseM13', 'CameraPoseM14',
    'CameraPoseM21', 'CameraPoseM22', 'CameraPoseM23', 'CameraPoseM24',
    'CameraPoseM31', 'CameraPoseM32', 'CameraPoseM33', 'CameraPoseM34',
    'CameraPoseM41', 'CameraPoseM42', 'CameraPoseM43', 'CameraPoseM44']
)

def parse_args():
    parser = argparse.ArgumentParser()
    parser.add_argument("--host",
                    help="Host address to connect", default="192.168.50.202")
    parser.add_argument("--type",
                    help="sensor type", default="color")
    args = parser.parse_args()

    return args


def main(host, sensor_type):
    """Receiver main"""
    port = STREAM_PORTS[sensor_type]

    # Create a TCP Stream socket
    try:
        ss = socket.socket(family=socket.AF_INET, type=socket.SOCK_STREAM)
        print('=> [INFO]: Socket create success')
    except socket.error as msg:
        print("=> [ERROR]: Failed to create socket!!! {}".format(msg))
        sys.exit()

    # Try connecting to the address 'ip_addr:port'
    ss.connect((host, port))
    print("=> [INFO]: Socket connected to {}:{}".format(host, port))

    # Try receive data
    try:
        while True:
            reply = ss.recv(struct.calcsize(SENSOR_STREAM_HEADER_FORMAT))
            if not reply:
                print('=> [ERROR]: Failed to receive data!!!')
                sys.exit()

            data = struct.unpack(SENSOR_STREAM_HEADER_FORMAT, reply)

            # Parse the header
            header = SENSOR_FRAME_STREAM_HEADER(*data)
            # print("=> [INFO]: header\n",header)

            # Read the image in chunks
            image_data = b""
            while len(image_data) < header.BytesLength:
                remaining_bytes = header.BytesLength - len(image_data)
                image_data_chunk = ss.recv(remaining_bytes)
                if not image_data_chunk:
                    print('=> [ERROR]: Failed to receive image data')
                    sys.exit()
                image_data += image_data_chunk

            # Depth image
            if header.PixelStride==2:
                image_array = np.frombuffer(image_data, dtype=np.uint16).copy()
                image_array = image_array.reshape((header.ImageHeight, header.ImageWidth, -1))
                cv2.imwrite("./out/depth_{}.png".format(header.Timestamp), image_array)
                image_array  = cv2.applyColorMap(cv2.convertScaleAbs(image_array, alpha=0.03), cv2.COLORMAP_JET)
                # print("=> [INFO]: Depth image received: {} bits, {}".format(header.BytesLength, image_array.shape))

            # Color image BGRA8
            if header.PixelStride==4:
                image_array = np.frombuffer(image_data, dtype=np.uint8).copy()
                image_array = image_array.reshape((header.ImageHeight, header.ImageWidth, -1))
                cv2.imwrite("./out/color_{}.png".format(header.Timestamp), image_array)
                # print("=> [INFO]: Color image received: {} bits, {}".format(header.BytesLength, image_array.shape))

            # Color image BGR8
            if header.PixelStride==3:
                image_array = np.frombuffer(image_data, dtype=np.uint8).copy()
                image_array = image_array.reshape((header.ImageHeight, header.ImageWidth, -1))
                cv2.imwrite("./out/color_{}.png".format(header.Timestamp), image_array)
                # print("=> [INFO]: Color image received: {} bits, {}".format(header.BytesLength, image_array.shape))

            # Display image
            cv2.imshow('Stream Preview', image_array)
            if cv2.waitKey(1) & 0xFF == ord('q'):
                break
    except KeyboardInterrupt:
        pass

    cv2.destroyAllWindows()
    ss.close()
    print('=> [INFO]: Socket close success')


if __name__ == "__main__":
    args = parse_args()
    host = args.host
    sensor_type = args.type.lower()
    if sensor_type == "all":
        p1 = multiprocessing.Process(target=main,args=(host, "color",),name="ColorSensor")
        p2 = multiprocessing.Process(target=main,args=(host, "depth",),name="DepthSensor")
        p1.start()
        p2.start()
        p1.join()
        p2.join()
    else:
        main(host=host, sensor_type=sensor_type)