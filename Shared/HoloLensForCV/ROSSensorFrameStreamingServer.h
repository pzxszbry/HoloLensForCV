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

    private:
        Windows::Networking::Sockets::StreamSocketListener^ _listener;
        Windows::Networking::Sockets::StreamSocket^ _socket;
        Windows::Storage::Streams::DataWriter^ _writer;
        bool _writeInProgress;
    };
}
