#pragma once

#include <cstdlib>
#include "libCZI_compress.h"

namespace libCZI
{
    namespace detail
    {
        std::shared_ptr<libCZI::IMemoryBlock> CreateMemoryBlock(size_t initialSize);
    }
}
