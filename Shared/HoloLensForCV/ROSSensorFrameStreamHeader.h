#pragma once

namespace HoloLensForCV
{
    //
    // Network header for ROS sensor frame streaming.
    //
    public ref class ROSSensorFrameStreamHeader sealed
    {
    public:
        ROSSensorFrameStreamHeader();

        property uint64_t Timestamp;
        property uint32_t ImageWidth;
        property uint32_t ImageHeight;
        property uint32_t PixelStride;
        property uint32_t BytesLenght;

        static void Write(
            _In_ ROSSensorFrameStreamHeader^ header,
            _Inout_ Windows::Storage::Streams::DataWriter^ dataWriter);
    };
}
