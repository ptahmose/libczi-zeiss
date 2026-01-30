#pragma once

#include <atomic>
#include <stdexcept>
#include <functional>
#include <exception>

#include "libCZI_Async.h"

namespace libCZI
{
    namespace detail
    {
        /// Base class for implementing asynchronous operations.
        /// It encapsulates the state management (AsyncStatus), error handling, and cancellation logic
        /// common to both void-returning actions and result-returning operations.
        /// Memory ordering rationale:
        /// - completion_reserved_ is the single-writer guard for status/payload transitions; status stores use
        ///   memory_order_release and readers use memory_order_acquire.
        /// - callback_ready_ is release-stored after writing the callback; producers acquire-load it before
        ///   invoking NotifyCompleted to ensure callback visibility.
        /// - invoked_ is a once-flag around callback invocation; ordering relies on surrounding acquire/release.
        /// - seq_cst fences after terminal writes are a conservative barrier to ensure payload/status visibility
        ///   across threads; keep or remove only with care.
        class AsyncStateBase
        {
        protected:
            // Represents the state of the operation (Started, Completed, Canceled, Error).
            // Transitions are guarded by completion_reserved_ to ensure single-writer semantics.
            std::atomic<libCZI::AsyncStatus> async_status_{ libCZI::AsyncStatus::Started };

            // Callback invoked when cancellation is requested by the consumer (Cancel/CancelCore).
            // Provided by the producer to propagate cancellation intent to the underlying operation.
            std::function<void()> cancellation_requested_;

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
            /// Initializes a new instance of the AsyncStateBase class.
            /// \param cancellation_requested The function to invoke when cancellation is requested.
            explicit AsyncStateBase(const std::function<void()>& cancellation_requested);

            /// Finalizes an instance of the AsyncStateBase class.
            virtual ~AsyncStateBase();

            /// Cancels the operation by invoking the cancellation callback.
            void CancelCore();

            /// Gets the current status of the operation.
            /// \return The current status.
            libCZI::AsyncStatus GetStatusCore() const;

            /// Gets the exception if the operation failed.
            /// \return The exception pointer, or null if no error occurred.
            std::exception_ptr GetExceptionCore() const;

            /// Transitions the operation to the Canceled state.
            /// This should be called by the producer when the operation is canceled.
            void SetCanceled();

            /// Transitions the operation to the Error state.
            /// This should be called by the producer when the operation fails.
            /// \param exception_ptr The exception indicating the failure.
            void SetError(std::exception_ptr exception_ptr);

        protected:
            void SetCompletedPrepare();
            void SetCompletedFinish();
            void SetDoneBase();
            void NotifyCompletedBase();
        };

        /// Implementation of the IAsyncAction interface.
        /// Represents an asynchronous action that does not return a value.
        class AsyncAction : public libCZI::IAsyncAction, public AsyncStateBase
        {
        private:
            // The callback provided by the consumer.
            // Safe to access only when callback_ready_ == true (seen via acquire load) or if the current thread wrote it.
            std::function<void(libCZI::IAsyncAction*)> completed_callback_;

        public:
            /// Initializes a new instance of the AsyncAction class.
            /// \param cancellation_requested The function to invoke when cancellation is requested.
            explicit AsyncAction(const std::function<void()>& cancellation_requested);

            // Forward IAsyncInfo methods to AsyncStateBase
            void Cancel() override { this->CancelCore(); }
            libCZI::AsyncStatus GetStatus() const override { return this->GetStatusCore(); }
            std::exception_ptr GetException() const override { return this->GetExceptionCore(); }

            /// Sets the completion callback.
            /// \param completed_callback The callback to be invoked when the operation completes.
            void SetCompleted(const std::function<void(libCZI::IAsyncAction*)>& completed_callback) override;

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

        protected:
            void OnNotifyCompleted() override;
        };

        /// Implementation of the IAsyncOperation<TResult> interface.
        /// Represents an asynchronous operation that returns a value of type TResult.
        /// \tparam TResult The type of the result produced by the operation.
        template <typename TResult>
        class AsyncOperation : public libCZI::IAsyncOperation<TResult>, public AsyncStateBase
        {
        private:
            std::function<void(libCZI::IAsyncOperation<TResult>*)> completed_callback_;
            TResult result_{};

        public:
            /// Initializes a new instance of the AsyncOperation class.
            /// \param cancellation_requested The function to invoke when cancellation is requested.
            explicit AsyncOperation(const std::function<void()>& cancellation_requested)
                : AsyncStateBase(cancellation_requested)
            {
            }

            // Forward IAsyncInfo methods
            void Cancel() override { this->CancelCore(); }
            libCZI::AsyncStatus GetStatus() const override { return this->GetStatusCore(); }
            std::exception_ptr GetException() const override { return this->GetExceptionCore(); }

            // Expose SetCanceled/SetError for producer
            using AsyncStateBase::SetCanceled;
            using AsyncStateBase::SetError;

            /// Sets the completion callback.
            /// \param completed_callback The callback to be invoked when the operation completes.
            void SetCompleted(const std::function<void(libCZI::IAsyncOperation<TResult>*)>& completed_callback) override
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
                     this->completed_callback_(this);
                 }
             }
        };
    }
}
