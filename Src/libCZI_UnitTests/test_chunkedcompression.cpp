// SPDX-FileCopyrightText: 2026 Carl Zeiss Microscopy GmbH
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "include_gtest.h"
#include "inc_libCZI.h"

#include <array>
#include <memory>

using namespace libCZI;
using namespace std;

TEST(ChunkedCompression, DecodeTestScenario1)
{
    // we construct a simple chunked-compressed data block manually here, and then decode it with the chunked-compression decoder. 

    static uint8_t data_chunk_1[] = { 1,2,3,4,5 };
    static uint8_t data_chunk_2[] = { 6,7,8,9,10 };
    static uint8_t data_chunk_3[] = { 11,12,13,14,15 };
    static uint8_t data_chunk_4[] = { 16,17,18 };

    static_assert(sizeof(data_chunk_1) == sizeof(data_chunk_2) && sizeof(data_chunk_2) == sizeof(data_chunk_3), "data chunks 1, 2 and 3 must have the same size for this test");

    const auto compressed_chunk_1 = ZstdCompress::CompressZStd0Alloc(sizeof(data_chunk_1), 1, sizeof(data_chunk_1), PixelType::Gray8, data_chunk_1, nullptr);
    const auto compressed_chunk_2 = ZstdCompress::CompressZStd0Alloc(sizeof(data_chunk_2), 1, sizeof(data_chunk_2), PixelType::Gray8, data_chunk_2, nullptr);
    const auto compressed_chunk_3 = ZstdCompress::CompressZStd0Alloc(sizeof(data_chunk_3), 1, sizeof(data_chunk_3), PixelType::Gray8, data_chunk_3, nullptr);
    const auto compressed_chunk_4 = ZstdCompress::CompressZStd0Alloc(sizeof(data_chunk_4), 1, sizeof(data_chunk_4), PixelType::Gray8, data_chunk_4, nullptr);

    ChunkedCompressionHeaderHelper::HeaderInfoForCreation header_info_for_creation;
    header_info_for_creation.codec = ChunkedCompressionHeaderHelper::Codec::ZStd;
    header_info_for_creation.hiLoBytePackingApplied = 0xff;
    header_info_for_creation.chunkSizes =
    {
        static_cast<uint32_t>(compressed_chunk_1->GetSizeOfData()),
        static_cast<uint32_t>(compressed_chunk_2->GetSizeOfData()),
        static_cast<uint32_t>(compressed_chunk_3->GetSizeOfData()),
        static_cast<uint32_t>(compressed_chunk_4->GetSizeOfData())
    };
    header_info_for_creation.uncompressedSizes = { static_cast<uint32_t>(sizeof(data_chunk_1)), static_cast<uint32_t>(sizeof(data_chunk_4)) };

    const size_t max_header_size = ChunkedCompressionHeaderHelper::DetermineMaxSizeForCompressionHeader(header_info_for_creation);

    const auto sub_block_data = std::make_unique<uint8_t[]>(max_header_size + compressed_chunk_1->GetSizeOfData() + compressed_chunk_2->GetSizeOfData() + compressed_chunk_3->GetSizeOfData() + compressed_chunk_4->GetSizeOfData());

    size_t actual_header_size = ChunkedCompressionHeaderHelper::CreateCompressionHeader(sub_block_data.get(), max_header_size, header_info_for_creation);
    ASSERT_LE(actual_header_size, max_header_size) << "Actual header size exceeds the maximum header size";

    memcpy(sub_block_data.get() + actual_header_size, compressed_chunk_1->GetPtr(), compressed_chunk_1->GetSizeOfData());
    memcpy(sub_block_data.get() + actual_header_size + compressed_chunk_1->GetSizeOfData(), compressed_chunk_2->GetPtr(), compressed_chunk_2->GetSizeOfData());
    memcpy(sub_block_data.get() + actual_header_size + compressed_chunk_1->GetSizeOfData() + compressed_chunk_2->GetSizeOfData(), compressed_chunk_3->GetPtr(), compressed_chunk_3->GetSizeOfData());
    memcpy(sub_block_data.get() + actual_header_size + compressed_chunk_1->GetSizeOfData() + compressed_chunk_2->GetSizeOfData() + compressed_chunk_3->GetSizeOfData(), compressed_chunk_4->GetPtr(), compressed_chunk_4->GetSizeOfData());

    const auto decoder = libCZI::GetDefaultSiteObject(SiteObjectType::Default)->GetDecoder(ImageDecoderType::ChunkedCompression, nullptr);
    const auto decoded_bitmap = decoder->Decode(
                                    sub_block_data.get(),
                                    actual_header_size + compressed_chunk_1->GetSizeOfData() + compressed_chunk_2->GetSizeOfData() + compressed_chunk_3->GetSizeOfData() + compressed_chunk_4->GetSizeOfData(),
                                    PixelType::Gray8,
                                    sizeof(data_chunk_1) + sizeof(data_chunk_2) + sizeof(data_chunk_3) + sizeof(data_chunk_4),
                                    1,
                                    nullptr);
    ASSERT_EQ(decoded_bitmap->GetPixelType(), PixelType::Gray8);
    ASSERT_EQ(decoded_bitmap->GetWidth(), sizeof(data_chunk_1) + sizeof(data_chunk_2) + sizeof(data_chunk_3) + sizeof(data_chunk_4));
    ASSERT_EQ(decoded_bitmap->GetHeight(), 1);
    const auto bitmap_lock_info = libCZI::ScopedBitmapLockerSP(decoded_bitmap);
    ASSERT_EQ(memcmp(bitmap_lock_info.ptrDataRoi, data_chunk_1, sizeof(data_chunk_1)), 0) << "Decoded data chunk 1 does not match original data";
    ASSERT_EQ(memcmp(static_cast<const uint8_t*>(bitmap_lock_info.ptrDataRoi) + sizeof(data_chunk_1), data_chunk_2, sizeof(data_chunk_2)), 0) << "Decoded data chunk 2 does not match original data";
    ASSERT_EQ(memcmp(static_cast<const uint8_t*>(bitmap_lock_info.ptrDataRoi) + sizeof(data_chunk_1) + sizeof(data_chunk_2), data_chunk_3, sizeof(data_chunk_3)), 0) << "Decoded data chunk 3 does not match original data";
    ASSERT_EQ(memcmp(static_cast<const uint8_t*>(bitmap_lock_info.ptrDataRoi) + sizeof(data_chunk_1) + sizeof(data_chunk_2) + sizeof(data_chunk_3), data_chunk_4, sizeof(data_chunk_4)), 0) << "Decoded data chunk 4 does not match original data";
}

TEST(ChunkedCompression, EncodeAndDecodeSmallGray8Bitmap)
{
    // we compress a small Gray8 bitmap with the chunked-compression encoder and then decode it again to verify the roundtrip.
    
    constexpr size_t kDestinationBufferSize = 10 * 1024;
    unique_ptr<uint8_t[]> compressed_data_buffer = make_unique<uint8_t[]>(kDestinationBufferSize);
    static constexpr array<uint8_t, 4> source_data = { 1,2,3,4 };

    size_t compressed_data_size = kDestinationBufferSize;
    const bool success = ChunkedCompress::Compress(2, 2, 2, PixelType::Gray8, source_data.data(), compressed_data_buffer.get(), compressed_data_size, nullptr);

    ASSERT_TRUE(success);
    ASSERT_GT(compressed_data_size, 0);
    ASSERT_LE(compressed_data_size, kDestinationBufferSize);

    const auto decoder = libCZI::GetDefaultSiteObject(SiteObjectType::Default)->GetDecoder(ImageDecoderType::ChunkedCompression, nullptr);
    const auto decoded_bitmap = decoder->Decode(
                                    compressed_data_buffer.get(),
                                    compressed_data_size,
                                    PixelType::Gray8,
                                    2,
                                    2,
                                    nullptr);
    ASSERT_EQ(decoded_bitmap->GetPixelType(), PixelType::Gray8);
    ASSERT_EQ(decoded_bitmap->GetWidth(), 2);
    ASSERT_EQ(decoded_bitmap->GetHeight(), 2);
    const auto bitmap_lock_info = libCZI::ScopedBitmapLockerSP(decoded_bitmap);
    ASSERT_EQ(*static_cast<const uint8_t*>(bitmap_lock_info.ptrDataRoi), 1) << "Decoded data does not match original data";
    ASSERT_EQ(*(static_cast<const uint8_t*>(bitmap_lock_info.ptrDataRoi) + 1), 2) << "Decoded data does not match original data";
    ASSERT_EQ(*(static_cast<const uint8_t*>(bitmap_lock_info.ptrDataRoi) + bitmap_lock_info.stride), 3) << "Decoded data does not match original data";
    ASSERT_EQ(*(static_cast<const uint8_t*>(bitmap_lock_info.ptrDataRoi) + bitmap_lock_info.stride + 1), 4) << "Decoded data does not match original data";
}
