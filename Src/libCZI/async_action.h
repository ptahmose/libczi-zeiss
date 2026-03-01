#pragma once

#include <atomic>
#include <stdexcept>
#include <functional>
#include <exception>
#include <memory>
#include <utility>

#include "libCZI_Async.h"

namespace libCZI
{
    namespace detail
    {
        /// \brief Base class for implementing asynchronous operations.
        ///
        /// This class encapsulates the state management (AsyncStatus), error handling, completion callback
        /// invocation and cancellation signalling logic common to both void-returning actions and
        /// result-returning operations.
        ///
        /// \par Cancellation model
        /// - Calling CancelCore() is a best-effort signal only. It does not transition \ref async_status_.
        /// - A producer may install a cancellation callback later via SetCancellationRequestedCallback().
        /// - If CancelCore() is called before the callback is installed, the request is latched and the
        ///   callback will be invoked immediately when it is later installed.
        /// - The cancellation callback is invoked at most once.
        ///
        /// \par Thread-safety
        /// - CancelCore() may run concurrently with SetCancellationRequestedCallback() and with all completion
        ///   related producer/consumer calls.
        /// - To avoid data races on \c std::function, the cancellation callback is published as an immutable
        ///   heap object behind a \c std::shared_ptr. Publication uses \c std::atomic_store/_load for shared_ptr
        ///   (available as free functions in C++11/14).
        ///
        /// \par Memory-ordering rationale (completion)
        /// - \ref completion_reserved_ is the single-writer guard for status/payload transitions; status stores
        ///   use \c memory_order_release and readers use \c memory_order_acquire.
        /// - \ref callback_ready_ is release-stored after writing the callback; producers acquire-load it before
        ///   invoking NotifyCompletedBase() to ensure callback visibility.
        /// - \ref invoked_ is a once-flag around callback invocation.
        /// - \c seq_cst fences after terminal writes are a conservative barrier to ensure payload/status
        ///   visibility across threads.
        ///
        /// \par Memory-ordering rationale (cancellation)
        /// - \ref cancel_requested_ is a latch set by CancelCore(). The producer checks it when installing the
        ///   callback.
        /// - \ref cancellation_requested_ is published with release-store and read with acquire-load.
        /// - \ref cancel_callback_invoked_ is the cross-thread once-flag that ensures the callback is only
        ///   invoked once, regardless of whether CancelCore() or SetCancellationRequestedCallback() wins the
        ///   race.
        class AsyncStateBase
        {
        private:
            std::shared_ptr<libCZI::IEventLoop> event_loop_;
        protected:
            /// \brief Represents the state of the operation (Started, Completed, Canceled, Error).
            ///
            /// Transitions are guarded by \ref completion_reserved_ to ensure single-writer semantics.
            std::atomic<libCZI::AsyncStatus> async_status_{ libCZI::AsyncStatus::Started };

            /// \brief Cancellation callback published by the producer.
            ///
            /// This points to an immutable \c std::function object allocated by the producer at the time
            /// SetCancellationRequestedCallback() is called.
            ///
            /// The indirection is used to provide thread-safe publication without locks: \c std::function is
            /// not safe for concurrent read/write, but publishing a fully-constructed function behind a
            /// \c std::shared_ptr is safe when the pointer itself is published via atomic store/load.
            std::shared_ptr<const std::function<void()>> cancellation_requested_;

            /// \brief Latches whether cancellation has been requested.
            ///
            /// Set to true by CancelCore(). If the producer installs the cancellation callback afterwards,
            /// it will be invoked immediately.
            std::atomic<bool> cancel_requested_{ false };

            /// \brief Ensures the cancellation callback is invoked at most once.
            ///
            /// Either CancelCore() or SetCancellationRequestedCallback() may attempt to invoke the callback,
            /// depending on timing. This flag is the cross-thread once-guard.
            std::atomic<bool> cancel_callback_invoked_{ false };

            /// \brief Ensures the cancellation callback can only be set once.
            std::atomic<bool> cancel_callback_set_{ false };

            // Stores exception information if the operation fails.
            // Protected by completion_reserved_ during write; safe to read if async_status_ == Error.
            std::exception_ptr error_;

            // Guards the actual invocation of the callback to ensure it runs exactly once.
            // This flag resolves the race between the producer (setting done) and the consumer (setting callback).
            std::atomic<bool> invoked_{ false };

            // Ensures that SetCompleted is called only once by the consumer.
            std::atomic<bool> callback_set_reserved_{ false };

            // Ensures that SetDone/SetCanceled/SetError transition happens only once.
            // Serves as a write-lock for transitioning state and writing payload (e.g. error_).
            std::atomic<bool> completion_reserved_{ false };

            // Indicates that completed_callback_ has been written and is safe to read.
            // Used with release-store by consumer and acquire-load by producer.
            std::atomic<bool> callback_ready_{ false };

            virtual void OnNotifyCompleted() = 0;

        public:
            /// \brief Initializes a new instance.
            ///
            /// After construction, the cancellation callback is not installed. Producers should call
            /// SetCancellationRequestedCallback() exactly once.
            AsyncStateBase();

            AsyncStateBase(std::shared_ptr<libCZI::IEventLoop> event);

            /// \brief Installs the cancellation callback (producer API).
            ///
            /// This method may be called after construction and it can only be called once.
            ///
            /// \param cancellation_requested The callback to invoke when cancellation is requested.
            ///        The callback must be non-empty.
            ///
            /// \throws std::invalid_argument
            ///         If \p cancellation_requested is empty.
            /// \throws LibCZIAsyncOperationInvalidStateException
            ///         If called more than once.
            ///
            /// \note If CancelCore() has already been called, this method will invoke the callback
            /// immediately (exactly once), on the calling thread.
            void SetCancellationRequestedCallback(std::function<void()> cancellation_requested);

            /// Finalizes an instance of the AsyncStateBase class.
            virtual ~AsyncStateBase();

            /// \brief Requests cancellation (consumer API).
            ///
            /// This method is best-effort and may be called multiple times.
            ///
            /// - The cancellation request is latched via \ref cancel_requested_.
            /// - If the cancellation callback is already installed and has not yet been invoked, it will be
            ///   invoked (at most once).
            /// - If the cancellation callback is installed later, it will be invoked immediately at
            ///   installation time.
            void CancelCore();

            /// Gets the current status of the operation.
            /// \return The current status.
            libCZI::AsyncStatus GetStatusCore() const;

            /// Gets the exception if the operation failed.
            /// \return The exception pointer, or null if no error occurred.
            std::exception_ptr GetExceptionCore() const;

            /// \brief Transitions the operation to the Canceled state (producer API).
            ///
            /// This method completes the operation with status Canceled.
            /// If the completion callback was already registered by the consumer, it will be invoked.
            ///
            /// \throws LibCZIAsyncOperationInvalidStateException
            ///         If the operation has already transitioned out of Started.
            void SetCanceled();

            /// \brief Transitions the operation to the Error state (producer API).
            ///
            /// Stores the provided exception pointer and completes the operation with status Error.
            /// If the completion callback was already registered by the consumer, it will be invoked.
            ///
            /// \param exception_ptr The exception indicating the failure.
            ///
            /// \throws LibCZIAsyncOperationInvalidStateException
            ///         If the operation has already transitioned out of Started.
            void SetError(std::exception_ptr exception_ptr);

            void WaitForCompletion();

        protected:
            void SetCompletedPrepare();
            void SetCompletedFinish();
            void SetDoneBase();
            void NotifyCompletedBase();
        };

        /// Implementation of the IAsyncAction interface.
        /// Represents an asynchronous action that does not return a value.
        class AsyncAction : public libCZI::IAsyncAction, public AsyncStateBase, public std::enable_shared_from_this<AsyncAction>
        {
        private:
            // The callback provided by the consumer.
            // Safe to access only when callback_ready_ == true (seen via acquire load) or if the current thread wrote it.
            std::function<void(const std::shared_ptr<libCZI::IAsyncAction>&)> completed_callback_;

        public:
            /// Initializes a new instance of the AsyncAction class.
            AsyncAction();
            
            AsyncAction(std::shared_ptr<libCZI::IEventLoop> event) : AsyncStateBase(event) {};

            // Forward IAsyncInfo methods to AsyncStateBase
            void Cancel() override { this->CancelCore(); }
            libCZI::AsyncStatus GetStatus() const override { return this->GetStatusCore(); }
            std::exception_ptr GetException() const override { return this->GetExceptionCore(); }

            /// Sets the completion callback.
            /// \param completed_callback The callback to be invoked when the operation completes.
            void SetCompleted(const std::function<void(const std::shared_ptr<libCZI::IAsyncAction>&)>& completed_callback) override;

            /// Gets the result of the action.
            /// Since this is a void action, this method only validates that the operation has completed successfully.
            /// Throws an exception if the operation is not completed or failed.
            void GetResult() override;

            /// Marks the action as successfully completed.
            /// This should be called by the producer.
            void SetDone();

            // Expose SetCanceled and SetError from base (or just call them)
            // But strict implementation of interface requires them to match logic? 
            // AsyncStateBase has SetCanceled/SetError which do exactly what AsyncAction needs.
            // Wait, SetDone/SetCanceled/SetError are methods on AsyncAction in previous code?
            // Yes, SetDone/SetCanceled/SetError were public methods on AsyncAction but they are not part of IAsyncAction interface.
            // IAsyncInfo only has GetStatus/GetException/Cancel.
            // IAsyncAction has GetResult and SetCompleted.
            // SetDone/SetCanceled/SetError are implementation methods for the producer.
            // So we can expose them directly from base via using or wrapper.
            using AsyncStateBase::SetCanceled;
            using AsyncStateBase::SetError;
            
            void WaitForCompletion() override { AsyncStateBase::WaitForCompletion(); }

        protected:
            void OnNotifyCompleted() override;
        };

        /// Implementation of the IAsyncOperation<TResult> interface.
        /// Represents an asynchronous operation that returns a value of type TResult.
        /// \tparam TResult The type of the result produced by the operation.
        template <typename TResult>
        class AsyncOperation : public libCZI::IAsyncOperation<TResult>, public AsyncStateBase, public std::enable_shared_from_this<AsyncOperation<TResult>>
        {
        private:
            std::function<void(const std::shared_ptr<libCZI::IAsyncOperation<TResult>>&)> completed_callback_;
            TResult result_{};

        public:
            /// Initializes a new instance of the AsyncOperation class.
            AsyncOperation() = default;

            AsyncOperation(std::shared_ptr<libCZI::IEventLoop> event) : AsyncStateBase(event) {};

            // Forward IAsyncInfo methods
            void Cancel() override { this->CancelCore(); }
            libCZI::AsyncStatus GetStatus() const override { return this->GetStatusCore(); }
            std::exception_ptr GetException() const override { return this->GetExceptionCore(); }

            // Expose SetCanceled/SetError for producer
            using AsyncStateBase::SetCanceled;
            using AsyncStateBase::SetError;

            void WaitForCompletion() override { AsyncStateBase::WaitForCompletion(); }

            /// Sets the completion callback.
            /// \param completed_callback The callback to be invoked when the operation completes.
            void SetCompleted(const std::function<void(const std::shared_ptr<libCZI::IAsyncOperation<TResult>>&)>& completed_callback) override
            {
                if (!completed_callback)
                {
                    throw std::invalid_argument("completed_callback cannot be null");
                }

                this->SetCompletedPrepare();
                this->completed_callback_ = completed_callback;
                this->SetCompletedFinish();
            }

            /// Gets the result of the operation.
            /// \return The result.
            /// Throws an exception if the operation is not completed or failed.
            TResult GetResult() override
            {
                const AsyncStatus status = this->async_status_.load(std::memory_order_acquire);
                switch (status)
                {
                case AsyncStatus::Completed:
                    return this->result_;
                case AsyncStatus::Started:
                    throw LibCZIAsyncOperationInvalidStateException(
                        "GetResult called before the asynchronous action completed.",
                        LibCZIAsyncOperationInvalidStateException::ErrorType::GetResultCalledBeforeCompletion);
                case AsyncStatus::Canceled:
                    throw LibCZIAsyncOperationInvalidStateException(
                        "GetResult called on a canceled asynchronous action.",
                        LibCZIAsyncOperationInvalidStateException::ErrorType::GetResultCalledOnCanceledOperation);
                case AsyncStatus::Error:
                    throw LibCZIAsyncOperationInvalidStateException(
                        "GetResult called on a failed asynchronous action.",
                        LibCZIAsyncOperationInvalidStateException::ErrorType::GetResultCalledOnFailedOperation);
                }

                throw std::runtime_error("GetResult called on an asynchronous action in an unexpected state.");
            }

            /// Marks the operation as successfully completed with a result.
            /// This should be called by the producer.
            /// \param result The result to set.
            void SetResult(TResult result)
            {
                 // Check reserve first? No, we need to set result before Status=Completed.
                 // Base SetDoneBase sets Status=Completed.
                 // Need to replicate SetDone logic but with Result storage.
                 
                bool expected = false;
                if (!this->completion_reserved_.compare_exchange_strong(expected, true))
                {
                    throw LibCZIAsyncOperationInvalidStateException(
                        "SetResult called after the asynchronous action already left the started state.",
                        LibCZIAsyncOperationInvalidStateException::ErrorType::InvalidStateTransition);
                }

                this->result_ = std::move(result);
                
                // Now replicate the rest of SetDoneBase logic:
                this->async_status_.store(AsyncStatus::Completed, std::memory_order_release);
                std::atomic_thread_fence(std::memory_order_seq_cst);
                
                if (this->callback_ready_.load(std::memory_order_acquire))
                {
                    this->NotifyCompletedBase();
                }
            }
            
        protected:
             void OnNotifyCompleted() override
             {
                 if (this->completed_callback_)
                 {
                     auto op = std::static_pointer_cast<libCZI::IAsyncOperation<TResult>>(this->shared_from_this());
                     this->completed_callback_(std::move(op));
                 }
             }
        };
    }
}
