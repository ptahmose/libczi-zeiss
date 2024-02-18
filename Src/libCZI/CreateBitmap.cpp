// SPDX-FileCopyrightText: 2017-2022 Carl Zeiss Microscopy GmbH
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "bitmapData.h"
#include "Site.h"
#include "libCZI.h"
#include "BitmapOperations.h"
#include "inc_libCZI_Config.h"
#include "CziSubBlock.h"

using namespace libCZI;

static std::shared_ptr<libCZI::IBitmapData> CreateBitmapFromCompressedData_JpgXr(const void* pv, size_t size, libCZI::PixelType pixelType, std::uint32_t width, std::uint32_t height)
{
    const auto dec = GetSite()->GetDecoder(ImageDecoderType::JPXR_JxrLib, nullptr);
    return dec->Decode(pv, size, pixelType, width, height);
}

static std::shared_ptr<libCZI::IBitmapData> CreateBitmapFromSubBlock_JpgXr(ISubBlock* subBlk)
{
    /*auto dec = GetSite()->GetDecoder(ImageDecoderType::JPXR_JxrLib, nullptr);
    const void* ptr; size_t size;
    subBlk->DangerousGetRawData(ISubBlock::MemBlkType::Data, ptr, size);
    SubBlockInfo subBlockInfo = subBlk->GetSubBlockInfo();
    return dec->Decode(ptr, size, subBlockInfo.pixelType, subBlockInfo.physicalSize.w, subBlockInfo.physicalSize.h);*/
    const void* ptr; size_t size;
    subBlk->DangerousGetRawData(ISubBlock::MemBlkType::Data, ptr, size);
    const SubBlockInfo& subBlockInfo = subBlk->GetSubBlockInfo();
    return CreateBitmapFromCompressedData_JpgXr(ptr, size, subBlockInfo.pixelType, subBlockInfo.physicalSize.w, subBlockInfo.physicalSize.h);
}

static std::shared_ptr<libCZI::IBitmapData> CreateBitmapFromCompressedData_ZStd0(const void* pv, size_t size, libCZI::PixelType pixelType, std::uint32_t width, std::uint32_t height)
{
    const auto dec = GetSite()->GetDecoder(ImageDecoderType::ZStd0, nullptr);
    return dec->Decode(pv, size, pixelType, width, height);
}

static std::shared_ptr<libCZI::IBitmapData> CreateBitmapFromSubBlock_ZStd0(ISubBlock* subBlk)
{
    //auto dec = GetSite()->GetDecoder(ImageDecoderType::ZStd0, nullptr);
    const void* ptr; size_t size;
    subBlk->DangerousGetRawData(ISubBlock::MemBlkType::Data, ptr, size);
    const SubBlockInfo& subBlockInfo = subBlk->GetSubBlockInfo();
    return CreateBitmapFromCompressedData_ZStd0(ptr, size, subBlockInfo.pixelType, subBlockInfo.physicalSize.w, subBlockInfo.physicalSize.h);
    //return dec->Decode(ptr, size, subBlockInfo.pixelType, subBlockInfo.physicalSize.w, subBlockInfo.physicalSize.h);
}

static std::shared_ptr<libCZI::IBitmapData> CreateBitmapFromCompressedData_ZStd1(const void* pv, size_t size, libCZI::PixelType pixelType, std::uint32_t width, std::uint32_t height)
{
    const auto dec = GetSite()->GetDecoder(ImageDecoderType::ZStd1, nullptr);
    return dec->Decode(pv, size, pixelType, width, height);
}

static std::shared_ptr<libCZI::IBitmapData> CreateBitmapFromSubBlock_ZStd1(ISubBlock* subBlk)
{
    /*auto dec = GetSite()->GetDecoder(ImageDecoderType::ZStd1, nullptr);
    const void* ptr; size_t size;
    subBlk->DangerousGetRawData(ISubBlock::MemBlkType::Data, ptr, size);
    SubBlockInfo subBlockInfo = subBlk->GetSubBlockInfo();

    return dec->Decode(ptr, size, subBlockInfo.pixelType, subBlockInfo.physicalSize.w, subBlockInfo.physicalSize.h);*/
    const void* ptr; size_t size;
    subBlk->DangerousGetRawData(ISubBlock::MemBlkType::Data, ptr, size);
    const SubBlockInfo& subBlockInfo = subBlk->GetSubBlockInfo();
    return CreateBitmapFromCompressedData_ZStd1(ptr, size, subBlockInfo.pixelType, subBlockInfo.physicalSize.w, subBlockInfo.physicalSize.h);
}

static std::shared_ptr<libCZI::IBitmapData> CreateBitmapFromSubBlock_Uncompressed(ISubBlock* subBlk)
{
    size_t size;
    CSharedPtrAllocator sharedPtrAllocator(subBlk->GetRawData(ISubBlock::MemBlkType::Data, &size));

    const auto& sbBlkInfo = subBlk->GetSubBlockInfo();

    // The stride with an uncompressed bitmap in CZI is exactly the linesize.
    const std::uint32_t stride = sbBlkInfo.physicalSize.w * CziUtils::GetBytesPerPel(sbBlkInfo.pixelType);
    if (static_cast<size_t>(stride) * sbBlkInfo.physicalSize.h > size)
    {
        throw std::logic_error("insufficient size of subblock");
    }

    auto sb = CBitmapData<CSharedPtrAllocator>::Create(
        sharedPtrAllocator,
        sbBlkInfo.pixelType,
        sbBlkInfo.physicalSize.w,
        sbBlkInfo.physicalSize.h,
        stride);

#if LIBCZI_ISBIGENDIANHOST
    if (!CziUtils::IsPixelTypeEndianessAgnostic(subBlk->GetSubBlockInfo().pixelType))
    {
        return CBitmapOperations::ConvertToBigEndian(sb.get());
    }
#endif

    return sb;
}

std::shared_ptr<libCZI::IBitmapData> libCZI::CreateBitmapFromSubBlock(ISubBlock* subBlk)
{
    switch (subBlk->GetSubBlockInfo().GetCompressionMode())
    {
    case CompressionMode::JpgXr:
        return CreateBitmapFromSubBlock_JpgXr(subBlk);
    case CompressionMode::Zstd0:
        return CreateBitmapFromSubBlock_ZStd0(subBlk);
    case CompressionMode::Zstd1:
        return CreateBitmapFromSubBlock_ZStd1(subBlk);
    case CompressionMode::UnCompressed:
        return CreateBitmapFromSubBlock_Uncompressed(subBlk);
    default:    // silence warnings
        throw std::logic_error("The method or operation is not implemented.");
    }
}

std::shared_ptr<libCZI::IBitmapData> libCZI::CreateBitmapFromCompressedData(
        libCZI::CompressionMode compression_mode,
        const void* pv,
        size_t size,
        libCZI::PixelType pixelType,
        std::uint32_t width,
        std::uint32_t height)
{
    switch (compression_mode)  // NOLINT(clang-diagnostic-switch-enum)
    {
    case CompressionMode::JpgXr:
        return CreateBitmapFromCompressedData_JpgXr(pv, size, pixelType, width, height);
    case CompressionMode::Zstd0:
        return CreateBitmapFromCompressedData_ZStd0(pv, size, pixelType, width, height);
    case CompressionMode::Zstd1:
        return CreateBitmapFromCompressedData_ZStd1(pv, size, pixelType, width, height);
    default:
        throw std::logic_error("The specified compression mode is not supported or implemented.");
    }
}
