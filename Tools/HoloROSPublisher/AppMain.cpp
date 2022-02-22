//*********************************************************
//
// Copyright (c) Microsoft. All rights reserved.
// This code is licensed under the MIT License (MIT).
// THIS CODE IS PROVIDED *AS IS* WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING ANY
// IMPLIED WARRANTIES OF FITNESS FOR A PARTICULAR
// PURPOSE, MERCHANTABILITY, OR NON-INFRINGEMENT.
//
//*********************************************************

#include "pch.h"
#include "AppMain.h"

using namespace Windows::Foundation;
using namespace Windows::Foundation::Numerics;
using namespace Windows::Networking;
using namespace Windows::Networking::Connectivity;
using namespace Windows::Networking::Sockets;
using namespace Windows::Storage::Streams;
using namespace Windows::Graphics::Holographic;
using namespace Windows::Perception::Spatial;
using namespace Windows::UI::Input::Spatial;
using namespace std::placeholders;

#define RENDER_PREVIEW_HOLOGRAM

// Only enable PhotoVideo & ShortThrowDepth sensors.
std::vector<HoloLensForCV::SensorType> kEnabledSensorTypes = {
    HoloLensForCV::SensorType::PhotoVideo,
    HoloLensForCV::SensorType::ShortThrowToFDepth,
};

namespace HoloROSPublisher
{
    // Loads and initializes application assets when the application is loaded.
    AppMain::AppMain(const std::shared_ptr<Graphics::DeviceResources>& deviceResources)
        : Holographic::AppMainBase(deviceResources)
        , _photoVideoMediaFrameSourceGroupStarted(false)
        , _researchModeMediaFrameSourceGroupStarted(false)
    {
    }

    void AppMain::OnHolographicSpaceChanged(
        Windows::Graphics::Holographic::HolographicSpace^ holographicSpace)
    {
        //
        // Initialize the HoloLens media frame readers
        //
        StartHoloLensMediaFrameSourceGroup();

#ifdef RENDER_PREVIEW_HOLOGRAM
        //
        // Initialize the camera preview hologram.
        //
        _previewRenderer =
            std::make_unique<Rendering::SlateRenderer>(
                _deviceResources);
#endif
    }

    void AppMain::OnSpatialInput(
        _In_ Windows::UI::Input::Spatial::SpatialInteractionSourceState^ pointerState)
    {
    }

    // Updates the application state once per frame.
    void AppMain::OnUpdate(
        _In_ Windows::Graphics::Holographic::HolographicFrame^ holographicFrame,
        _In_ const Graphics::StepTimer& stepTimer)
    {
        dbg::TimerGuard timerGuard(
            L"AppMain::OnUpdate",
            30.0 /* minimum_time_elapsed_in_milliseconds */);

        if (!_photoVideoMediaFrameSourceGroupStarted)
        {
            return;
        }

        HoloLensForCV::SensorFrame^ latestColorFrame =
            _photoVideoMediaFrameSourceGroup->GetLatestSensorFrame(
                HoloLensForCV::SensorType::PhotoVideo);

        if (nullptr == latestColorFrame || 
            _latestColorFrameTimestamp.UniversalTime.Equals(latestColorFrame->Timestamp.UniversalTime))
        {
            return;
        }

        _latestColorFrameTimestamp = latestColorFrame->Timestamp;

#ifdef RENDER_PREVIEW_HOLOGRAM
        //
        // Here, we only render the PhotoVideo frame
        //

        // Initialize the preview texture
        if (nullptr == _previewTexture)
        {
            _previewTexture =
                std::make_shared<Rendering::Texture2D>(
                    _deviceResources,
                    latestColorFrame->SoftwareBitmap->PixelWidth,
                    latestColorFrame->SoftwareBitmap->PixelHeight,
                    DXGI_FORMAT::DXGI_FORMAT_B8G8R8A8_UNORM);
        }

        // Update the preview texture
        {
            void* mappedTexture =
                _previewTexture->MapCPUTexture<void>(
                    D3D11_MAP_WRITE /* mapType */);

            Windows::Graphics::Imaging::SoftwareBitmap^ colorBitmap =
                latestColorFrame->SoftwareBitmap;

            Windows::Graphics::Imaging::BitmapBuffer^ colorBitmapBuffer =
                colorBitmap->LockBuffer(
                    Windows::Graphics::Imaging::BitmapBufferAccessMode::Read);

            uint32_t pixelBufferDataLength = 0;

            uint8_t* pixelBufferData =
                Io::GetTypedPointerToMemoryBuffer<uint8_t>(
                    colorBitmapBuffer->CreateReference(),
                    pixelBufferDataLength);

            ASSERT(0 == memcpy_s(
                mappedTexture,
                pixelBufferDataLength,
                pixelBufferData,
                pixelBufferDataLength));

            _previewTexture->UnmapCPUTexture();
        }
        _previewTexture->CopyCPU2GPU();

        // Update the hologram position to be 2 meters in front of the user.
        //Windows::Graphics::Holographic::HolographicFramePrediction^ prediction;
        Windows::UI::Input::Spatial::SpatialPointerPose^ pointerPose;
        {
            
            holographicFrame->UpdateCurrentPrediction();
            Windows::Graphics::Holographic::HolographicFramePrediction^ prediction =
                holographicFrame->CurrentPrediction;
            pointerPose =
                SpatialPointerPose::TryGetAtTimestamp(
                    _spatialPerception->GetOriginFrameOfReference()->CoordinateSystem,
                    prediction->Timestamp);
        }
        _previewRenderer->PositionHologram(pointerPose);

        _previewRenderer->Update(stepTimer);
#endif // RENDER_PREVIEW_HOLOGRAM
    }

    void AppMain::OnPreRender()
    {
    }

    // Renders the current frame to each holographic camera, according to the
    // current application and spatial positioning state.
    void AppMain::OnRender()
    {
#ifdef RENDER_PREVIEW_HOLOGRAM
        // Draw the sample hologram.
        _previewRenderer->Render(
            _previewTexture);
#endif // RENDER_PREVIEW_HOLOGRAM
    }

    // Notifies classes that use Direct3D device resources that the device resources
    // need to be released before this method returns.
    void AppMain::OnDeviceLost()
    {
        _photoVideoMediaFrameSourceGroupStarted = false;
        _photoVideoMediaFrameSourceGroup = nullptr;

        _researchModeMediaFrameSourceGroupStarted = false;
        _researchModeMediaFrameSourceGroup = nullptr;

#ifdef RENDER_PREVIEW_HOLOGRAM
        _previewRenderer->ReleaseDeviceDependentResources();
        _previewTexture.reset();
#endif // RENDER_PREVIEW_HOLOGRAM
    }

    // Notifies classes that use Direct3D device resources that the device resources
    // may now be recreated.
    void AppMain::OnDeviceRestored()
    {
        StartHoloLensMediaFrameSourceGroup();

#ifdef RENDER_PREVIEW_HOLOGRAM
        _previewRenderer->CreateDeviceDependentResources();
#endif // RENDER_PREVIEW_HOLOGRAM 
    }

	// Called when the application is suspending.
	void AppMain::SaveAppState()
	{
        if (_photoVideoMediaFrameSourceGroup == nullptr ||
            _researchModeMediaFrameSourceGroup == nullptr)
        {
            return;
        }

        concurrency::create_task(_photoVideoMediaFrameSourceGroup->StopAsync()).then([&]()
            {
                concurrency::create_task(_researchModeMediaFrameSourceGroup->StopAsync()).then([&]()
                    {
                        delete _photoVideoMediaFrameSourceGroup;
                        _photoVideoMediaFrameSourceGroup = nullptr;
                        _photoVideoMediaFrameSourceGroupStarted = false;

                        delete _researchModeMediaFrameSourceGroup;
                        _researchModeMediaFrameSourceGroup = nullptr;
                        _researchModeMediaFrameSourceGroupStarted = false;

                        delete _sensorFrameStreamer;
                        _sensorFrameStreamer = nullptr;
                    }).wait();
            }).wait();
	}

	// Called when the application is resuming.
	void AppMain::LoadAppState()
	{
		StartHoloLensMediaFrameSourceGroup();
	}

    void AppMain::StartHoloLensMediaFrameSourceGroup()
    {
        REQUIRES(
            !_photoVideoMediaFrameSourceGroupStarted &&
            !_researchModeMediaFrameSourceGroupStarted &&
            nullptr != _spatialPerception);

        _sensorFrameStreamer =
            ref new HoloLensForCV::ROSSensorFrameStreamer();

        _photoVideoMediaFrameSourceGroup =
            ref new HoloLensForCV::MediaFrameSourceGroup(
                HoloLensForCV::MediaFrameSourceGroupType::PhotoVideoCamera,
                _spatialPerception, _sensorFrameStreamer);

        _researchModeMediaFrameSourceGroup =
            ref new HoloLensForCV::MediaFrameSourceGroup(
                HoloLensForCV::MediaFrameSourceGroupType::HoloLensResearchModeSensors,
                _spatialPerception, _sensorFrameStreamer);

        // Enable each sensor
        for (const auto enabledSensorType : kEnabledSensorTypes)
        {
            if (enabledSensorType == HoloLensForCV::SensorType::PhotoVideo)
            {
                _photoVideoMediaFrameSourceGroup->Enable(enabledSensorType);
            }
            else
            {
                _researchModeMediaFrameSourceGroup->Enable(enabledSensorType);
            }

            _sensorFrameStreamer->Enable(enabledSensorType);
        }

        concurrency::create_task(_photoVideoMediaFrameSourceGroup->StartAsync()).then([&]() 
            {
                _photoVideoMediaFrameSourceGroupStarted = true;
            });

        concurrency::create_task(_researchModeMediaFrameSourceGroup->StartAsync()).then([&]() 
            {
                _researchModeMediaFrameSourceGroupStarted = true;
            });
    }
}
