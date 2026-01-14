#pragma once

#include "libCZI.h"
#include "libCZI_compress.h"

#include <cstdint>
#include <functional>
#include <memory>
#include <stdexcept>

namespace libCZI
{
    /// AsyncStatus represents the status of an asynchronous operation.
    enum class AsyncStatus : std::uint8_t
    {
        Started,    ///< The operation is still running (may or may not have produced any internal progress).
        Completed,  ///< The operation has finished successfully.
        Canceled,   ///< The operation finished due to cancellation.
        Error       ///< The operation finished due to a failure.
    };

    /// IAsyncInfo is the base interface for all asynchronous operations, providing methods to
    /// request cancellation and to query the status of the operation.
    /// Invariants:
    /// 1. Status begins as Started.  
    /// 2. It transitions at most once from Started to one terminal state.  
    /// 3. Once terminal, it stays terminal forever.  
    /// 4. A terminal operation is "observable" without races: calling getters repeatedly yields the same outcome.
    class IAsyncInfo
    {
    public:
        virtual ~IAsyncInfo() = default;
        /// Requests cancellation of the operation. This is a best-effort signal: it may be invoked multiple times
        /// and completion may still report any terminal state (Completed, Canceled, Error). No status is returned
        /// directly from this call.
        virtual void Cancel() = 0;
        /// Gets the exception if the operation failed.
        /// Throws LibCZIAsyncOperationInvalidStateException when the operation is not in the Error state.
        /// Multiple calls are allowed and will return the same exception pointer when in the Error state.
        virtual AsyncStatus GetStatus() const = 0;
        virtual std::exception_ptr GetException() const = 0;
    };

    /// IAsyncOperation is used to represent a single-shot, eventually-completing asynchronous operation that produces only
    /// one result (of type TResult) and does not report progress.
    /// The operation
    /// - starts usually immediately upon creation (the caller should not make any assumptions about when the operation starts),  
    /// - transitions through well-defined states (Started, Completed, Canceled, Error),  
    /// - completes exactly once into one of the terminal states (Completed, Canceled or Error),  
    /// - after completion, the result (or error/cancel status) is stable and can be observed repeatedly.  
    ///
    /// \tparam	TResult	Type of the result.
    template <typename TResult>
    class IAsyncOperation : public IAsyncInfo
    {
    public:
        virtual void SetCompleted(const std::function<void(IAsyncOperation*)>& completed_callback) = 0;

        /// Gets the result. This method may only be called once the operation has completed successfully.
        /// If the operation is not in the Completed state, a LibCZIAsyncOperationInvalidStateException is thrown;
        /// the original failure exception is not rethrown here (use GetException when status is Error).
        /// Multiple calls are allowed.
        ///
        /// \returns    The result.
        virtual TResult GetResult() = 0;
    };

    class IAsyncAction : public IAsyncInfo
    {
    public:
        virtual void SetCompleted(const std::function<void(IAsyncAction*)>& completed_callback) = 0;
        /// Validates completion. If the operation is not in the Completed state, a LibCZIAsyncOperationInvalidStateException
        /// is thrown; the original failure exception is not rethrown here (use GetException when status is Error).
        /// Multiple calls are allowed.
        virtual void GetResult() = 0;
    };

    //----------------------------------------------------------------
     
    struct AsyncReadRequestResult
    {
        enum class Status : std::uint8_t
        {
            Success,
            Failure,
            Cancelled
        };

        /// The status of the operation. Only 'Success' indicates that the read completed successfully,
        /// in which case 'bytes_read' indicates how many bytes were actually read.
        Status status;

        /// If 'status' is 'Failure', this contains information about the failure.
        std::exception_ptr failure_info;

        /// The number of bytes read from the stream. Valid only if 'status' is 'Success'.
        std::uint64_t bytes_read;

        /// The offset at which the read was performed.
        std::uint64_t offset;

        std::shared_ptr<libCZI::IMemoryBlock> buffer;

        void* user_data;
    };

    struct AsyncReadRequest
    {
        std::uint64_t offset;
        std::uint64_t size;
        std::shared_ptr<IMemoryBlock> buffer;                           ///< valid until callback fires
        std::function<void(const AsyncReadRequestResult&)> callback;    ///< function pointer
        void* user_data;                                                ///< opaque user pointer
    };

    class IAsyncInputStream
    {
    public:
        typedef std::uint32_t RequestId;
        static constexpr RequestId kRequestIdSynchronousOperation = 0;
        static constexpr RequestId kRequestIdAll = 0xffffffff;

        virtual ~IAsyncInputStream() = default;

        /// Starts an asynchronous read operation. This method either returns immediately, and
        /// the operation completes asynchronously via the callback, or it may throw an exception if setting
        /// up the operation fails. If the operation completes synchronously, the callback is invoked
        /// before this method returns. In this case the return value will be 'kRequestIdSynchronousOperation',
        /// otherwise it will return a stable handle with which the operation can be cancelled (by calling
        /// 'Cancel').
        ///
        /// \param 	request	The request.
        ///
        /// \returns	The asynchronous.
        virtual RequestId ReadAsync(const AsyncReadRequest& request) = 0;

        virtual void Cancel(RequestId request_id) = 0;
    };

    //----------------------------------------------------------------
    
    class LIBCZI_API ICZIReaderAsync
    {
    public:
        virtual std::shared_ptr<IAsyncAction> Open(const std::shared_ptr<IAsyncInputStream>& stream) = 0;
        virtual std::shared_ptr<IAsyncOperation<std::shared_ptr< ISubBlock>>> ReadSubBlock(std::uint32_t index) = 0;

        virtual ~ICZIReaderAsync() = default;
    };

} // namespace libCZI
