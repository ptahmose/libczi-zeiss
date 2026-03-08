// SPDX-FileCopyrightText: 2026 Carl Zeiss Microscopy GmbH
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <cstdint>
#include <functional>

#include "libCZI_Pixels.h"

namespace libCZI
{
    namespace detail
    {
        class CompressionUtilities
        {
        public:
            static void CheckSourceBitmapArgumentsAndThrow(std::uint32_t sourceWidth, std::uint32_t sourceHeight, std::uint32_t sourceStride, libCZI::PixelType sourcePixeltype, const void* source);
            static void CheckDestinationArgumentsAndThrow(const void* destination, size_t sizeDestination, size_t minSizeOfDestination);
            static void CheckTempBufferAllocArgumentsAndThrow(const std::function<void* (size_t)>& allocateTempBuffer, const std::function<void(void*)>& freeTempBuffer);
        };
    }
}
