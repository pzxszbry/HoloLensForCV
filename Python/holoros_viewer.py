from multiprocessing.connection import wait
import sys
import argparse
import socket
import struct
from collections import namedtuple
import cv2
import numpy as np
import multiprocessing

def parse_args():
    parser = argparse.ArgumentParser()
    parser.add_argument("--host",
                    help="Host address to connect", default="192.168.50.202")
    parser.add_argument("--type",
                    help="Sensor type: color, depth (default color)", default="color")
    args = parser.parse_args()
    return args

class HoloROSViewer():
    def __init__(self, host, sensor_type) -> None:
        self.STREAM_PORTS = {
            "color": 10080,
            "depth": 10081,
            }
        self.STREAM_HEADER_FORMAT = "@qIIIIffffffffffffffffffff"
        self.STREAM_HEADER = namedtuple(
            'SensorFrameStreamHeader',
            [ 'Timestamp', 'ImageWidth', 'ImageHeight', 'PixelStride', 'BytesLength',
            'FocalLengthX','FocalLengthY','PrincipalPointX','PrincipalPointY',
            'CameraPoseM11', 'CameraPoseM12', 'CameraPoseM13', 'CameraPoseM14',
            'CameraPoseM21', 'CameraPoseM22', 'CameraPoseM23', 'CameraPoseM24',
            'CameraPoseM31', 'CameraPoseM32', 'CameraPoseM33', 'CameraPoseM34',
            'CameraPoseM41', 'CameraPoseM42', 'CameraPoseM43', 'CameraPoseM44']
            )
        self.host = host
        self.port = self.STREAM_PORTS[sensor_type]
        self.socket = None
        self.header_size = struct.calcsize(self.STREAM_HEADER_FORMAT)
        socket.setdefaulttimeout(3.0)

    def create_tcp_socket(self):
        while True:
            try:
                self.socket = socket.socket(family=socket.AF_INET, type=socket.SOCK_STREAM)
                break
            except socket.error as msg:
                print("=> [ERROR] Create socket failed!!!")
                print("  *{}".format(msg))

    def close_tcp_socket(self):
        self.socket.close()
        print('=> [INFO] Socket close success...')

    def receive_data(self, data_size):
        data = b""

        while len(data) < data_size:
            remaining_bytes = data_size - len(data)
            try:
                data_chunk = self.socket.recv(remaining_bytes)
                data += data_chunk
            except Exception:
                    break

        return data

    def parse_header(self, header_data):
        self.header = struct.unpack(self.STREAM_HEADER_FORMAT, header_data)
        self.header = self.STREAM_HEADER(*self.header)

    def parse_image(self, image_data):
        # Depth image
        if self.header.PixelStride==2:
            self.image = np.frombuffer(image_data, dtype=np.uint16)
            self.image  = cv2.applyColorMap(cv2.convertScaleAbs(self.image, alpha=0.03), cv2.COLORMAP_JET)
        # Color image BGRA8
        if self.header.PixelStride==4:
            self.image = np.frombuffer(image_data, dtype=np.uint8)
        # Color image BGR8
        if self.header.PixelStride==3:
            self.image = np.frombuffer(image_data, dtype=np.uint8)

        self.image =  self.image.reshape((self.header.ImageHeight, self.header.ImageWidth, -1))

    def stop(self):
        cv2.destroyAllWindows()
        self.close_tcp_socket()
        sys.exit()

    def start(self):
        timeout_counter = 0
        while True:
            self.create_tcp_socket()
            try:
                self.socket.connect((self.host, self.port))
                print("=> [INFO] Connection create success... ({}:{})".format(self.host,self.port))
            except Exception:
                self.socket.close()
                timeout_counter +=1
                print('=> [ERROR]: Connection failed ({})!!! ({}:{})'.format(timeout_counter,self.host, self.port))
                print("  *Try to reconnect 3 seconds later")
                continue

            while True:
                header_data = self.receive_data(self.header_size)
                if len(header_data) != self.header_size:
                    print('=> [ERROR] Failed to receive header data!!!')
                    break
                self.parse_header(header_data)
                print(self.header)

                image_data = self.receive_data(self.header.BytesLength)
                print(self.header.BytesLength)
                print(len(image_data))
                if len(image_data) != self.header.BytesLength:
                    print('=> [ERROR] Failed to receive image data!!!')
                    break
                print(image_data)
                self.parse_image(image_data)
                print(self.image)

                # Display image
                cv2.imshow('Stream Viewer', self.image)
                if cv2.waitKey(1) & 0xFF == ord('q'):
                    self.stop()


if __name__ == "__main__":
    args = parse_args()
    host = args.host
    sensor_type = args.type.lower()
    if sensor_type == "all":
        viewer_color = HoloROSViewer()
        viewer_depth = HoloROSViewer()
        p1 = multiprocessing.Process(target=viewer_color.connect(),args=(host, "color",),name="ColorViewer")
        p2 = multiprocessing.Process(target=viewer_color.connect(),args=(host, "depth",),name="DepthViewer")
        p1.start()
        p2.start()
        p1.join()
        p2.join()
    else:
        viewer = HoloROSViewer(host, sensor_type)
        try:
            viewer.start()
        except Exception:
            viewer.stop()

