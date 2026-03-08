// SPDX-FileCopyrightText: 2026 Carl Zeiss Microscopy GmbH
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "compresscommon.h"
#include <stdexcept>
#include <sstream>

#include "libCZI_Utilities.h"

using namespace std;
using namespace libCZI;

void libCZI::detail::CompressionUtilities::CheckSourceBitmapArgumentsAndThrow(std::uint32_t sourceWidth, std::uint32_t sourceHeight, std::uint32_t sourceStride, libCZI::PixelType sourcePixeltype, const void* source)
{
    if (sourceWidth == 0)
    {
        throw invalid_argument("width must be greater than zero");
    }

    if (sourceHeight == 0)
    {
        throw invalid_argument("height must be greater than zero");
    }

    // note: GetBytesPerPixel will throw (an invalid_argument-exception) in case of an invalid enum value
    if (sourceStride < sourceWidth * Utils::GetBytesPerPixel(sourcePixeltype))
    {
        stringstream ss;
        ss << "stride is illegal, for width=" << sourceWidth << " and pixeltype=" << Utils::PixelTypeToInformalString(sourcePixeltype) << " the minimum stride is "
            << sourceWidth * Utils::GetBytesPerPixel(sourcePixeltype) << " whereas " << sourceStride << " was specified.";
        throw invalid_argument(ss.str());
    }

    if (source == nullptr)
    {
        throw invalid_argument("source must not be null");
    }
}

void libCZI::detail::CompressionUtilities::CheckDestinationArgumentsAndThrow(const void* destination, size_t sizeDestination, size_t minSizeOfDestination)
{
    if (destination == nullptr)
    {
        throw invalid_argument("destination must not be null.");
    }

    if (sizeDestination < minSizeOfDestination)
    {
        stringstream ss;
        ss << "sizeDestination must be greater than or equal to " << minSizeOfDestination << ", whereas " << sizeDestination << " was specified.";
        throw invalid_argument(ss.str());
    }
}

void libCZI::detail::CompressionUtilities::CheckTempBufferAllocArgumentsAndThrow(const std::function<void* (size_t)>& allocateTempBuffer, const std::function<void(void*)>& freeTempBuffer)
{
    if (!allocateTempBuffer)
    {
        throw invalid_argument("A function for allocating temp memory must be given.");
    }

    if (!freeTempBuffer)
    {
        throw invalid_argument("A function for freeing temp memory must be given.");
    }
}
