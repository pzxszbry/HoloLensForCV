#include "pch.h"
#include "MemoryBuffer.h"

namespace HoloLensForCV
{
    ROSSensorFrameStreamingServer::ROSSensorFrameStreamingServer(
        _In_ Platform::String^ serviceName)
        : _writeInProgress(false)
    {
        // Initialize a TCP stream socket listener for incoming network 
        _listener = ref new Windows::Networking::Sockets::StreamSocketListener();

        _listener->ConnectionReceived +=
            ref new Windows::Foundation::TypedEventHandler<
            Windows::Networking::Sockets::StreamSocketListener^,
            Windows::Networking::Sockets::StreamSocketListenerConnectionReceivedEventArgs^>(
                this,
                &ROSSensorFrameStreamingServer::OnConnection);

        _listener->Control->KeepAlive = true;

        Concurrency::create_task(_listener->BindServiceNameAsync(serviceName)).then(
            [this](Concurrency::task<void> previousTask)
            {
                try
                {
                    // Try getting an exception.
                    previousTask.get();
                }
                catch (Platform::Exception^ exception)
                {
#if DBG_ENABLE_ERROR_LOGGING
                    dbg::trace(
                        L"ROSSensorFrameStreamingServer::ROSSensorFrameStreamingServer: %s",
                        exception->Message->Data());
#endif /* DBG_ENABLE_ERROR_LOGGING */
                }
            });
    }

    ROSSensorFrameStreamingServer::~ROSSensorFrameStreamingServer()
    {
        delete _listener;
        _listener = nullptr;
    }

    void ROSSensorFrameStreamingServer::OnConnection(
        Windows::Networking::Sockets::StreamSocketListener^ listener,
        Windows::Networking::Sockets::StreamSocketListenerConnectionReceivedEventArgs^ object)
    {
        _socket = object->Socket;

        // Initialize a DataWriter
        {
            _writer = ref new Windows::Storage::Streams::DataWriter(_socket->OutputStream);
            _writer->UnicodeEncoding = Windows::Storage::Streams::UnicodeEncoding::Utf8;
            _writer->ByteOrder = Windows::Storage::Streams::ByteOrder::LittleEndian;    // Check Ubuntu ByteOrder with `lscpu | grep "Byte Order"`
            _writeInProgress = false;
        }
    }

    void ROSSensorFrameStreamingServer::Send(
        SensorFrame^ sensorFrame)
    {
        if (nullptr == _socket)
        {
            return;
        }

        if (_previousTimestamp.UniversalTime.Equals(sensorFrame->Timestamp.UniversalTime))
        {
            return;
        }

        _previousTimestamp = sensorFrame->Timestamp;

#if DBG_ENABLE_INFORMATIONAL_LOGGING
        dbg::TimerGuard timerGuard(
            L"ROSSensorFrameStreamingServer::Send: buffer prepare operation",
            10.0 /* minimum_time_elapsed_in_milliseconds */);
#endif /* DBG_ENABLE_INFORMATIONAL_LOGGING */
        std::chrono::nanoseconds currTimestamp = 
            std::chrono::duration_cast<std::chrono::nanoseconds>(
                std::chrono::system_clock::now().time_since_epoch());   // use current nano seconds as timestamp (already in Unix epoch)
        Windows::Graphics::Imaging::SoftwareBitmap^ bitmap;
        Windows::Foundation::IMemoryBufferReference^ bitmapBufferReference;

        

        Platform::Array<byte>^ imageBufferAsPlatformArray;
        uint32_t imageBufferSize = 0;

        // for OpenCV
        cv::Mat wrappedImage;
        double_t resizeScale = 0.5;

        {
            bitmap = sensorFrame->SoftwareBitmap;

            bitmapBufferReference = 
                bitmap->LockBuffer(
                    Windows::Graphics::Imaging::BitmapBufferAccessMode::Read)->CreateReference();

            byte* bitmapBufferData =
                Io::GetTypedPointerToMemoryBuffer<byte>(
                    bitmapBufferReference,
                    imageBufferSize);
            
            switch (bitmap->BitmapPixelFormat)
            {

                case Windows::Graphics::Imaging::BitmapPixelFormat::Bgra8:
                    wrappedImage = cv::Mat(
                        bitmap->PixelHeight,
                        bitmap->PixelWidth,
                        CV_8UC4,
                        bitmapBufferData);

                    cv::cvtColor(
                        wrappedImage, wrappedImage,
                        cv::COLOR_BGRA2BGR);

                    cv::resize(
                        wrappedImage, wrappedImage,
                        cv::Size(), resizeScale, resizeScale,
                        cv::INTER_LINEAR);
                    
                    break;

                case Windows::Graphics::Imaging::BitmapPixelFormat::Gray16:
                    wrappedImage = cv::Mat(
                        bitmap->PixelHeight,
                        bitmap->PixelWidth,
                        CV_16UC1,
                        bitmapBufferData);

                    break;

                default:
                    wrappedImage = cv::Mat(
                        bitmap->PixelHeight,
                        bitmap->PixelWidth,
                        CV_8UC1,
                        bitmapBufferData);

                    break;
            }

            imageBufferSize = wrappedImage.step * wrappedImage.rows;

            imageBufferAsPlatformArray =
                ref new Platform::Array<byte>(
                    wrappedImage.data,
                    imageBufferSize);
        }

        if (_writeInProgress)
        {
#if DBG_ENABLE_INFORMATIONAL_LOGGING
            dbg::trace(
                L"ROSSensorFrameStreamingServer::Send: image dropped -- previous send operation is in progress!");
#endif /* DBG_ENABLE_INFORMATIONAL_LOGGING */

            return;
        }
        
        _writeInProgress = true;

        {
            /*
            A 64-bit signed integer that represents a point in time 
            as the number of 100-nanosecond intervals
            prior to or after midnight on January 1, 1601 
            (according to the Gregorian Calendar)
            */
            //_writer->WriteInt64(int64_t(sensorFrame->Timestamp.UniversalTime));  // Timestamp
            _writer->WriteInt64(int64_t(currTimestamp.count()));  // Timestamp
            _writer->WriteUInt32(uint32_t(wrappedImage.cols));    // Width
            _writer->WriteUInt32(uint32_t(wrappedImage.rows));    // Height
            _writer->WriteUInt32(uint32_t(wrappedImage.step));    // Number of bytes each matrix row occupies.
            _writer->WriteUInt32(uint32_t(bitmap->BitmapPixelFormat));   // PixelFormat
            WriteFloat4x4(sensorFrame->FrameToOrigin, _writer); // FrameToOrigin (Float4x4)
            WriteFloat4x4(sensorFrame->CameraViewTransform, _writer);   // CameraViewTransform (Float4x4)
            WriteFloat4x4(sensorFrame->CameraProjectionTransform, _writer); // CameraProjectionTransform (Float4x4)
            _writer->WriteBytes(imageBufferAsPlatformArray);    // Image BytesArray
        }

        Concurrency::create_task(_writer->StoreAsync()).then(
            [&](Concurrency::task<unsigned int> writeTask)
            {
                try
                {
                    writeTask.get();
                    _writeInProgress = false;
                }
                catch (Platform::Exception^ exception)
                {
#if DBG_ENABLE_ERROR_LOGGING
                    dbg::trace(
                        L"ROSSensorFrameStreamingServer::SendImage: StoreAsync call failed with error: %s",
                        exception->Message->Data());
#endif /* DBG_ENABLE_ERROR_LOGGING */
                    _socket = nullptr;
                }
            });
    }

    Windows::Foundation::Numerics::float4x4 
        ROSSensorFrameStreamingServer::GetAbsoluteCameraPose(HoloLensForCV::SensorFrame^ frame)
    {
        Windows::Foundation::Numerics::float4x4 interm = Windows::Foundation::Numerics::float4x4::identity();
        //memset(&interm, 0, sizeof(interm));
        //interm.m11 = 1; interm.m22 = 1; interm.m33 = 1; interm.m44 = 1;

        Windows::Foundation::Numerics::float4x4 InvFrameToOrigin;
        Windows::Foundation::Numerics::invert(frame->FrameToOrigin, &InvFrameToOrigin);

        auto pose = interm * frame->CameraViewTransform * InvFrameToOrigin;

        return pose;
    }

    void ROSSensorFrameStreamingServer::WriteFloat4x4(
        Windows::Foundation::Numerics::float4x4 matrix,
        Windows::Storage::Streams::DataWriter^ dataWriter)
    {
        dataWriter->WriteSingle(matrix.m11);
        dataWriter->WriteSingle(matrix.m12);
        dataWriter->WriteSingle(matrix.m13);
        dataWriter->WriteSingle(matrix.m14);
        dataWriter->WriteSingle(matrix.m21);
        dataWriter->WriteSingle(matrix.m22);
        dataWriter->WriteSingle(matrix.m23);
        dataWriter->WriteSingle(matrix.m24);
        dataWriter->WriteSingle(matrix.m31);
        dataWriter->WriteSingle(matrix.m32);
        dataWriter->WriteSingle(matrix.m33);
        dataWriter->WriteSingle(matrix.m34);
        dataWriter->WriteSingle(matrix.m41);
        dataWriter->WriteSingle(matrix.m42);
        dataWriter->WriteSingle(matrix.m43);
        dataWriter->WriteSingle(matrix.m44);
    }
}
