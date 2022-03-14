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

SENSOR_STREAM_HEADER_FORMAT = "@qIIIIffffffffffffffffffff"
SENSOR_FRAME_STREAM_HEADER = namedtuple(
    'SensorFrameStreamHeader',
    [ 'Timestamp', 'ImageWidth', 'ImageHeight', 'PixelStride', 'BytesLength',
    'FocalLengthX','FocalLengthY','PrincipalPointX','PrincipalPointY',
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
    try:
        while True:
            ss = create_socket()
            try:
                ss.connect((host, port))
                print("=> [INFO] Connection success... ({}:{})".format(host, port))
                pass
            except Exception:
                ss.close()
                timeout_counter +=1
                print('=> [ERROR]: Connection failed ({})!!! ({}:{})'.format(timeout_counter,host, port))
                print("  *Try to reconnect 3 seconds later")
                continue

            while True:
                # Receive the header
                try:
                    header_data = ss.recv(struct.calcsize(SENSOR_STREAM_HEADER_FORMAT))
                except Exception:
                    ss.close()
                    break
                header_data = struct.unpack(SENSOR_STREAM_HEADER_FORMAT, header_data)
                header = SENSOR_FRAME_STREAM_HEADER(*header_data)
                print(header)

                # Read the image in chunks
                image_data = b""
                while len(image_data) < header.BytesLength:
                    remaining_bytes = header.BytesLength - len(image_data)
                    image_data_chunk = ss.recv(remaining_bytes)
                    if not image_data_chunk:
                        print('=> [ERROR]: Failed to receive image data')
                        break
                        # sys.exit()
                    image_data += image_data_chunk

                if len(image_data) != header.BytesLength:
                    break
                # Depth image
                if header.PixelStride==2:
                    image_array = np.frombuffer(image_data, dtype=np.uint16).copy()
                    image_array = image_array.reshape((header.ImageHeight, header.ImageWidth, -1))
                    image_array  = cv2.applyColorMap(cv2.convertScaleAbs(image_array, alpha=0.03), cv2.COLORMAP_JET)

                # Color image BGRA8
                if header.PixelStride==4:
                    image_array = np.frombuffer(image_data, dtype=np.uint8).copy()
                    image_array = image_array.reshape((header.ImageHeight, header.ImageWidth, -1))

                # Color image BGR8
                if header.PixelStride==3:
                    image_array = np.frombuffer(image_data, dtype=np.uint8).copy()
                    image_array = image_array.reshape((header.ImageHeight, header.ImageWidth, -1))

                # Display image
                cv2.imshow('Stream Preview', image_array)
                if cv2.waitKey(1) & 0xFF == ord('q'):
                    # break
                    cv2.destroyAllWindows()
                    ss.close()
                    print('=> [INFO]: Socket close success')
                    sys.exit()
    except KeyboardInterrupt:
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