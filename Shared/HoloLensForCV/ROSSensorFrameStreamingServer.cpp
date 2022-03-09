#include "pch.h"
#include "MemoryBuffer.h"

namespace HoloLensForCV
{
    ROSSensorFrameStreamingServer::ROSSensorFrameStreamingServer(
        _In_ Platform::String^ serviceName)
        : _writeInProgress(false)
    {
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
            _writer->ByteOrder = Windows::Storage::Streams::ByteOrder::LittleEndian;
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

        Windows::Graphics::Imaging::SoftwareBitmap^ bitmap;
        Windows::Graphics::Imaging::BitmapBuffer^ bitmapBuffer;
        Windows::Foundation::IMemoryBufferReference^ bitmapBufferReference;
        Windows::Foundation::Numerics::float4x4 cameraPose;

        // Intrinsics
        Windows::Media::Devices::Core::CameraIntrinsics^ cameraIntrinsics;
        Windows::Foundation::Numerics::float4 intrinsics;
        

        Platform::Array<uint8_t>^ imageBufferAsPlatformArray;
        uint32_t imageBufferSize = 0;

        // for OpenCV
        cv::Mat wrappedImage;
        double_t resizeScale = 0.5;

        cameraPose = GetAbsoluteCameraPose(sensorFrame);
//#if DBG_ENABLE_INFORMATIONAL_LOGGING
//        dbg::trace(L"CameraPose:\n%f %f %f %f\n%f %f %f %f\n%f %f %f %f\n%f %f %f %f",
//            cameraPose.m11, cameraPose.m12, cameraPose.m13, cameraPose.m14,
//            cameraPose.m21, cameraPose.m22, cameraPose.m23, cameraPose.m24,
//            cameraPose.m31, cameraPose.m32, cameraPose.m33, cameraPose.m34,
//            cameraPose.m41, cameraPose.m42, cameraPose.m43, cameraPose.m44
//        );
//#endif /* DBG_ENABLE_INFORMATIONAL_LOGGING */

        {
            bitmap = sensorFrame->SoftwareBitmap;

            intrinsics.zero();
            cameraIntrinsics = sensorFrame->CoreCameraIntrinsics;
            if (nullptr != cameraIntrinsics)
            {
                intrinsics.x = resizeScale * cameraIntrinsics->FocalLength.x;
                intrinsics.y = resizeScale * cameraIntrinsics->FocalLength.y;
                intrinsics.z = resizeScale * cameraIntrinsics->PrincipalPoint.x;
                intrinsics.w = resizeScale * cameraIntrinsics->PrincipalPoint.y;
            }
//#if DBG_ENABLE_INFORMATIONAL_LOGGING
//            dbg::trace(L"intrinsics:\n fx=%f\n fy=%f\n ppx=%f\n ppy=%f",
//                intrinsics.x, intrinsics.y, intrinsics.z, intrinsics.w
//            );
//#endif /* DBG_ENABLE_INFORMATIONAL_LOGGING */
            

            bitmapBuffer =
                bitmap->LockBuffer(
                    Windows::Graphics::Imaging::BitmapBufferAccessMode::Read);

            uint8_t* bitmapBufferData =
                Io::GetTypedPointerToMemoryBuffer<uint8_t>(
                    bitmapBuffer->CreateReference(),
                    imageBufferSize);

            switch (bitmap->BitmapPixelFormat)
            {

                case Windows::Graphics::Imaging::BitmapPixelFormat::Bgra8:
                    wrappedImage = cv::Mat(
                        bitmap->PixelHeight,
                        bitmap->PixelWidth,
                        CV_8UC4,
                        bitmapBufferData);

                    cv::resize(
                        wrappedImage, wrappedImage, 
                        cv::Size(), resizeScale, resizeScale, 
                        cv::INTER_LINEAR);

                    cv::cvtColor(
                        wrappedImage, wrappedImage, 
                        cv::COLOR_BGRA2BGR);

                    imageBufferSize = 
                        wrappedImage.total() * wrappedImage.elemSize();
                    
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

            imageBufferAsPlatformArray =
                ref new Platform::Array<uint8_t>(
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
            _writer->WriteUInt64(sensorFrame->Timestamp.UniversalTime);
            _writer->WriteUInt32(wrappedImage.cols);
            _writer->WriteUInt32(wrappedImage.rows);
            _writer->WriteUInt32(wrappedImage.elemSize());
            _writer->WriteUInt32(imageBufferSize);
            _writer->WriteSingle(intrinsics.x); // fx
            _writer->WriteSingle(intrinsics.x); // fy
            _writer->WriteSingle(intrinsics.x); // ppx
            _writer->WriteSingle(intrinsics.x); // ppy
            WriteFloat4x4(cameraPose, _writer); // 
            //WriteFloat4x4(sensorFrame->FrameToOrigin, _writer);
            //WriteFloat4x4(sensorFrame->CameraViewTransform, _writer);
            //WriteFloat4x4(sensorFrame->CameraProjectionTransform, _writer);
            _writer->WriteBytes(imageBufferAsPlatformArray);
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

    Windows::Foundation::Numerics::float4x4 ROSSensorFrameStreamingServer::GetAbsoluteCameraPoseForPV(HoloLensForCV::SensorFrame^ frame) 
    {
        Windows::Foundation::Numerics::float4x4 InvFrameToOrigin;
        Windows::Foundation::Numerics::invert(frame->FrameToOrigin, &InvFrameToOrigin);

        auto pose = frame->CameraProjectionTransform * frame->CameraViewTransform * InvFrameToOrigin;

        return pose;
    }

    Windows::Foundation::Numerics::float4x4 ROSSensorFrameStreamingServer::GetAbsoluteCameraPoseForDepth(HoloLensForCV::SensorFrame^ frame) 
    {
        Windows::Foundation::Numerics::float4x4 interm;
        memset(&interm, 0, sizeof(interm));
        interm.m11 = 1; interm.m22 = 1; interm.m33 = 1; interm.m44 = 1;

        Windows::Foundation::Numerics::float4x4 InvFrameToOrigin;
        Windows::Foundation::Numerics::invert(frame->FrameToOrigin, &InvFrameToOrigin);

        auto pose = interm * frame->CameraViewTransform * InvFrameToOrigin;

        return pose;
    }

    Windows::Foundation::Numerics::float4x4 ROSSensorFrameStreamingServer::GetAbsoluteCameraPose(HoloLensForCV::SensorFrame^ frame)
    {
        Windows::Foundation::Numerics::float4x4 interm;
        memset(&interm, 0, sizeof(interm));
        interm.m11 = 1; interm.m22 = 1; interm.m33 = 1; interm.m44 = 1;

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
