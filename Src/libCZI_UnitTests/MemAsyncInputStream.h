#pragma once

#include "inc_libCZI.h"


class MemAsyncInputStream : public libCZI::IAsyncInputStream
{
public:
    MemAsyncInputStream(const void* pv, size_t size);
    ~MemAsyncInputStream() override;

    RequestId ReadAsync(const libCZI::AsyncReadRequest& request) override;

    void Cancel(RequestId request_id) override;
};
