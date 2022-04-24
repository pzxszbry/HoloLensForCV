#pragma once

namespace HoloLensForCV
{
    public ref class ROSSensorFrameStreamingServer sealed
        : public ISensorFrameSink
    {
    public:
        ROSSensorFrameStreamingServer(
            _In_ Platform::String^ serviceName);

        virtual void Send(
            SensorFrame^ sensorFrame);

    private:
        ~ROSSensorFrameStreamingServer();

        void OnConnection(
            Windows::Networking::Sockets::StreamSocketListener^ listener,
            Windows::Networking::Sockets::StreamSocketListenerConnectionReceivedEventArgs^ object);

        Windows::Foundation::Numerics::float4x4 GetAbsoluteCameraPoseForPV(HoloLensForCV::SensorFrame^ frame);
        Windows::Foundation::Numerics::float4x4 GetAbsoluteCameraPoseForDepth(HoloLensForCV::SensorFrame^ frame);
        Windows::Foundation::Numerics::float4x4 GetAbsoluteCameraPose(HoloLensForCV::SensorFrame^ frame);

        void WriteFloat4x4(
            Windows::Foundation::Numerics::float4x4 matrix,
            Windows::Storage::Streams::DataWriter^ dataWriter);

    private:
        Windows::Networking::Sockets::StreamSocketListener^ _listener;
        Windows::Networking::Sockets::StreamSocket^ _socket;
        Windows::Storage::Streams::DataWriter^ _writer;
        bool _writeInProgress;
        Windows::Foundation::DateTime _previousTimestamp;
        //Io::TimeConverter _timeConverter;
    };
}
