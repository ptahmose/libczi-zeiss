#include "libCZI_compress.h"
#include <stdexcept>

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
    size_t offset = 0;
    if (!headerInfo.chunkSizes.empty())
    {
        size_t size_needed_for_offsets = 0;
        for (const uint32_t chunkSize : headerInfo.chunkSizes)
        {
            size_needed_for_offsets += DetermineNumberOfBytesNeededFor4ByteVarInt(chunkSize);
        }


    }

    return 0;
}
