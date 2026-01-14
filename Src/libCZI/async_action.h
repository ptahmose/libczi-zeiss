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
            std::atomic<libCZI::AsyncStatus> async_status_{ libCZI::AsyncStatus::Started };
            std::function<void()> cancellation_requested_;
            std::function<void(libCZI::IAsyncAction*)> completed_callback_;
            std::exception_ptr error_;
            std::atomic<bool> invoked_{ false };
            std::atomic<bool> callback_set_reserved_{ false };
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
