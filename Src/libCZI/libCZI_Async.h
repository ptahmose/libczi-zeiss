#pragma once

#include <cstdint>

namespace libCZI
{

    struct AsyncReadRequestResult
    {
        bool success;
        std::uint64_t bytesRead;

        std::uint64_t offset;
        std::uint64_t size;
        void* buffer;

        void* userData;
    };

    typedef void(*AsyncReadCallback)(const AsyncReadRequestResult& request_result);

    struct AsyncReadRequest
    {
        std::uint64_t offset;
        std::uint64_t size;
        void* buffer;
        AsyncReadCallback callback;
        void* userData;
    };

    class IAsyncInputStream
    {
        virtual ~IAsyncInputStream() = default;

        virtual void ReadAsync(const AsyncReadRequest& request) = 0;

        virtual void Cancel() = 0;
    };


}
