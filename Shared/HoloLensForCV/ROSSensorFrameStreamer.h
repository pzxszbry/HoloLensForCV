#pragma once

namespace HoloLensForCV
{
    //
    // Collects sensor frames for all the enabled sensors. Opens a stream socket for each
    // of the sensors and streams the sensor images to connected clients.
    //
    public ref class ROSSensorFrameStreamer sealed
        : public ISensorFrameSinkGroup
    {
    public:
        ROSSensorFrameStreamer();

        void Enable(
            _In_ SensorType sensorType);

        virtual ISensorFrameSink^ GetSensorFrameSink(
            _In_ SensorType sensorType);

    private:
        std::array<ROSSensorFrameStreamingServer^, (size_t)SensorType::NumberOfSensorTypes> _sensorFrameStreamingServers;
    };
}
