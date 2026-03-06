// SPDX-FileCopyrightText: 2026 Carl Zeiss Microscopy GmbH
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "libCZI_compress.h"
#include <stdexcept>
#include <limits>

using namespace libCZI;
using namespace std;

namespace
{
    /// Parse a 2 byte varint from the given data. The encoding is "MSB varint encoding" (cf. here https://techoverflow.net/2013/01/25/efficiently-encoding-variable-length-integers-in-cc/ 
    /// or https://developpaper.com/explain-the-principle-of-varint-coding-in-detail/):
    ///
    /// \exception	invalid_argument	Thrown when an invalid argument error condition occurs.
    ///
    /// \param 	data		The data.
    /// \param 	sizeData	Information describing the size.
    ///
    /// \returns	A tuple, where the first element is the parsed value, and the second element is the number of bytes consumed from the input data.
    tuple<uint16_t, size_t> Parse2ByteVarInt(const void* data, size_t sizeData)
    {
        if (sizeData < 1)
        {
            throw invalid_argument("sizeData must be at least 1");
        }

        const std::uint8_t* p = static_cast<const std::uint8_t*>(data);
        const std::uint16_t value = p[0] & 0x7F;
        const bool hasMore = (p[0] & 0x80) != 0;
        if (!hasMore)
        {
            return { value, 1 };
        }

        if (sizeData < 2)
        {
            throw invalid_argument("sizeData must be at least 2 when the first byte indicates that more bytes follow");
        }

        return { value | (static_cast<std::uint16_t>(p[1]) << 7), 2 };
    }

    /// Parse a 3 byte varint from the given data. The encoding is "MSB varint encoding" (cf. here https://techoverflow.net/2013/01/25/efficiently-encoding-variable-length-integers-in-cc/ 
    /// or https://developpaper.com/explain-the-principle-of-varint-coding-in-detail/):
    ///
    /// \exception	invalid_argument	Thrown when an invalid argument error condition occurs.
    ///
    /// \param 	data		The data.
    /// \param 	sizeData	Information describing the size.
    ///
    /// \returns	A tuple, where the first element is the parsed value, and the second element is the number of bytes consumed from the input data.
    tuple<uint32_t, size_t> Parse3ByteVarInt(const void* data, size_t sizeData)
    {
        if (sizeData < 1)
        {
            throw invalid_argument("sizeData must be at least 1");
        }

        const std::uint8_t* p = static_cast<const std::uint8_t*>(data);
        std::uint32_t value = p[0] & 0x7F;
        bool hasMore = (p[0] & 0x80) != 0;
        if (!hasMore)
        {
            return { value, 1 };
        }

        if (sizeData < 2)
        {
            throw invalid_argument("sizeData must be at least 2 when the first byte indicates that more bytes follow");
        }

        value |= static_cast<std::uint32_t>(p[1] & 0x7F) << 7;
        hasMore = (p[1] & 0x80) != 0;
        if (!hasMore)
        {
            return { value, 2 };
        }

        if (sizeData < 3)
        {
            throw invalid_argument("sizeData must be at least 3 when the second byte indicates that more bytes follow");
        }

        value |= static_cast<std::uint32_t>(p[2]) << 14;
        return { value, 3 };
    }

    /// Parse a 4 byte varint from the given data. The encoding is "MSB varint encoding" (cf. here https://techoverflow.net/2013/01/25/efficiently-encoding-variable-length-integers-in-cc/ 
    /// or https://developpaper.com/explain-the-principle-of-varint-coding-in-detail/):
    ///
    /// \exception	invalid_argument	Thrown when an invalid argument error condition occurs.
    ///
    /// \param 	data		The data.
    /// \param 	sizeData	Information describing the size.
    ///
    /// \returns	A tuple, where the first element is the parsed value, and the second element is the number of bytes consumed from the input data.
    tuple<uint32_t, size_t> Parse4ByteVarInt(const void* data, size_t sizeData)
    {
        if (sizeData < 1)
        {
            throw invalid_argument("sizeData must be at least 1");
        }

        const std::uint8_t* p = static_cast<const std::uint8_t*>(data);
        std::uint32_t value = p[0] & 0x7F;
        bool hasMore = (p[0] & 0x80) != 0;
        if (!hasMore)
        {
            return { value, 1 };
        }

        if (sizeData < 2)
        {
            throw invalid_argument("sizeData must be at least 2 when the first byte indicates that more bytes follow");
        }

        value |= static_cast<std::uint32_t>(p[1] & 0x7F) << 7;
        hasMore = (p[1] & 0x80) != 0;
        if (!hasMore)
        {
            return { value, 2 };
        }

        if (sizeData < 3)
        {
            throw invalid_argument("sizeData must be at least 3 when the second byte indicates that more bytes follow");
        }

        value |= static_cast<std::uint32_t>(p[2] & 0x7F) << 14;
        hasMore = (p[2] & 0x80) != 0;
        if (!hasMore)
        {
            return { value, 3 };
        }

        if (sizeData < 4)
        {
            throw invalid_argument("sizeData must be at least 4 when the third byte indicates that more bytes follow");
        }

        value |= static_cast<std::uint32_t>(p[3]) << 21;
        return { value, 4 };
    }


    size_t DetermineNumberOfBytesNeededFor4ByteVarInt(uint32_t value)
    {
        // 1 byte: values 0 to 127 (7 bits)
        if (value < (1 << 7))
        {
            return 1;
        }

        // 2 bytes: values 128 to 16383 (14 bits)
        if (value < (1 << 14))
        {
            return 2;
        }

        // 3 bytes: values 16384 to 4194303 (22 bits)
        if (value < (1 << 22))
        {
            return 3;
        }

        // 4 bytes: values 4194304 to 536870911 (29 bits)
        if (value < (1 << 29))
        {
            return 4;
        }

        // Value too large (requires more than 29 bits)
        throw invalid_argument("Value too large for 4-byte varint encoding (max 29 bits)");
    }

    size_t DetermineNumberOfBytesNeededFor3ByteVarInt(uint32_t value)
    {
        // 1 byte: values 0 to 127 (7 bits)
        if (value < (1 << 7))
        {
            return 1;
        }

        // 2 bytes: values 128 to 16383 (14 bits)
        if (value < (1 << 14))
        {
            return 2;
        }

        // 3 bytes: values 16384 to 4194303 (22 bits)
        if (value < (1 << 22))
        {
            return 3;
        }

        // Value too large (requires more than 22 bits)
        throw invalid_argument("Value too large for 3-byte varint encoding (max 22 bits)");
    }

    size_t Write4ByteVarInt(void* destination, size_t sizeDestination, uint32_t value)
    {
        if (sizeDestination < 1)
        {
            throw invalid_argument("sizeDestination must be at least 1");
        }

        std::uint8_t* p = static_cast<std::uint8_t*>(destination);

        // 1 byte: values 0 to 127 (7 bits)
        if (value < (1 << 7))
        {
            p[0] = static_cast<std::uint8_t>(value);
            return 1;
        }

        // 2 bytes: values 128 to 16383 (14 bits)
        if (value < (1 << 14))
        {
            if (sizeDestination < 2)
            {
                throw invalid_argument("sizeDestination must be at least 2");
            }
            p[0] = static_cast<std::uint8_t>((value & 0x7F) | 0x80);
            p[1] = static_cast<std::uint8_t>(value >> 7);
            return 2;
        }

        // 3 bytes: values 16384 to 4194303 (22 bits)
        if (value < (1 << 22))
        {
            if (sizeDestination < 3)
            {
                throw invalid_argument("sizeDestination must be at least 3");
            }
            p[0] = static_cast<std::uint8_t>((value & 0x7F) | 0x80);
            p[1] = static_cast<std::uint8_t>(((value >> 7) & 0x7F) | 0x80);
            p[2] = static_cast<std::uint8_t>(value >> 14);
            return 3;
        }

        // 4 bytes: values 4194304 to 536870911 (29 bits)
        if (value < (1 << 29))
        {
            if (sizeDestination < 4)
            {
                throw invalid_argument("sizeDestination must be at least 4");
            }
            p[0] = static_cast<std::uint8_t>((value & 0x7F) | 0x80);
            p[1] = static_cast<std::uint8_t>(((value >> 7) & 0x7F) | 0x80);
            p[2] = static_cast<std::uint8_t>(((value >> 14) & 0x7F) | 0x80);
            p[3] = static_cast<std::uint8_t>(value >> 21);
            return 4;
        }

        // Value too large (requires more than 29 bits)
        throw invalid_argument("Value too large for 4-byte varint encoding (max 29 bits)");
    }

    /// Write a 3 byte varint to the given destination. The encoding is "MSB varint encoding" (cf. here https://techoverflow.net/2013/01/25/efficiently-encoding-variable-length-integers-in-cc/ 
    /// or https://developpaper.com/explain-the-principle-of-varint-coding-in-detail/):
    ///
    /// \exception	invalid_argument	Thrown when an invalid argument error condition occurs.
    ///
    /// \param [in,out] destination		The destination buffer.
    /// \param 		   sizeDestination	Size of the destination buffer.
    /// \param 		   value		   	The value to encode.
    ///
    /// \returns	The number of bytes written to the destination buffer.
    size_t Write3ByteVarInt(void* destination, size_t sizeDestination, uint32_t value)
    {
        if (sizeDestination < 1)
        {
            throw invalid_argument("sizeDestination must be at least 1");
        }

        std::uint8_t* p = static_cast<std::uint8_t*>(destination);

        // 1 byte: values 0 to 127 (7 bits)
        if (value < (1 << 7))
        {
            p[0] = static_cast<std::uint8_t>(value);
            return 1;
        }

        // 2 bytes: values 128 to 16383 (14 bits)
        if (value < (1 << 14))
        {
            if (sizeDestination < 2)
            {
                throw invalid_argument("sizeDestination must be at least 2");
            }
            p[0] = static_cast<std::uint8_t>((value & 0x7F) | 0x80);
            p[1] = static_cast<std::uint8_t>(value >> 7);
            return 2;
        }

        // 3 bytes: values 16384 to 4194303 (22 bits)
        if (value < (1 << 22))
        {
            if (sizeDestination < 3)
            {
                throw invalid_argument("sizeDestination must be at least 3");
            }
            p[0] = static_cast<std::uint8_t>((value & 0x7F) | 0x80);
            p[1] = static_cast<std::uint8_t>(((value >> 7) & 0x7F) | 0x80);
            p[2] = static_cast<std::uint8_t>(value >> 14);
            return 3;
        }

        // Value too large (requires more than 22 bits)
        throw invalid_argument("Value too large for 3-byte varint encoding (max 22 bits)");
    }
}

bool libCZI::ChunkedCompressionHeaderHelper::WalkCompressionHeader(const void* data, size_t sizeData, const std::function<bool(const CompressionHeaderChunk&)>& callback, size_t* bytes_consumed)
{
    if (data == nullptr)
    {
        throw invalid_argument("data must not be null");
    }

    if (callback == nullptr)
    {
        throw invalid_argument("callback must not be null");
    }

    const std::uint8_t* p = static_cast<const std::uint8_t*>(data);
    size_t offset = 0;
    for (;;)
    {
        const auto id = Parse2ByteVarInt(p + offset, sizeData - offset);
        offset += get<1>(id);
        if (get<0>(id) == static_cast<uint16_t>(HeaderChunkId::EndOfHeader))
        {
            if (bytes_consumed != nullptr)
            {
                *bytes_consumed = offset;
            }

            return true;
        }

        const auto chunkSize = Parse3ByteVarInt(p + offset, sizeData - offset);
        offset += get<1>(chunkSize);
        if (offset > sizeData)
        {
            throw invalid_argument("Invalid chunk size in compression header.");
        }

        CompressionHeaderChunk compression_header_chunk;
        compression_header_chunk.chunkId = get<0>(id);
        compression_header_chunk.chunkSize = get<0>(chunkSize);
        compression_header_chunk.chunkPayload = p + offset;
        compression_header_chunk.chunkPayloadSize = get<0>(chunkSize);
        offset += get<0>(chunkSize);
        const bool b = callback(compression_header_chunk);
        if (!b)
        {
            if (bytes_consumed != nullptr)
            {
                *bytes_consumed = offset;
            }

            return false;
        }
    }
}

size_t libCZI::ChunkedCompressionHeaderHelper::GetCompressionHeaderSize(const void* data, size_t sizeData)
{
    if (sizeData < 1)
    {
        throw invalid_argument("sizeData must be at least 1");
    }

    const uint8_t* p = static_cast<const uint8_t*>(data);
    size_t offset = 0;
    for (;;)
    {
        const tuple<uint16_t, size_t> id = Parse2ByteVarInt(p + offset, sizeData - offset);
        offset += get<1>(id);
        if (get<0>(id) == static_cast<uint16_t>(HeaderChunkId::EndOfHeader))
        {
            return offset;
        }

        const tuple<uint32_t, size_t> chunkSize = Parse3ByteVarInt(p + offset, sizeData - offset);
        offset += get<1>(id);

        // check if the chunk size is valid (i.e. does not exceed the remaining data size)
        if (get<0>(chunkSize) + offset > sizeData)
        {
            throw invalid_argument("Invalid chunk size in compression header.");
        }

        offset += get<0>(chunkSize);
    }
}

size_t libCZI::ChunkedCompressionHeaderHelper::CreateCompressionHeader(void* destination, size_t sizeDestination, const HeaderInfoForCreation& headerInfo)
{
    if (destination == nullptr)
    {
        throw invalid_argument("destination must not be null");
    }

    uint8_t* p = static_cast<uint8_t*>(destination);
    size_t offset = 0;

    // --- id=1: ChunkSizes ---
    if (!headerInfo.chunkSizes.empty())
    {
        // determine the size needed for the chunk sizes (i.e. the size of the chunk payload)
        size_t size_needed_for_offsets = 0;
        for (const uint32_t chunkSize : headerInfo.chunkSizes)
        {
            size_needed_for_offsets += DetermineNumberOfBytesNeededFor4ByteVarInt(chunkSize);
        }

        // check whether the cast in the next statement is safe (and note that DetermineNumberOfBytesNeededFor3ByteVarInt will throw if the value is too large for 3 bytes, 
        // so we do not have to check this here)
        if (size_needed_for_offsets > numeric_limits<uint32_t>::max())
        {
            throw invalid_argument("Size needed for chunk sizes exceeds maximum representable size.");
        }

        // total bytes for this header chunk: id(1) + length-varint + payload
        const size_t length_varint_size = DetermineNumberOfBytesNeededFor3ByteVarInt(static_cast<uint32_t>(size_needed_for_offsets));
        const size_t total_chunk_bytes = 1 + length_varint_size + size_needed_for_offsets;

        if (offset + total_chunk_bytes > sizeDestination)
        {
            throw invalid_argument("Destination buffer is too small for the chunk sizes header chunk.");
        }

        // write id
        *(p + offset) = static_cast<uint8_t>(HeaderChunkId::ChunkSizes);
        ++offset;

        // write payload length
        offset += Write3ByteVarInt(p + offset, sizeDestination - offset, static_cast<uint32_t>(size_needed_for_offsets));

        // write the actual chunk sizes (payload)
        for (const uint32_t chunkSize : headerInfo.chunkSizes)
        {
            offset += Write4ByteVarInt(p + offset, sizeDestination - offset, chunkSize);
        }
    }

    // --- id=3: DecompressedSizes ---
    {
        // The payload is a compact representation of the uncompressed size for each chunk.
        // It is an array of varint-encoded values with the following semantic:
        //   - If there is only one element, it gives the uncompressed size for ALL chunks.
        //   - If there are two elements, the first gives the uncompressed size for all chunks
        //     except the last, and the second gives the uncompressed size of the last chunk.
        //   - In general, values are listed in chunk order, and the second-to-last element
        //     repeats for all earlier chunks not explicitly listed.
        //     E.g. with 5 chunks and [20, 60]: chunk 0–3 = 20, chunk 4 = 60.
        //
        // HeaderInfo::uncompressedSizes encodes this as:
        //   get<0> = uncompressed size for all chunks except the last
        //   get<1> = uncompressed size of the last chunk, or 0 if same as get<0>
        const uint32_t commonSize = get<0>(headerInfo.uncompressedSizes);
        const uint32_t lastSize = get<1>(headerInfo.uncompressedSizes);
        const bool lastChunkDiffers = (lastSize != 0);

        size_t payload_size;
        if (!lastChunkDiffers)
        {
            // all chunks have the same uncompressed size — one value suffices
            payload_size = DetermineNumberOfBytesNeededFor4ByteVarInt(commonSize);
        }
        else
        {
            // two values: common (repeating) size first, then last chunk size
            payload_size = DetermineNumberOfBytesNeededFor4ByteVarInt(commonSize)
                + DetermineNumberOfBytesNeededFor4ByteVarInt(lastSize);
        }

        if (payload_size > numeric_limits<uint32_t>::max())
        {
            throw invalid_argument("Payload size for decompressed sizes exceeds maximum representable size.");
        }

        const size_t length_varint_size = DetermineNumberOfBytesNeededFor3ByteVarInt(static_cast<uint32_t>(payload_size));
        const size_t total_chunk_bytes = 1 + length_varint_size + payload_size;

        if (offset + total_chunk_bytes > sizeDestination)
        {
            throw invalid_argument("Destination buffer is too small for the decompressed sizes header chunk.");
        }

        *(p + offset) = static_cast<uint8_t>(HeaderChunkId::DecompressedSizes);
        ++offset;

        offset += Write3ByteVarInt(p + offset, sizeDestination - offset, static_cast<uint32_t>(payload_size));

        // write common (repeating) size first
        offset += Write4ByteVarInt(p + offset, sizeDestination - offset, commonSize);
        if (lastChunkDiffers)
        {
            // write last chunk's differing size second
            offset += Write4ByteVarInt(p + offset, sizeDestination - offset, lastSize);
        }
    }

    // --- id=2: CompressionMethod ---
    {
        // since zstd is the default (if this header-chunk is not present), we only write this chunk if the codec is something other than zstd
        if (headerInfo.codec != Codec::ZStd)
        {
            if (offset + 3 > sizeDestination)  // id(1) + length(1) + payload(1)
            {
                throw invalid_argument("Destination buffer is too small for the compression method header chunk.");
            }

            *(p + offset) = static_cast<uint8_t>(HeaderChunkId::CompressionMethod);
            ++offset;

            // payload length = 1 byte
            offset += Write3ByteVarInt(p + offset, sizeDestination - offset, 1);

            // payload: codec enum value
            *(p + offset) = static_cast<uint8_t>(headerInfo.codec);
            ++offset;
        }
    }

    // --- EndOfHeader terminator ---
    if (offset + 1 > sizeDestination)
    {
        throw invalid_argument("Destination buffer is too small for the end-of-header marker.");
    }

    *(p + offset) = static_cast<uint8_t>(HeaderChunkId::EndOfHeader);
    ++offset;

    return offset;
}

size_t libCZI::ChunkedCompressionHeaderHelper::DetermineMaxSizeForCompressionHeader(const HeaderInfoForMaxSizeDetermination& headerInfo)
{
    if (headerInfo.number_of_chunks <= 1)
    {
        throw invalid_argument("number_of_chunks must be greater than 1.");
    }

    // For the maximum size, we assume each varint uses its maximum encoding:
    //   - id varint:         1 byte (all current HeaderChunkId values are < 128)
    //   - length varint:     3 bytes max
    //   - chunk size varint: 4 bytes max

    size_t maxSize = 0;

    // --- id=1: ChunkSizes ---
    // id(1) + length(3) + payload(number_of_chunks * 4)
    maxSize += 1 + 3 + static_cast<size_t>(headerInfo.number_of_chunks) * 4;

    // --- id=3: DecompressedSizes ---
    // id(1) + length(3) + payload(at most 2 values, each 4 bytes max)
    maxSize += 1 + 3 + 2 * 4;

    // --- id=2: CompressionMethod ---
    // id(1) + length(3) + payload(1)
    // Only written when codec != ZStd (zstd is the default)
    if (headerInfo.codec != Codec::ZStd)
    {
        maxSize += 1 + 3 + 1;
    }

    // --- EndOfHeader terminator ---
    // The EndOfHeader id is always 0, which is always a single byte in varint encoding.
    maxSize += 1;

    return maxSize;
}

namespace
{
    std::vector<std::uint32_t> GetChunkSizesFromHeader(const ChunkedCompressionHeaderHelper::CompressionHeaderChunk& chunk)
    {
        std::vector<std::uint32_t> chunk_sizes;
        size_t i;
        for (i = 0; i < chunk.chunkPayloadSize;)
        {
            const auto value_and_size = Parse3ByteVarInt(static_cast<const uint8_t*>(chunk.chunkPayload) + i, chunk.chunkPayloadSize - i);
            i += get<1>(value_and_size);
            chunk_sizes.emplace_back(get<0>(value_and_size));
        }

        // check that the total size of the chunk sizes matches the chunk payload size exactly (i.e. that there is no "unused" data in the chunk payload)
        if (i > chunk.chunkPayloadSize)
        {
            throw invalid_argument("Invalid chunk sizes header chunk: payload size does not match the sum of the sizes of the encoded chunk sizes.");
        }

        return  chunk_sizes;
    }

    std::vector<std::uint32_t> GetUncompressedChunkSizesFromHeader(const ChunkedCompressionHeaderHelper::CompressionHeaderChunk& chunk)
    {
        std::vector<std::uint32_t> uncompressed_chunk_sizes;
        size_t i;
        for (i = 0; i < chunk.chunkPayloadSize;)
        {
            const auto value_and_size = Parse4ByteVarInt(static_cast<const uint8_t*>(chunk.chunkPayload) + i, chunk.chunkPayloadSize - i);
            i += get<1>(value_and_size);
            uncompressed_chunk_sizes.emplace_back(get<0>(value_and_size));
        }

        // check that the total size of the chunk sizes matches the chunk payload size exactly (i.e. that there is no "unused" data in the chunk payload)
        if (i > chunk.chunkPayloadSize)
        {
            throw invalid_argument("Invalid decompressed sizes header chunk: payload size does not match the sum of the sizes of the encoded uncompressed sizes.");
        }

        return  uncompressed_chunk_sizes;
    }

    std::vector< ChunkedCompressionHeaderHelper::HeaderInfo::ChunkInfo> GetChunkInfosFromCompressedAndUncompressedChunkSizes(const std::vector<std::uint32_t>& chunk_sizes, const std::vector<std::uint32_t>& uncompressed_chunk_sizes)
    {
        const size_t number_of_chunks = chunk_sizes.size();
        if (number_of_chunks == 0)
        {
            throw invalid_argument("At least one chunk size must be specified in the chunk sizes header chunk.");
        }

        const size_t number_of_uncompressed_sizes = uncompressed_chunk_sizes.size();
        if (number_of_uncompressed_sizes == 0)
        {
            throw invalid_argument("At least one uncompressed size must be specified in the decompressed sizes header chunk.");
        }

        if (number_of_uncompressed_sizes > number_of_chunks)
        {
            throw invalid_argument("Invalid decompressed sizes header chunk: more uncompressed sizes specified than the number of chunks.");
        }

        // the semantic is: 
        // * if there is only one uncompressed size, then this applies to all chunks
        // * if there are two uncompressed sizes, then the first applies to all chunks except the last, and the second applies to the last chunk
        // * if there are more than two uncompressed sizes, then the sizes are listed in chunk order, and the second-to-last element repeats for all earlier chunks not explicitly listed

        vector<ChunkedCompressionHeaderHelper::HeaderInfo::ChunkInfo> result;
        result.reserve(number_of_chunks);

        for (size_t i = 0; i < number_of_chunks; ++i)
        {
            uint32_t uncompressed_size_for_chunk;

            if (number_of_uncompressed_sizes == 1)
            {
                uncompressed_size_for_chunk = uncompressed_chunk_sizes[0];
            }
            else
            {
                // Values are interpreted as a suffix in chunk order.
                // The last value applies to the last chunk. If there are chunks before
                // the explicit suffix, they repeat the second-to-last value.
                const size_t prefix_count = number_of_chunks - number_of_uncompressed_sizes;
                if (i < prefix_count)
                {
                    uncompressed_size_for_chunk = uncompressed_chunk_sizes[number_of_uncompressed_sizes - 2];
                }
                else
                {
                    const size_t explicit_index = i - prefix_count;
                    uncompressed_size_for_chunk = uncompressed_chunk_sizes[explicit_index];
                }
            }

            ChunkedCompressionHeaderHelper::HeaderInfo::ChunkInfo chunk_info;
            chunk_info.compressedSize = chunk_sizes[i];
            chunk_info.uncompressedSize = uncompressed_size_for_chunk;
            result.emplace_back(chunk_info);
        }

        return result;
    }
}

std::tuple<size_t, ChunkedCompressionHeaderHelper::HeaderInfo> ChunkedCompressionHeaderHelper::ParseCompressionHeader(const void* data, size_t sizeData)
{
    std::vector<std::uint32_t> chunk_sizes;
    std::vector<std::uint32_t> uncompressed_sizes;
    Codec codec = Codec::Invalid;

    size_t bytes_consumed = 0;
    ChunkedCompressionHeaderHelper::WalkCompressionHeader(
        data,
        sizeData,
        [&](const CompressionHeaderChunk& chunk) -> bool
        {
            switch (chunk.chunkId)
            {
            case HeaderChunkId::ChunkSizes:
                chunk_sizes = GetChunkSizesFromHeader(chunk);
                break;
            case HeaderChunkId::DecompressedSizes:
                uncompressed_sizes = GetUncompressedChunkSizesFromHeader(chunk);
                break;
            case HeaderChunkId::CompressionMethod:
                if (chunk.chunkPayloadSize != 1)
                {
                    throw invalid_argument("Invalid compression method header chunk: payload size must be exactly 1 byte.");
                }

                codec = static_cast<Codec>(*static_cast<const uint8_t*>(chunk.chunkPayload));
                break;
            default:
                throw invalid_argument("Invalid header chunk ID in compression header.");
            }

            return true;  // continue walking through the header
        },
        &bytes_consumed);

    ChunkedCompressionHeaderHelper::HeaderInfo header_info;
    header_info.chunks = GetChunkInfosFromCompressedAndUncompressedChunkSizes(chunk_sizes, uncompressed_sizes);
    header_info.hiLoBytePackingApplied = false;  // currently, hi-lo byte packing is not supported, so we always set this to false
    switch (codec)
    {
    case Codec::Invalid:
        // if the compression method chunk is not present, we assume zstd (which is the default)
        header_info.codec = Codec::ZStd;
        break;
    case Codec::ZStd:
    case Codec::Lz4:
        header_info.codec = codec;
        break;
    default:
        throw invalid_argument("Invalid codec specified in compression method header chunk.");
    }

    return make_tuple(bytes_consumed, header_info);
}
