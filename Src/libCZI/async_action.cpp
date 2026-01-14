#include "async_action.h"

#include <stdexcept>

using namespace std;
using namespace libCZI;
using namespace libCZI::detail;

AsyncAction::AsyncAction(const std::function<void()>& cancellation_requested)
    : cancellation_requested_(cancellation_requested)
{
}

void AsyncAction::Cancel()
{
    if (this->cancellation_requested_)
    {
        this->cancellation_requested_();
    }
}

void AsyncAction::SetCompleted(const std::function<void(libCZI::IAsyncAction*)>& completed_callback)
{
    if (!completed_callback)
    {
        throw std::invalid_argument("completed_callback cannot be null");
    }

    bool expected = false;
    if (!this->callback_set_reserved_.compare_exchange_strong(expected, true))
    {
        throw std::logic_error("SetCompleted has already been called on this AsyncAction");
    }

    this->completed_callback_ = completed_callback;

    // Publish the callback so the producer thread can see it
    this->callback_ready_.store(true, std::memory_order_release);

    // Ensure that if we don't see the completed status, the producer thread will see our callback
    std::atomic_thread_fence(std::memory_order_seq_cst);

    if (this->async_status_.load(std::memory_order_acquire) != AsyncStatus::Started)
    {
        this->NotifyCompleted();
    }
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

std::exception_ptr AsyncAction::GetException() const
{
    const AsyncStatus status = this->async_status_.load(std::memory_order_acquire);
    switch (status)
    {
    case AsyncStatus::Completed:
        throw LibCZIAsyncOperationInvalidStateException(
            "GetException called on a completed asynchronous action.",
            LibCZIAsyncOperationInvalidStateException::ErrorType::GetResultCalledOnFailedOperation);
    case AsyncStatus::Started:
        throw LibCZIAsyncOperationInvalidStateException(
            "GetException called before the asynchronous action completed.",
            LibCZIAsyncOperationInvalidStateException::ErrorType::GetResultCalledBeforeCompletion);
    case AsyncStatus::Canceled:
        throw LibCZIAsyncOperationInvalidStateException(
            "GetException called on a canceled asynchronous action.",
            LibCZIAsyncOperationInvalidStateException::ErrorType::GetResultCalledOnCanceledOperation);
    case AsyncStatus::Error:
        return this->error_;
    }

    throw runtime_error("GetException called on an asynchronous action in an unexpected state.");
}

libCZI::AsyncStatus AsyncAction::GetStatus() const
{
    return this->async_status_.load(std::memory_order_acquire);
}

void AsyncAction::SetDone()
{
    const AsyncStatus status = this->async_status_.load(std::memory_order_relaxed);
    if (status != AsyncStatus::Started)
    {
        throw LibCZIAsyncOperationInvalidStateException(
            "SetDone called after the asynchronous action already left the started state.",
            LibCZIAsyncOperationInvalidStateException::ErrorType::InvalidStateTransition);
    }
    
    // We use exchange to act as a barrier and ensure only one transition happens,
    // though the contract says producer calls these once. If multiple producer calls race,
    // only one should succeed.
    AsyncStatus expected = AsyncStatus::Started;
    if (!this->async_status_.compare_exchange_strong(expected, AsyncStatus::Completed, std::memory_order_release))
    {
         throw LibCZIAsyncOperationInvalidStateException(
            "SetDone called after the asynchronous action already left the started state.",
            LibCZIAsyncOperationInvalidStateException::ErrorType::InvalidStateTransition);
    }
    
    // Ensure that if we don't see the callback ready, the consumer thread will see our status change
    std::atomic_thread_fence(std::memory_order_seq_cst);
    
    if (this->callback_ready_.load(std::memory_order_acquire))
    {
        this->NotifyCompleted();
    }
}

void AsyncAction::SetCanceled()
{
    AsyncStatus expected = AsyncStatus::Started;
    if (!this->async_status_.compare_exchange_strong(expected, AsyncStatus::Canceled, std::memory_order_release))
    {
        throw LibCZIAsyncOperationInvalidStateException(
            "SetCanceled called after the asynchronous action already left the started state.",
            LibCZIAsyncOperationInvalidStateException::ErrorType::InvalidStateTransition);
    }

    std::atomic_thread_fence(std::memory_order_seq_cst);

    if (this->callback_ready_.load(std::memory_order_acquire))
    {
        this->NotifyCompleted();
    }
}

void AsyncAction::SetError(std::exception_ptr exception_ptr)
{
    // Write error before publishing status
    this->error_ = exception_ptr;

    AsyncStatus expected = AsyncStatus::Started;
    if (!this->async_status_.compare_exchange_strong(expected, AsyncStatus::Error, std::memory_order_release))
    {
        throw LibCZIAsyncOperationInvalidStateException(
            "SetError called after the asynchronous action already left the started state.",
            LibCZIAsyncOperationInvalidStateException::ErrorType::InvalidStateTransition);
    }

    std::atomic_thread_fence(std::memory_order_seq_cst);
    
    if (this->callback_ready_.load(std::memory_order_acquire))
    {
        this->NotifyCompleted();
    }
}

void AsyncAction::NotifyCompleted()
{
    bool expected = false;
    if (this->invoked_.compare_exchange_strong(expected, true))
    {
        // At this point we know:
        // 1. One thread (producer or consumer) has taken responsibility to run the callback.
        // 2. Access to completed_callback_ is safe because:
        //    - If Consumer runs this: It wrote completed_callback_ itself.
        //    - If Producer runs this: It saw callback_ready_ == true (Acquire), which matches
        //      Consumer's Release store after writing completed_callback_.
        if (this->completed_callback_) 
        {
            this->completed_callback_(this);
        }
    }
}

