# HoloLensForCV

This repo is forked from [HoloLensForCV](https://github.com/microsoft/HoloLensForCV) and modified for our project purpose. We are focusing on the communication between HoloLens and ROS system, so the unused examples/tools are removed.

# Contents
This repository contains two reusable components under folder **[Tools](./Tools)**.

Take a quick look at the [HoloLensForCV UWP component](/Shared/HoloLensForCV/). This component is used by most of our samples and tools to access, stream and record HoloLens sensor data.

- [HoloROSPublisher](/Tools/HoloROSPublisher/)
Used to streaming data of color & depth sensors through network with TCP streaming.
- [Recorder](/Tools/Recorder/)
Used to record sensor data and save in HoloLens device.
- [Python Scripts](/Python/)
You could find the scripts for viewing streaming from HoloROSPublisher and download recorded files from HoloLens.

# Using the Samples
**TBD**
- Build Project in Visual Studio 2019
- Deploy Tools to HoloLens

