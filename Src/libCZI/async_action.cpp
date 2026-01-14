#include "async_action.h"

#include <stdexcept>

using namespace std;
using namespace libCZI;
using namespace libCZI::detail;

// AsyncStateBase Implementation

AsyncStateBase::AsyncStateBase(const std::function<void()>& cancellation_requested)
    : cancellation_requested_(cancellation_requested)
{
}

void AsyncStateBase::CancelCore()
{
    if (this->async_status_.load(std::memory_order_acquire) != AsyncStatus::Started)
    {
        return;
    }

    if (this->cancellation_requested_)
    {
        this->cancellation_requested_();
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

AsyncAction::AsyncAction(const std::function<void()>& cancellation_requested)
    : AsyncStateBase(cancellation_requested)
{
}

void AsyncAction::SetCompleted(const std::function<void(libCZI::IAsyncAction*)>& completed_callback)
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
        this->completed_callback_(this);
    }
}

