#include "libCZI_compress.h"
#include <stdexcept>

using namespace libCZI;

bool libCZI::CompressionHeaderHelper::WalkCompressionHeader(const void* data, size_t sizeData, const std::function<bool(const CompressionHeaderChunk&)>& callback)
{
    throw std::logic_error("not implemented yet");
}

size_t libCZI::CompressionHeaderHelper::GetCompressionHeaderSize(const void* data, size_t sizeData)
{
    throw std::logic_error("not implemented yet");
}
