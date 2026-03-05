// SPDX-FileCopyrightText: 2026 Carl Zeiss Microscopy GmbH
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "decoder_chunkedcompression.h"

using namespace std;
using namespace libCZI;
using namespace libCZI::detail;

/*static*/std::shared_ptr<CChunkedCompressionDecoder> CChunkedCompressionDecoder::Create()
{
    return make_shared<CChunkedCompressionDecoder>();
}

std::shared_ptr<libCZI::IBitmapData> CChunkedCompressionDecoder::Decode(const void* ptrData, size_t size, const libCZI::PixelType* pixelType, const std::uint32_t* width, const std::uint32_t* height, const char* additional_arguments)
{
    if (pixelType == nullptr || width == nullptr || height == nullptr)
    {
        throw invalid_argument("pixeltype, width and height must be specified.");
    }


}
