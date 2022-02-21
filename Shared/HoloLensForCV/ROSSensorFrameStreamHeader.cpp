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

namespace HoloLensForCV
{
    ROSSensorFrameStreamHeader::ROSSensorFrameStreamHeader()
    {
        Timestamp = 0;
        ImageWidth = 0;
        ImageHeight = 0;
        PixelStride = 0;
        BytesLenght = 0;
    }

    /* static */ void ROSSensorFrameStreamHeader::Write(
        _In_ ROSSensorFrameStreamHeader^ header,
        _Inout_ Windows::Storage::Streams::DataWriter^ dataWriter)
    {
#if DBG_ENABLE_INFORMATIONAL_LOGGING
        dbg::TimerGuard timerGuard(
            L"ROSSensorFrameStreamHeader::Write: buffer write operation [header]",
            0.0 /* minimum_time_elapsed_in_milliseconds */);
#endif /* DBG_ENABLE_INFORMATIONAL_LOGGING */

        dataWriter->WriteUInt64(header->Timestamp);
        dataWriter->WriteUInt32(header->ImageWidth);
        dataWriter->WriteUInt32(header->ImageHeight);
        dataWriter->WriteUInt32(header->PixelStride);
        dataWriter->WriteUInt32(header->BytesLenght);
    }
}
