// SPDX-FileCopyrightText: 2017-2022 Carl Zeiss Microscopy GmbH
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <memory>
#include "libCZI_Pixels.h"
#include "libCZI_compress.h"
#include "libCZI_Site.h"

class CJxrLibDecoder : public libCZI::IDecoder
{
public:
    static std::shared_ptr<CJxrLibDecoder> Create();

    std::shared_ptr<libCZI::IBitmapData> Decode(const void* ptrData, size_t size, const libCZI::PixelType* pixelType, const std::uint32_t* width, const std::uint32_t* height, const char* additional_arguments) override;

    std::shared_ptr<libCZI::IBitmapData> Decode(const void* ptrData, size_t size, libCZI::PixelType pixelType, std::uint32_t width, std::uint32_t height, const char* additional_arguments = nullptr)
    {
        return this->Decode(ptrData, size, &pixelType, &width, &height, additional_arguments);
    }
};
