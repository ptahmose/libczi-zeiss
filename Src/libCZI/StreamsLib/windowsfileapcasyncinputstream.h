#pragma once
#include <libCZI_Config.h>

#if LIBCZI_WINDOWSAPI_AVAILABLE
#include <string>
#include "../libCZI.h"
#include "../libCZI_Async.h"
#include <Windows.h>

#include "StructurePool.h"

namespace libCZI
{
    namespace detail
    {

        /// Implementation of the IStream-interface for files based on the Win32-API.
        /// It leverages the Win32-API ReadFile passing in an offset, thus allowing for concurrent
        /// access without locking.
        class WindowsFileApcAsyncInputStream : public libCZI::IAsyncInputStream
        {
        private:
            /// This struct holds context for each pending read operation.
            struct PendingOperationData
            {
                OVERLAPPED overlapped;
                std::uint32_t id;
                std::function<void(const AsyncReadRequestResult&)> callback;
                std::shared_ptr<IMemoryBlock> buffer;
                void* user_data;
            };

            StructurePool<PendingOperationData> pending_operations_pool_{ 64,64,1024 };
            HANDLE file_handle_{ INVALID_HANDLE_VALUE };
        public:
            WindowsFileApcAsyncInputStream() = delete;
            explicit WindowsFileApcAsyncInputStream(const wchar_t* filename);
            explicit WindowsFileApcAsyncInputStream(const std::string& filename);
            ~WindowsFileApcAsyncInputStream() override;
        public: // interface libCZI::IAsyncInputStream
            RequestId ReadAsync(const AsyncReadRequest& request) override;
            void Cancel(RequestId request_id) override;

        private:
            static VOID CALLBACK FileIoCompletionRoutine(DWORD dw_error_code, DWORD number_of_bytes_transferred, LPOVERLAPPED overlapped);
            void HandleReadCompletion(DWORD dw_error_code, DWORD number_of_bytes_transferred, PendingOperationData* context);
        };

    }  // namespace detail
}  // namespace libCZI

#endif
