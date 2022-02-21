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

        if (_writeInProgress)
        {
#if DBG_ENABLE_INFORMATIONAL_LOGGING
            dbg::trace(
                L"ROSSensorFrameStreamingServer::Send: image dropped -- previous send operation is in progress!");
#endif /* DBG_ENABLE_INFORMATIONAL_LOGGING */

            return;
        }

#if DBG_ENABLE_INFORMATIONAL_LOGGING
        dbg::TimerGuard timerGuard(
            L"ROSSensorFrameStreamingServer::Send: buffer prepare operation",
            5.0 /* minimum_time_elapsed_in_milliseconds */);
#endif /* DBG_ENABLE_INFORMATIONAL_LOGGING */

        Windows::Graphics::Imaging::SoftwareBitmap^ bitmap;
        Windows::Graphics::Imaging::BitmapBuffer^ bitmapBuffer;
        Windows::Foundation::IMemoryBufferReference^ bitmapBufferReference;

        Platform::Array<uint8_t>^ imageBufferAsPlatformArray;
        uint32_t imageBufferSize = 0;
        

        // for OpenCV
        cv::Mat wrappedImage;
        int32_t wrappedImageType = CV_8UC1;
        int32_t pixelStride = 1;

        {
            bitmap = sensorFrame->SoftwareBitmap;

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
                    pixelStride = 4;
                    wrappedImageType = CV_8UC4;
                    break;

                case Windows::Graphics::Imaging::BitmapPixelFormat::Gray16:
                    pixelStride = 2;
                    wrappedImageType = CV_16UC1;
                    break;

                default:
#if DBG_ENABLE_INFORMATIONAL_LOGGING
                dbg::trace(
                    L"ROSSensorFrameStreamingServer::Send: unrecognized bitmap pixel format, assuming 1 byte per pixel");
#endif /* DBG_ENABLE_INFORMATIONAL_LOGGING */

                    pixelStride = 1;
                    wrappedImageType = CV_8UC1;
                    break;
            }

            wrappedImage = cv::Mat(
                bitmap->PixelHeight,
                bitmap->PixelWidth,
                wrappedImageType,
                bitmapBufferData);

            imageBufferAsPlatformArray =
                ref new Platform::Array<uint8_t>(
                    wrappedImage.data,
                    imageBufferSize);
        }
        
        _writeInProgress = true;

        {
            _writer->WriteUInt64(sensorFrame->Timestamp.UniversalTime);
            _writer->WriteUInt32(bitmap->PixelWidth);
            _writer->WriteUInt32(bitmap->PixelHeight);
            _writer->WriteUInt32(pixelStride);
            _writer->WriteUInt32((uint32_t)imageBufferSize);
            _writer->WriteBytes(imageBufferAsPlatformArray);
        }

        Concurrency::create_task(_writer->StoreAsync()).then(
            [&](Concurrency::task<unsigned int> writeTask)
            {
                try
                {
                    // Try getting an exception.
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

        //SendImage(header,imageBufferAsPlatformArray);
    }

//    void ROSSensorFrameStreamingServer::SendImage(
//        ROSSensorFrameStreamHeader^ header,
//        const Platform::Array<uint8_t>^ imageBytesData)
//    {
//        if (nullptr == _socket)
//        {
//#if DBG_ENABLE_VERBOSE_LOGGING
//            dbg::trace(
//                L"ROSSensorFrameStreamingServer::SendImage: image dropped -- no connection!");
//#endif /* DBG_ENABLE_VERBOSE_LOGGING */
//
//            return;
//        }
//
//        if (_writeInProgress)
//        {
//#if DBG_ENABLE_INFORMATIONAL_LOGGING
//            dbg::trace(
//                L"ROSSensorFrameStreamingServer::SendImage: image dropped -- previous StoreAsync task is still in progress!");
//#endif /* DBG_ENABLE_INFORMATIONAL_LOGGING */
//
//            return;
//        }
//
//        _writeInProgress = true;
//
//        {
//#if DBG_ENABLE_INFORMATIONAL_LOGGING
//            dbg::TimerGuard timerGuard(
//                L"ROSSensorFrameStreamingServer::SendImage: buffer write operation",
//                0.0 /* minimum_time_elapsed_in_milliseconds */);
//#endif /* DBG_ENABLE_INFORMATIONAL_LOGGING */
//
//            //ROSSensorFrameStreamHeader::Write(header,_writer);
//
//            _writer->WriteUInt64(header->Timestamp);
//            _writer->WriteUInt32(header->ImageWidth);
//            _writer->WriteUInt32(header->ImageHeight);
//            _writer->WriteUInt32(header->PixelStride);
//            _writer->WriteUInt32(header->BytesLenght);
//            _writer->WriteBytes(imageBytesData);
//        }
//
//#if DBG_ENABLE_INFORMATIONAL_LOGGING
//        dbg::TimerGuard timerGuard(
//            L"ROSSensorFrameStreamingServer::SendImage: StoreAsync task creation",
//            0.0 /* minimum_time_elapsed_in_milliseconds */);
//#endif /* DBG_ENABLE_INFORMATIONAL_LOGGING */
//
//        Concurrency::create_task(_writer->StoreAsync()).then(
//            [&](Concurrency::task<unsigned int> writeTask)
//            {
//                try
//                {
//                    // Try getting an exception.
//                    writeTask.get();
//
//                    _writeInProgress = false;
//                }
//                catch (Platform::Exception^ exception)
//                {
//#if DBG_ENABLE_ERROR_LOGGING
//                    dbg::trace(
//                        L"ROSSensorFrameStreamingServer::SendImage: StoreAsync call failed with error: %s",
//                        exception->Message->Data());
//#endif /* DBG_ENABLE_ERROR_LOGGING */
//
//                    _socket = nullptr;
//                }
//            });
//    }
}
