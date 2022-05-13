import os
import sys
import glob
import tarfile
import argparse
import sqlite3
import shutil
import json
import subprocess
import urllib.request
import numpy as np

BASE_PATH = os.path.abspath(os.path.dirname(__file__))

print(BASE_PATH)


def parse_args():
    parser = argparse.ArgumentParser()
    parser.add_argument("--address", default="192.168.50.202",
                        help="The IP address for the HoloLens Device Portal")
    parser.add_argument("--username", default="admin",
                        help="The username for the HoloLens Device Portal")
    parser.add_argument("--password", default="123456789",
                        help="The password for the HoloLens Device Portal")
    parser.add_argument("--workspace_path",
                        default=os.path.join(BASE_PATH, "./out"),
                        help="Path to workspace folder")
    args = parser.parse_args()

    return args


if __name__ == "__main__":
    args = parse_args()
    address = args.address
    username = args.username
    password = args.password

    """
    Connection
    """
    print("=> [INFO]: Connecting to HoloLens Device Portal")
    url = "http://{}".format(address)
    password_manager = urllib.request.HTTPPasswordMgrWithDefaultRealm()
    password_manager.add_password(None, url, username, password)
    handler = urllib.request.HTTPBasicAuthHandler(password_manager)
    opener = urllib.request.build_opener(handler)
    opener.open(url)
    urllib.request.install_opener(opener)
    print("=> [INFO]: Connected to HoloLens at address:", url)

    print(
        "=> [INFO]: Retrieves the list of installed apps on the system. Includes details.")
    response = urllib.request.urlopen(
        "{}/api/app/packagemanager/packages".format(url))
    packages = json.loads(response.read().decode())
    print(packages)

    # for package in packages["InstalledPackages"]:
    #     package_name = package["Name"]
    #     package_full_name = package["PackageFullName"]
    #     print(package_name, ":", package_full_name)

    print(
        "=> [INFO]: Get the thermal stage of the device (0 normal, 1 warm, 2 critical).")
    response = urllib.request.urlopen(
        "{}/api/holographic/thermal/stage".format(url))
    thermal = json.loads(response.read().decode())["CurrentStage"]
    print("CurrentThermalStage:", thermal)
    """
    {'CurrentStage': 0}
    """

    # response = urllib.request.urlopen(
    #         "{}/api/holographic/mrc/settings".format(url))
    # response = json.loads(response.read().decode())
    # print(response)
    # """
    # {'MrcSettings': [{'Setting': 'EnableCamera', 'Value': True}, {'Setting': 'EnableHolograms', 'Value': True}, {'Setting': 'EnableSystemAudio', 'Value': False}, {'Setting': 'EnableMicrophone', 'Value': False}, {'Setting': 'VideoStabilizationBuffer', 'Value': 15}]}
    # """

    # response = urllib.request.urlopen(
    #         "{}/api/holographic/mrc/status".format(url))
    # response = json.loads(response.read().decode())
    # print(response)
    # """
    # {'IsRecording': False, 'ProcessStatus': {'MrcProcess': 'Running'}}
    # """

    print("=> [INFO]: Networking")
    response = urllib.request.urlopen(
        "{}/api/networking/ipconfig".format(url))
    response = json.loads(response.read().decode())
    print(response)

    print("=> [INFO]: OS Information")
    print("Gets operating system information.")
    response = urllib.request.urlopen(
        "{}/api/os/info".format(url))
    response = json.loads(response.read().decode())
    print(response)

    print("Gets the machine name.")
    response = urllib.request.urlopen(
        "{}/api/os/machinename ".format(url))
    response = json.loads(response.read().decode())
    print(response)

    print("=> [INFO]: Performance data")
    print("Returns the list of running processes with details.")
    response = urllib.request.urlopen(
        "{}/api/resourcemanager/processes".format(url))
    response = json.loads(response.read().decode())
    print(response)
    print("Returns system perf statistics (I/O read/write, memory stats, etc.")
    response = urllib.request.urlopen(
        "{}/api/resourcemanager/systemperf".format(url))
    response = json.loads(response.read().decode())
    print(response)

    print("=> [INFO]: Power")
    print("Gets the current battery state.")
    response = urllib.request.urlopen(
        "{}/api/power/battery".format(url))
    response = json.loads(response.read().decode())
    print(response)
    print("Checks if the system is in a low power state.")
    response = urllib.request.urlopen(
        "{}/api/power/state".format(url))
    response = json.loads(response.read().decode())
    print(response)

    print("=> [INFO]: Remote Control")
    # print("Restarts the target device.")
    # response = urllib.request.urlopen(urllib.request.Request(
    #         "{}/api/control/restart".format(url), method="POST"))
    # print(response)
    print("Shuts down the target device.")
    response = urllib.request.urlopen(urllib.request.Request(
        "{}/api/control/shutdown".format(url), method="POST")).getcode()
    print(response)
