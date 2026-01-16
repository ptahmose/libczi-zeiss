#include "windowsfileapcasyncinputstream.h"

#if LIBCZI_WINDOWSAPI_AVAILABLE
#include <limits>
#include <iomanip>
#include <functional>
#include <sstream>
#include <exception>
#include "../utilities.h"

using namespace libCZI;
using namespace libCZI::detail;
using namespace std;

namespace
{
    std::string WindowsGetSystemErrorAsText(DWORD error_code)
    {
        LPSTR lpMsgBuf;
        BOOL B = FormatMessageA(
            FORMAT_MESSAGE_ALLOCATE_BUFFER |
            FORMAT_MESSAGE_FROM_SYSTEM |
            FORMAT_MESSAGE_IGNORE_INSERTS,
            NULL,
            error_code,
            MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
            (LPSTR)&lpMsgBuf,
            0, NULL);

        string error_text = lpMsgBuf;
        LocalFree(lpMsgBuf);

        // Strip trailing CR/LF left over from FormatMessageA so callers get clean text.
        while (!error_text.empty() && (error_text.back() == '\r' || error_text.back() == '\n'))
        {
            error_text.pop_back();
        }

        return error_text;
    }
}

WindowsFileApcAsyncInputStream::WindowsFileApcAsyncInputStream(const std::string& filename)
    : WindowsFileApcAsyncInputStream(Utilities::convertUtf8ToWchar_t(filename.c_str()).c_str())
{
}

WindowsFileApcAsyncInputStream::WindowsFileApcAsyncInputStream(const wchar_t* filename)
{
    const HANDLE file_handle = CreateFileW(
        filename, 
        GENERIC_READ, 
        FILE_SHARE_READ, 
        NULL, 
        OPEN_EXISTING, 
        FILE_FLAG_OVERLAPPED, 
        NULL);
    if (file_handle == INVALID_HANDLE_VALUE)
    {
        const DWORD error = GetLastError();
        std::stringstream ss;
        ss << "Error opening the file \"" << Utilities::convertWchar_tToUtf8(filename)
           << "\": LastError=0x" << std::hex << std::uppercase << std::setw(8) << std::setfill('0') << error;
        throw std::runtime_error(ss.str());
    }

    this->file_handle_ = file_handle;
}

WindowsFileApcAsyncInputStream::~WindowsFileApcAsyncInputStream()
{
    if (this->file_handle_ != INVALID_HANDLE_VALUE)
    {
        CloseHandle(this->file_handle_);
    }
}

WindowsFileApcAsyncInputStream::RequestId WindowsFileApcAsyncInputStream::ReadAsync(const AsyncReadRequest& request)
{
    if (!request.callback)
    {
        throw invalid_argument("request.callback must be non-null");
    }

    if (!request.buffer)
    {
        throw invalid_argument("A non-null buffer object must be given");
    }

    if (request.buffer->GetSizeOfData() < request.size)
    {
        throw invalid_argument("The size of the specified buffer object must be larger than the requested read size");
    }

    auto id_and_pointer = this->pending_operations_pool_.Add(PendingOperationData{});
    PendingOperationData* context = std::get<1>(id_and_pointer);
    context->id = std::get<0>(id_and_pointer);
    context->overlapped.hEvent = this;
    context->overlapped.Offset = static_cast<DWORD>(request.offset);
    context->overlapped.OffsetHigh = static_cast<DWORD>(request.offset >> 32);
    context->user_data = request.user_data;
    context->buffer = request.buffer;
    context->callback = request.callback;

    BOOL ok = ReadFileEx(
        this->file_handle_,
        request.buffer->GetPtr(),
        static_cast<DWORD>(request.size),
        &context->overlapped,
        WindowsFileApcAsyncInputStream::FileIoCompletionRoutine);

    // also in case of "SUCCESS", we shall check GetLastError() - c.f. https://learn.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-readfileex?devlangs=cpp&f1url=%3FappId%3DDev18IDEF1%26l%3DEN-US%26k%3Dk(FILEAPI%2FReadFileEx)%3Bk(ReadFileEx)%3Bk(DevLang-C%2B%2B)%3Bk(TargetOS-Windows)%26rd%3Dtrue
    DWORD last_error = GetLastError();
    if (!ok || last_error != ERROR_SUCCESS)
    {
        // remove the pending operation context
        this->pending_operations_pool_.TryGetAndRemove(context->id, nullptr);
        throw std::runtime_error("ReadFileEx failed: " + std::to_string(GetLastError()));
    }

    return context->id;
}

/*static*/VOID CALLBACK WindowsFileApcAsyncInputStream::FileIoCompletionRoutine(DWORD dw_error_code, DWORD number_of_bytes_transferred, LPOVERLAPPED overlapped)
{
    PendingOperationData* context = reinterpret_cast<PendingOperationData*>(overlapped);
    WindowsFileApcAsyncInputStream* self = static_cast<WindowsFileApcAsyncInputStream*>(overlapped->hEvent);
    self->HandleReadCompletion(dw_error_code, number_of_bytes_transferred, context);
}

void WindowsFileApcAsyncInputStream::HandleReadCompletion(DWORD dw_error_code, DWORD number_of_bytes_transferred, PendingOperationData* context)
{
    AsyncReadRequestResult read_request_result;
    read_request_result.user_data = context->user_data;
    read_request_result.buffer = context->buffer;
    read_request_result.offset =
        (static_cast<std::uint64_t>(context->overlapped.OffsetHigh) << 32) |
        static_cast<std::uint64_t>(context->overlapped.Offset);

    if (dw_error_code == 0)
    {
        read_request_result.status = AsyncReadRequestResult::Status::Success;
        read_request_result.bytes_read = number_of_bytes_transferred;
    }
    else if (dw_error_code == ERROR_OPERATION_ABORTED)
    {
        read_request_result.status = AsyncReadRequestResult::Status::Cancelled;
        read_request_result.bytes_read = 0;
    }
    else
    {
        ostringstream string_stream;
        string_stream << "Asynchronous read failed with error code 0x"
            << std::hex << std::setfill('0') << std::setw(8) << dw_error_code
            << std::dec << " (" << WindowsGetSystemErrorAsText(dw_error_code) << ")";
        read_request_result.failure_info = std::make_exception_ptr(std::runtime_error(string_stream.str()));
        read_request_result.status = AsyncReadRequestResult::Status::Failure;
        read_request_result.bytes_read = 0;
    }


    const std::uint32_t id = context->id;

    const auto callback = context->callback;

    const bool b = this->pending_operations_pool_.TryGetAndRemove(id, nullptr);

    callback(read_request_result);
}

void WindowsFileApcAsyncInputStream::Cancel(RequestId request_id) override
{
    if (request_id == IAsyncInputStream::kRequestIdAll)
    {
        // Cancel all pending operations
        BOOL ok = CancelIoEx(this->file_handle_, nullptr);
        if (!ok)
        {
            throw std::runtime_error("CancelIoEx failed: " + std::to_string(GetLastError()));
        }
    }
    else
    {
        // TODO(JBL): I guess there is a race condition here (if the operation completes between the
        //             lookup and the CancelIoEx call). Need to verify and fix if so.
        PendingOperationData* context = this->pending_operations_pool_.Get(request_id);
        if (context == nullptr)
        {
            throw invalid_argument("Invalid request_id");
        }

        const BOOL ok = CancelIoEx(this->file_handle_, &context->overlapped);
        if (!ok)
        {
            // We don't want to throw if the operation already completed
            if (GetLastError() != ERROR_NOT_FOUND)
            {
                throw std::runtime_error("CancelIoEx failed: " + std::to_string(GetLastError()));
            }
        }
    }
}
#endif
