#pragma once

#include "inc_libCZI.h"
#include "MemInputOutputStream.h"


class MemAsyncInputStream : public libCZI::IAsyncInputStream
{
private:
    CMemInputOutputStream mem_stream_;
public:
    MemAsyncInputStream(const void* pv, size_t size)
        : mem_stream_(pv, size)
    {
    }

    ~MemAsyncInputStream() override;

    RequestId ReadAsync(const libCZI::AsyncReadRequest& request) override;
    void Cancel(RequestId request_id) override;

    const char* GetDataC() const;
    size_t GetDataSize() const;
    std::shared_ptr<void> GetCopy(size_t* pSize) const;
};
