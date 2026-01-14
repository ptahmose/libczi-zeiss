#pragma once

#include <atomic>

#include "libCZI_Async.h"

namespace libCZI
{
    namespace detail
    {
        class AsyncAction : public libCZI::IAsyncAction
        {
        private:
            // Represents the state of the operation (Started, Completed, Canceled, Error).
            // Transitions are guarded by completion_reserved_ to ensure single-writer semantics.
            std::atomic<libCZI::AsyncStatus> async_status_{ libCZI::AsyncStatus::Started };

            std::function<void()> cancellation_requested_;
            
            // The callback provided by the consumer.
            // Safe to access only when callback_ready_ == true (seen via acquire load) or if the current thread wrote it.
            std::function<void(libCZI::IAsyncAction*)> completed_callback_;

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

        public:
            explicit AsyncAction(const std::function<void()>& cancellation_requested);

            void SetCompleted(const std::function<void(libCZI::IAsyncAction*)>& completed_callback) override;
            void GetResult() override;
            void Cancel() override;
            libCZI::AsyncStatus GetStatus() const override;
            std::exception_ptr GetException() const override;

            void SetDone();
            void SetCanceled();
            void SetError(std::exception_ptr exception_ptr);

        private:
            void NotifyCompleted();
        };
    }
}
