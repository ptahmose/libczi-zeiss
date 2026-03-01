#include "CZIReaderAsync.h"

#include "async_action.h"
#include "CziParse.h"

using namespace std;
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

std::shared_ptr<IAsyncAction> CZIReaderAsync::Open(const std::shared_ptr<IAsyncInputStream>& stream, const ICZIReader::OpenOptions* options)
{
    if (options == nullptr)
    {
        constexpr auto default_options = ICZIReader::OpenOptions{};
        return this->CZIReaderAsync::Open(stream, &default_options);
    }

    // store state derived from options
    switch (options->default_frame_of_reference)
    {
    case CZIFrameOfReference::Invalid:
    case CZIFrameOfReference::Default:
        this->default_frame_of_reference_ = CZIFrameOfReference::RawSubBlockCoordinateSystem;
        break;
    default:
        this->default_frame_of_reference_ = options->default_frame_of_reference;
        break;
    }

    this->sub_block_directory_info_policy_ = options->subBlockDirectoryInfoPolicy;

    // then, start the asynchronous open operation
    auto async_action = std::make_shared<AsyncAction>(stream);
    async_action->SetCancellationRequestedCallback([this]() { this->OpenCancellationHandler(); });
    this->open_state = std::make_unique<OpenOperationState>(async_action, stream);

    this->open_state->read_file_header_segment_operation = CCZIParse::ReadFileHeaderSegmentDataAsync(stream);
    this->open_state->read_file_header_segment_operation->SetCompleted([this](const shared_ptr<IAsyncOperation<CFileHeaderSegmentData>>& op)
        {
            this->OpenHandlerStage1(op);
        });

    return async_action;
}

void CZIReaderAsync::OpenHandlerStage1(const shared_ptr<IAsyncOperation<CFileHeaderSegmentData>>& async_operation)
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
        auto read_attachment_directory_promise = CCZIParse::ReadAttachmentsDirectoryAsync(
            this->open_state->stream,
            this->open_state->file_header_segment_data.GetAttachmentDirectoryPosition());
        read_subblock_directory_promise->SetCompleted([this, read_attachment_directory_promise](const shared_ptr<IAsyncOperation<CCziSubBlockDirectory>>& op)
            {
                switch (op->GetStatus())
                {
                case AsyncStatus::Error:
                case AsyncStatus::Canceled:
                    read_attachment_directory_promise->Cancel();
                    break;
                }

                //this->open_state->async_action->SetDone();
                this->open_state->read_subblock_directory_operation = op;

                this->OpenHandlerStage2(OpenOperationState::kReadySubblockDir);
            });
        read_attachment_directory_promise->SetCompleted([this, read_subblock_directory_promise](const shared_ptr<IAsyncOperation<CCziAttachmentsDirectory>>& op)
            {
                switch (op->GetStatus())
                {
                case AsyncStatus::Error:
                case AsyncStatus::Canceled:
                    read_subblock_directory_promise->Cancel();
                    break;
                }

                //this->open_state->async_action->SetDone();
                this->open_state->read_attachments_directory_operation = op;
                this->OpenHandlerStage2(OpenOperationState::kReadyAttachmentsDir);
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

void CZIReaderAsync::OpenHandlerStage2(std::uint32_t ready_bit)
{
    // This method may be called concurrently from multiple completion callbacks.
    // Only proceed when both dependent operations have been stored.
    const auto previous_mask = this->open_state->stage2_ready_mask.fetch_or(ready_bit, std::memory_order_acq_rel);
    const auto new_mask = previous_mask | ready_bit;
    if (new_mask != (OpenOperationState::kReadySubblockDir | OpenOperationState::kReadyAttachmentsDir))
    {
        return;
    }

    // Both are available now.
    const auto subblock_directory_status = this->open_state->read_subblock_directory_operation->GetStatus();
    const auto attachments_directory_status = this->open_state->read_attachments_directory_operation->GetStatus();

    if (subblock_directory_status == AsyncStatus::Error)
    {
        this->open_state->async_action->SetError(this->open_state->read_subblock_directory_operation->GetException());
        return;
    }

    if (attachments_directory_status == AsyncStatus::Error)
    {
        this->open_state->async_action->SetError(this->open_state->read_attachments_directory_operation->GetException());
        return;
    }

    if (subblock_directory_status == AsyncStatus::Canceled || attachments_directory_status == AsyncStatus::Canceled)
    {
        this->open_state->async_action->SetCanceled();
        return;
    }

    this->CompleteOpenStage();
}

void CZIReaderAsync::CompleteOpenStage()
{
    this->hdrSegmentData_ = std::move(this->open_state->file_header_segment_data);
    this->subBlkDir_ = std::move(this->open_state->read_subblock_directory_operation->GetResult());
    this->attachmentDir_ = std::move(this->open_state->read_attachments_directory_operation->GetResult());
    this->isOperational = true;
  
    this->open_state->async_action->SetDone();
    this->open_state.reset();
}

std::shared_ptr<IAsyncOperation<std::shared_ptr< ISubBlock>>> CZIReaderAsync::ReadSubBlock(std::uint32_t index)
{
    throw std::logic_error("Not implemented");
}

/*virtual*/FileHeaderInfo CZIReaderAsync::GetFileHeaderInfo()
{
    this->ThrowIfNotOperational();
    FileHeaderInfo fhi;
    fhi.fileGuid = this->hdrSegmentData_.GetFileGuid();
    this->hdrSegmentData_.GetVersion(&fhi.majorVersion, &fhi.minorVersion);
    return fhi;
}

void CZIReaderAsync::ThrowIfNotOperational()
{
    // TODO(JBL): implement
}

/*virtual*/SubBlockStatistics CZIReaderAsync::GetStatistics()
{
    this->ThrowIfNotOperational();
    SubBlockStatistics s = this->subBlkDir_.GetStatistics();
    return s;
}

/*virtual*/libCZI::PyramidStatistics CZIReaderAsync::GetPyramidStatistics()
{
    this->ThrowIfNotOperational();
    return this->subBlkDir_.GetPyramidStatistics();
}

void CZIReaderAsync::OpenCancellationHandler()
{
    if (this->open_state->async_action)
    {
        this->open_state->read_file_header_segment_operation->Cancel();
    }

    if (this->open_state->read_file_header_segment_operation)
    {
        this->open_state->read_file_header_segment_operation->Cancel();
    }

    if (this->open_state->read_subblock_directory_operation)
    {
        this->open_state->read_subblock_directory_operation->Cancel();
    }
}
