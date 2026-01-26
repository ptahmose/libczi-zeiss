#pragma once

#include "libCZI_Async.h"

namespace libCZI
{
    namespace detail
    {

        class CZIReaderAsync : public libCZI::ICZIReaderAsync
        {
        public:
            CZIReaderAsync();
            ~CZIReaderAsync() override;

            std::shared_ptr<IAsyncAction> Open(const std::shared_ptr<IAsyncInputStream>& stream) override;
            std::shared_ptr<IAsyncOperation<std::shared_ptr< ISubBlock>>> ReadSubBlock(std::uint32_t index) override;
        };

    }
}
