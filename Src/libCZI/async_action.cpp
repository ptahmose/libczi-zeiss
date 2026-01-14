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

    // Ensure SetCompleted is called only once.
    bool expected = false;
    if (!this->callback_set_reserved_.compare_exchange_strong(expected, true))
    {
        throw std::logic_error("SetCompleted has already been called on this AsyncAction");
    }

    // Now we own the slot for setting the callback.
    this->completed_callback_ = completed_callback;

    // Publish the callback so the producer thread can see it
    // Release ensures that the write to completed_callback_ happens before this store.
    this->callback_ready_.store(true, std::memory_order_release);

    // Ensure that if we don't see the completed status, the producer thread will see our callback.
    // This atomic_thread_fence(seq_cst) is part of a store-load barrier (Dekker's algorithm pattern) 
    // interacting with the fence in SetDone/SetCanceled/SetError.
    // Rule: One of the threads must see the other's flag.
    std::atomic_thread_fence(std::memory_order_seq_cst);

    // Check if the operation is already done.
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
    // Ensure that only one thread transitions the state to terminal.
    bool expected = false;
    if (!this->completion_reserved_.compare_exchange_strong(expected, true))
    {
        throw LibCZIAsyncOperationInvalidStateException(
            "SetDone called after the asynchronous action already left the started state.",
            LibCZIAsyncOperationInvalidStateException::ErrorType::InvalidStateTransition);
    }
    
    // We own the transition now.
    // Publish the status change. Release ensures that any payload writes (none here) are visible.
    this->async_status_.store(AsyncStatus::Completed, std::memory_order_release);
    
    // Ensure that if we don't see the callback ready, the consumer thread will see our status change.
    // This corresponds to the fence in SetCompleted.
    std::atomic_thread_fence(std::memory_order_seq_cst);
    
    // If the consumer has already set the callback, we must invoke it.
    if (this->callback_ready_.load(std::memory_order_acquire))
    {
        this->NotifyCompleted();
    }
}

void AsyncAction::SetCanceled()
{
    // Ensure that only one thread transitions the state to terminal.
    bool expected = false;
    if (!this->completion_reserved_.compare_exchange_strong(expected, true))
    {
        throw LibCZIAsyncOperationInvalidStateException(
            "SetCanceled called after the asynchronous action already left the started state.",
            LibCZIAsyncOperationInvalidStateException::ErrorType::InvalidStateTransition);
    }

    // Publish the status change to Canceled.
    this->async_status_.store(AsyncStatus::Canceled, std::memory_order_release);

    // Store-Load barrier to coordinate with SetCompleted.
    std::atomic_thread_fence(std::memory_order_seq_cst);

    if (this->callback_ready_.load(std::memory_order_acquire))
    {
        this->NotifyCompleted();
    }
}

void AsyncAction::SetError(std::exception_ptr exception_ptr)
{
    // Ensure that only one thread transitions the state to terminal.
    // This also acts as a lock for the error_ member variable.
    bool expected = false;
    if (!this->completion_reserved_.compare_exchange_strong(expected, true))
    {
        throw LibCZIAsyncOperationInvalidStateException(
            "SetError called after the asynchronous action already left the started state.",
            LibCZIAsyncOperationInvalidStateException::ErrorType::InvalidStateTransition);
    }

    // Write error before publishing status. Use of completion_reserved_ guarantees exclusive access.
    this->error_ = exception_ptr;

    // Publish status. Release ensures the write to error_ is visible to any thread observing Error status.
    this->async_status_.store(AsyncStatus::Error, std::memory_order_release);

    // Store-Load barrier to coordinate with SetCompleted.
    std::atomic_thread_fence(std::memory_order_seq_cst);
    
    if (this->callback_ready_.load(std::memory_order_acquire))
    {
        this->NotifyCompleted();
    }
}

void AsyncAction::NotifyCompleted()
{
    bool expected = false;
    // invoked_ ensures the callback runs exactly once, regardless of race.
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

