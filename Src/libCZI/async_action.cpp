#include "async_action.h"

#include <stdexcept>
#include <thread>

using namespace std;
using namespace libCZI;
using namespace libCZI::detail;

// AsyncStateBase Implementation

AsyncStateBase::AsyncStateBase()
{
}

AsyncStateBase::AsyncStateBase(std::shared_ptr<libCZI::IEventLoop> event)
    : event_loop_(std::move(event))
{
}

AsyncStateBase::~AsyncStateBase()
{
}

void AsyncStateBase::WaitForCompletion()
{
    for (;;)
    {
        const AsyncStatus status = this->async_status_.load(std::memory_order_acquire);
        if (status != AsyncStatus::Started)
        {
            return;
        }
       
        if (this->event_loop_)
        {
            if (this->event_loop_->RunLoop(IEventLoop::RunMode::RunOnce) == 0)
            {
                std::this_thread::yield();
            }
        }
    }
}

void AsyncStateBase::CancelCore()
{
    // Cancellation is a best-effort signal.
    //
    // Design goals:
    // - CancelCore() may be called at any time and from any thread.
    // - A producer may install the cancellation callback later.
    // - If CancelCore() happens first, the request is latched and the callback will be invoked when installed.
    // - The callback is invoked at most once.
    //
    // Thread-safety strategy:
    // - We do not write to a shared std::function directly; std::function is not safe for concurrent access.
    // - Instead we publish an immutable std::function behind a shared_ptr.
    // - The shared_ptr itself is published via atomic store/load (C++14 free functions).
    this->cancel_requested_.store(true, std::memory_order_release);

    auto cb = std::atomic_load_explicit(&this->cancellation_requested_, std::memory_order_acquire);
    if (!cb)
    {
        return;
    }

    bool expected = false;
    if (this->cancel_callback_invoked_.compare_exchange_strong(expected, true, std::memory_order_acq_rel))
    {
        (*cb)();
    }
}

void AsyncStateBase::SetCancellationRequestedCallback(std::function<void()> cancellation_requested)
{
    // Producer installs the callback exactly once.
    // If cancellation was already requested, invoke immediately.
    if (!cancellation_requested)
    {
        throw std::invalid_argument("cancellation_requested cannot be null");
    }

    bool expected = false;
    if (!this->cancel_callback_set_.compare_exchange_strong(expected, true, std::memory_order_acq_rel))
    {
        throw LibCZIAsyncOperationInvalidStateException(
            "SetCancellationRequestedCallback has already been called.",
            LibCZIAsyncOperationInvalidStateException::ErrorType::InvalidStateTransition);
    }

    auto cb_ptr = std::make_shared<const std::function<void()>>(std::move(cancellation_requested));
    std::atomic_store_explicit(&this->cancellation_requested_, cb_ptr, std::memory_order_release);

    if (this->cancel_requested_.load(std::memory_order_acquire))
    {
        bool invoke_expected = false;
        if (this->cancel_callback_invoked_.compare_exchange_strong(invoke_expected, true, std::memory_order_acq_rel))
        {
            (*cb_ptr)();
        }
    }
}

libCZI::AsyncStatus AsyncStateBase::GetStatusCore() const
{
    return this->async_status_.load(std::memory_order_acquire);
}

std::exception_ptr AsyncStateBase::GetExceptionCore() const
{
    const AsyncStatus status = this->async_status_.load(std::memory_order_acquire);
    switch (status)
    {
    case AsyncStatus::Error:
        return this->error_;
    case AsyncStatus::Started:
        throw LibCZIAsyncOperationInvalidStateException(
            "GetException called before the asynchronous action completed.",
            LibCZIAsyncOperationInvalidStateException::ErrorType::GetExceptionCalledBeforeCompletion);
    case AsyncStatus::Canceled:
        throw LibCZIAsyncOperationInvalidStateException(
            "GetException called on a canceled asynchronous action.",
            LibCZIAsyncOperationInvalidStateException::ErrorType::GetExceptionCalledOnCanceledOperation);
    case AsyncStatus::Completed:
        throw LibCZIAsyncOperationInvalidStateException(
            "GetException called on a completed asynchronous action.",
            LibCZIAsyncOperationInvalidStateException::ErrorType::GetExceptionCalledOnCompletedOperation);
    }

    throw runtime_error("GetException called on an asynchronous action in an unexpected state.");
}

void AsyncStateBase::SetCanceled()
{
    bool expected = false;
    if (!this->completion_reserved_.compare_exchange_strong(expected, true))
    {
        throw LibCZIAsyncOperationInvalidStateException(
            "SetCanceled called after the asynchronous action already left the started state.",
            LibCZIAsyncOperationInvalidStateException::ErrorType::InvalidStateTransition);
    }

    this->async_status_.store(AsyncStatus::Canceled, std::memory_order_release);
    std::atomic_thread_fence(std::memory_order_seq_cst);

    if (this->callback_ready_.load(std::memory_order_acquire))
    {
        this->NotifyCompletedBase();
    }
}

void AsyncStateBase::SetError(std::exception_ptr exception_ptr)
{
    bool expected = false;
    if (!this->completion_reserved_.compare_exchange_strong(expected, true))
    {
        throw LibCZIAsyncOperationInvalidStateException(
            "SetError called after the asynchronous action already left the started state.",
            LibCZIAsyncOperationInvalidStateException::ErrorType::InvalidStateTransition);
    }

    this->error_ = exception_ptr;
    this->async_status_.store(AsyncStatus::Error, std::memory_order_release);
    std::atomic_thread_fence(std::memory_order_seq_cst);

    if (this->callback_ready_.load(std::memory_order_acquire))
    {
        this->NotifyCompletedBase();
    }
}

void AsyncStateBase::SetCompletedPrepare()
{
    bool expected = false;
    if (!this->callback_set_reserved_.compare_exchange_strong(expected, true))
    {
        throw std::logic_error("SetCompleted has already been called.");
    }
}

void AsyncStateBase::SetCompletedFinish()
{
    this->callback_ready_.store(true, std::memory_order_release);
    std::atomic_thread_fence(std::memory_order_seq_cst);

    if (this->async_status_.load(std::memory_order_acquire) != AsyncStatus::Started)
    {
        this->NotifyCompletedBase();
    }
}

void AsyncStateBase::SetDoneBase()
{
    bool expected = false;
    if (!this->completion_reserved_.compare_exchange_strong(expected, true))
    {
        throw LibCZIAsyncOperationInvalidStateException(
            "SetDone called after the asynchronous action already left the started state.",
            LibCZIAsyncOperationInvalidStateException::ErrorType::InvalidStateTransition);
    }

    this->async_status_.store(AsyncStatus::Completed, std::memory_order_release);
    std::atomic_thread_fence(std::memory_order_seq_cst);

    if (this->callback_ready_.load(std::memory_order_acquire))
    {
        this->NotifyCompletedBase();
    }
}

void AsyncStateBase::NotifyCompletedBase()
{
    bool expected = false;
    if (this->invoked_.compare_exchange_strong(expected, true))
    {
        this->OnNotifyCompleted();
    }
}


// AsyncAction Implementation

AsyncAction::AsyncAction() : AsyncStateBase() {}

void AsyncAction::SetCompleted(const std::function<void(const std::shared_ptr<libCZI::IAsyncAction>&)>& completed_callback)
{
    if (!completed_callback)
    {
        throw std::invalid_argument("completed_callback cannot be null");
    }

    this->SetCompletedPrepare();
    this->completed_callback_ = completed_callback;
    this->SetCompletedFinish();
}

void AsyncAction::GetResult()
{
    const AsyncStatus status = this->async_status_.load(std::memory_order_acquire);
    switch (status)
    {
    case AsyncStatus::Completed:
        return;
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

    throw runtime_error("GetResult called on an asynchronous action in an unexpected state.");
}

void AsyncAction::SetDone()
{
    this->SetDoneBase();
}

void AsyncAction::OnNotifyCompleted()
{
    if (this->completed_callback_)
    {
        this->completed_callback_(shared_from_this());
    }
}
