#pragma once

#include "async_action.h"
#include "CziAttachmentsDirectory.h"
#include "CziSubBlockDirectory.h"
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

                std::shared_ptr<libCZI::IAsyncOperation<CFileHeaderSegmentData>> read_file_header_segment_operation;

                CFileHeaderSegmentData file_header_segment_data;

                static constexpr std::uint32_t kReadySubblockDir = 0x1;
                static constexpr std::uint32_t kReadyAttachmentsDir = 0x2;
                std::atomic<std::uint32_t> stage2_ready_mask{ 0 };

                std::shared_ptr<libCZI::IAsyncOperation<CCziSubBlockDirectory>> read_subblock_directory_operation;
                std::shared_ptr<libCZI::IAsyncOperation<CCziAttachmentsDirectory>> read_attachments_directory_operation;
            };

            std::unique_ptr<OpenOperationState> open_state;

            CFileHeaderSegmentData hdrSegmentData_;
            CCziSubBlockDirectory subBlkDir_;
            CCziAttachmentsDirectory attachmentDir_;
            bool    isOperational;  ///<    If true, then stream, hdrSegmentData and subBlkDir can be considered valid and operational
            libCZI::CZIFrameOfReference default_frame_of_reference_;
            libCZI::ICZIReader::OpenOptions::SubBlockDirectoryInfoPolicy sub_block_directory_info_policy_;
        public:
            CZIReaderAsync();
            ~CZIReaderAsync() override;

            SubBlockStatistics GetStatistics() override;
            PyramidStatistics GetPyramidStatistics() override;
            std::shared_ptr<IAsyncOperation<std::shared_ptr< ISubBlock>>> ReadSubBlock(std::uint32_t index) override;

            std::shared_ptr<IAsyncAction> Open(const std::shared_ptr<IAsyncInputStream>& stream, const ICZIReader::OpenOptions* options) override;
            FileHeaderInfo GetFileHeaderInfo() override;

        private:
            void OpenHandlerStage1(const std::shared_ptr<IAsyncOperation<CFileHeaderSegmentData>>& async_operation);
            void OpenHandlerStage2(std::uint32_t ready_bit);
            void CompleteOpenStage();

            void ThrowIfNotOperational();
            void OpenCancellationHandler();
        };

    }
}
