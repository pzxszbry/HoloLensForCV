#include "pch.h"

namespace HoloLensForCV
{
    ROSSensorFrameStreamer::ROSSensorFrameStreamer()
    {
    }

    void ROSSensorFrameStreamer::Enable(
        _In_ SensorType sensorType)
    {
        switch (sensorType)
        {
        case SensorType::PhotoVideo:
            _sensorFrameStreamingServers[(int32_t)SensorType::PhotoVideo] =
                ref new ROSSensorFrameStreamingServer(L"10080");
            break;

#if ENABLE_HOLOLENS_RESEARCH_MODE_SENSORS
        case SensorType::ShortThrowToFDepth:
            _sensorFrameStreamingServers[(int32_t)SensorType::ShortThrowToFDepth] =
                ref new ROSSensorFrameStreamingServer(L"10081");
            break;

        case SensorType::LongThrowToFDepth:
            _sensorFrameStreamingServers[(int32_t)SensorType::LongThrowToFDepth] =
                ref new ROSSensorFrameStreamingServer(L"10082");
            break;
#endif /* ENABLE_HOLOLENS_RESEARCH_MODE_SENSORS */
        }
    }

    ISensorFrameSink^ ROSSensorFrameStreamer::GetSensorFrameSink(
        _In_ SensorType sensorType)
    {
        const int32_t sensorTypeAsIndex =
            (int32_t)sensorType;

        REQUIRES(
            0 <= sensorTypeAsIndex &&
            sensorTypeAsIndex < (int32_t)_sensorFrameStreamingServers.size());

        return _sensorFrameStreamingServers[
            sensorTypeAsIndex];
    }
}
