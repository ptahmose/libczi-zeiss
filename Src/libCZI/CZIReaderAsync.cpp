#include "CZIReaderAsync.h"

#include "async_action.h"
#include "CziParse.h"

using namespace libCZI;
using namespace libCZI::detail;

std::shared_ptr<ICZIReaderAsync> libCZI::CreateCZIReaderAsync()
{
    return std::make_shared<CZIReaderAsync>();
}

CZIReaderAsync::CZIReaderAsync()
{
}

CZIReaderAsync::~CZIReaderAsync()
{
}

std::shared_ptr<IAsyncAction> CZIReaderAsync::Open(const std::shared_ptr<IAsyncInputStream>& stream)
{
    auto async_action = std::make_shared<AsyncAction>(nullptr);
    this->open_state = std::make_unique<OpenOperationState>(async_action, stream);

    auto file_header_segment_promise = CCZIParse::ReadFileHeaderSegmentDataAsync(stream);
    file_header_segment_promise->SetCompleted([this](IAsyncOperation<CFileHeaderSegmentData>* op)
        {
            this->OpenHandlerStage1(op);
        });

    return async_action;
}

void CZIReaderAsync::OpenHandlerStage1(IAsyncOperation<CFileHeaderSegmentData>* async_operation)
{
    switch (async_operation->GetStatus())
    {
    case AsyncStatus::Completed:
    {
        this->open_state->file_header_segment_data = async_operation->GetResult();

        // now, we start stage 2 and 3 (read the sub-block directory and the attachment directory, concurrently)
        auto read_subblock_directory_promise = CCZIParse::ReadSubBlockDirectoryAsync(
            this->open_state->stream,
            this->open_state->file_header_segment_data.GetSubBlockDirectoryPosition(),
            /*parse_options=*/CCZIParse::SubblockDirectoryParseOptions());
        read_subblock_directory_promise->SetCompleted([this](IAsyncOperation<CCziSubBlockDirectory>* op)
            {
                this->open_state->async_action->SetDone();
            });
    }
        // now, we start stage 2 and 3 (read the sub-block directory and the attachment directory, concurrently)
        //this->open_state->async_action->SetDone();
        break;
    case AsyncStatus::Error:
        this->open_state->async_action->SetError(async_operation->GetException());
        break;
    case AsyncStatus::Canceled:
        this->open_state->async_action->SetCanceled();
        break;
    }
}

std::shared_ptr<IAsyncOperation<std::shared_ptr< ISubBlock>>> CZIReaderAsync::ReadSubBlock(std::uint32_t index)
{
    throw std::logic_error("Not implemented");
}
