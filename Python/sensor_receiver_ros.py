import argparse
import code
import socket
import sys
import binascii
import struct
from collections import namedtuple
import cv2
import numpy as np
import multiprocessing
import time

# Each port corresponds to a single stream type
STREAM_PORTS = {
    "photovideo": 10080,
    "shortthrowdepth": 10081,
}

SENSOR_STREAM_HEADER_FORMAT = "@qIIII"

SENSOR_FRAME_STREAM_HEADER = namedtuple(
    'SensorFrameStreamHeader',
    ['Timestamp', 'ImageWidth', 'ImageHeight', 'PixelStride', 'BytesLength']
)

def parse_args():
    parser = argparse.ArgumentParser()
    parser.add_argument("--host",
                    help="Host address to connect", default="192.168.50.202")
    parser.add_argument("--type",
                    help="sensor type", default="PhotoVideo")
    args = parser.parse_args()
    return args


def main(sensor_type):
    """Receiver main"""
    args = parse_args()
    STREAM_PORT = STREAM_PORTS[sensor_type]

    # Create a TCP Stream socket
    try:
        ss = socket.socket(family=socket.AF_INET, type=socket.SOCK_STREAM)
        print('=> INFO: socket created')
    except socket.error as msg:
        print("=> ERROR: Failed to create socket. {}".format(msg))
        sys.exit()

    # Try connecting to the address 'ip_addr:port'
    ss.connect((args.host, STREAM_PORT))
    print("=> INFO: Socket Connected to {ip_addr}:{port}".format(ip_addr=args.host, port=STREAM_PORT))

    # Try receive data
    start_time = time.time()
    image_count = 0

    try:
        while True:
            reply = ss.recv(struct.calcsize(SENSOR_STREAM_HEADER_FORMAT))
            if not reply:
                print('ERROR: Failed to receive data')
                sys.exit()

            data = struct.unpack(SENSOR_STREAM_HEADER_FORMAT, reply)

            # Parse the header
            header = SENSOR_FRAME_STREAM_HEADER(*data)

            # Record the meta data
            imageHeight = header.ImageHeight
            imageWidth = header.ImageWidth
            pixelStride = header.PixelStride
            bytesLength = header.BytesLength
            print("bytesLength:", bytesLength)

            # Read the image in chunks
            # image_size_bytes = header.RowStride
            # image_size_bytes = imageHeight * imageWidth * pixelStride
            image_data = b""

            while len(image_data) < bytesLength:
                remaining_bytes = bytesLength - len(image_data)
                image_data_chunk = ss.recv(remaining_bytes)
                if not image_data_chunk:
                    print('ERROR: Failed to receive image data')
                    sys.exit()
                image_data += image_data_chunk

            # depth image
            if pixelStride==2:
                image_array = np.frombuffer(image_data, dtype=np.uint16).copy()
                image_array = image_array.byteswap(True).reshape((
                    header.ImageHeight,
                    header.ImageWidth, -1))
                image_array = cv2.applyColorMap(cv2.convertScaleAbs(image_array), cv2.COLORMAP_JET)

            # color image
            # if pixelStride==4:
            else:
                image_array = np.frombuffer(image_data, dtype=np.uint8).copy()
                image_array = image_array.reshape((
                    imageHeight,
                    imageWidth, -1))

            cv2.imshow('Streaming', image_array)

            if cv2.waitKey(1) & 0xFF == ord('q'):
                break
        
    except KeyboardInterrupt:
        pass

    ss.close()
    cv2.destroyAllWindows()


if __name__ == "__main__":
    # main("photovideo")
    # main("shortthrowdepth")

    p1 = multiprocessing.Process(target=main,args=("photovideo",),name="PV")
    p2 = multiprocessing.Process(target=main,args=("shortthrowdepth",),name="ShortThrow")
    p1.start()
    p2.start()
    p1.join()
    p2.join()
    print("done!")
