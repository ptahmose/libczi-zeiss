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

bool libCZI::ChunkedCompressionHeaderHelper::WalkCompressionHeader(const void* data, size_t sizeData, const std::function<bool(const CompressionHeaderChunk&)>& callback)
{
    throw std::logic_error("not implemented yet");
}

size_t libCZI::ChunkedCompressionHeaderHelper::GetCompressionHeaderSize(const void* data, size_t sizeData)
{
    if (sizeData < 1)
    {
        throw invalid_argument("sizeData must be at least 1");
    }

    const std::uint8_t* p = static_cast<const std::uint8_t*>(data);
    size_t offset = 0;
    for (;;)
    {
        const tuple<uint16_t, size_t> id = Parse2ByteVarInt(p + offset, sizeData);
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
    }
}

size_t libCZI::ChunkedCompressionHeaderHelper::CreateCompressionHeader(void* destination, size_t sizeDestination, const HeaderInfo& headerInfo)
{
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
