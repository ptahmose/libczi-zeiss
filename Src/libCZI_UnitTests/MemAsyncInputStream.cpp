#include "MemAsyncInputStream.h"

MemAsyncInputStream::~MemAsyncInputStream()
{
}

MemAsyncInputStream::RequestId MemAsyncInputStream::ReadAsync(const libCZI::AsyncReadRequest& request)
{
    std::uint64_t bytes_actually_read;
    this->mem_stream_.Read(
        request.offset,
        request.buffer->GetPtr(),
        request.size,
        &bytes_actually_read);

    // invoke the callback synchronously
    libCZI::AsyncReadRequestResult result;
    result.status = libCZI::AsyncReadRequestResult::Status::Success;
    result.bytes_read = bytes_actually_read;
    result.buffer = request.buffer;
    result.offset = request.offset;
    result.user_data = request.user_data;
    request.callback(result);
    return libCZI::IAsyncInputStream::kRequestIdSynchronousOperation;
}

void MemAsyncInputStream::Cancel(RequestId request_id)
{
    // no-op, as all operations complete synchronously
}

const char* MemAsyncInputStream::GetDataC() const
{
    return this->mem_stream_.GetDataC();
}

size_t MemAsyncInputStream::GetDataSize() const
{
    return this->mem_stream_.GetDataSize();
}

std::shared_ptr<void> MemAsyncInputStream::GetCopy(size_t* pSize) const
{
    return  this->mem_stream_.GetCopy(pSize);
}
