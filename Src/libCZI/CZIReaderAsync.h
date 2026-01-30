#pragma once

#include "async_action.h"
#include "FileHeaderSegmentData.h"
#include "libCZI_Async.h"

namespace libCZI
{
    namespace detail
    {

        class CZIReaderAsync : public libCZI::ICZIReaderAsync
        {
        private:
            struct OpenOperationState
            {
                enum class Stage
                {
                    Virgin,
                    ReadFileHeaderSegment,
                    Done
                };

                OpenOperationState(std::shared_ptr<AsyncAction> action, std::shared_ptr<IAsyncInputStream> stream) : async_action(std::move(action)), stream(std::move(stream))
                {
                }

                std::shared_ptr<AsyncAction> async_action;
                std::shared_ptr<IAsyncInputStream> stream;

                Stage stage{Stage::Virgin};

                CFileHeaderSegmentData file_header_segment_data;
            };

            std::unique_ptr<OpenOperationState> open_state;
        public:
            CZIReaderAsync();
            ~CZIReaderAsync() override;

            std::shared_ptr<IAsyncAction> Open(const std::shared_ptr<IAsyncInputStream>& stream) override;
            std::shared_ptr<IAsyncOperation<std::shared_ptr< ISubBlock>>> ReadSubBlock(std::uint32_t index) override;

        private:
            void OpenHandlerStage1(IAsyncOperation<CFileHeaderSegmentData>* async_operation);
        };

    }
}
