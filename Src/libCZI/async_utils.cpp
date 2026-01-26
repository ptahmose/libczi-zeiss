#include "async_utils.h"

using namespace libCZI;
using namespace libCZI::detail;

namespace
{
    class MemoryBlock : public IMemoryBlock
    {
    private:
        void* ptr;
        size_t sizeOfData;
    public:
        MemoryBlock() = delete;
        MemoryBlock(const MemoryBlock&) = delete;
        MemoryBlock& operator=(const MemoryBlock&) = delete;

        explicit MemoryBlock(size_t initialSize)
            : ptr(nullptr), sizeOfData(0)
        {
            this->ptr = malloc(initialSize);
            this->sizeOfData = initialSize;
        }

        void* GetPtr() override { return this->ptr; }
        size_t GetSizeOfData() const override { return this->sizeOfData; }

        ~MemoryBlock() override { free(this->ptr); }
    };
}

std::shared_ptr<IMemoryBlock> libCZI::detail::CreateMemoryBlock(size_t initialSize)
{
    return std::make_shared<MemoryBlock>(initialSize);
}
