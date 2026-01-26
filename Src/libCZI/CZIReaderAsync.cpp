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
    auto file_header_segment_promise = CCZIParse::ReadFileHeaderSegmentDataAsync(stream);
    file_header_segment_promise->SetCompleted([async_action, file_header_segment_promise](IAsyncOperation<CFileHeaderSegmentData>* op)
    {
        try
        {
            // This will throw if the operation failed.
            CFileHeaderSegmentData hdrData = file_header_segment_promise->GetResult();
            // Successfully read the file header segment data.
            async_action->SetDone();
        }
        catch (...)
        {
            async_action->SetError(std::current_exception());
        }
    });

    return async_action;
}

std::shared_ptr<IAsyncOperation<std::shared_ptr< ISubBlock>>> CZIReaderAsync::ReadSubBlock(std::uint32_t index)
{
    throw std::logic_error("Not implemented");
}
