#include "include_gtest.h"
#include "inc_libCZI.h"

#include "async_action.h"

using namespace libCZI;
using namespace libCZI::detail;
using namespace std;

TEST(AsyncActionTest, Sequential_SetDone_Then_SetCompleted)
{
    /// This test verifies the scenario where the operation completes (SetDone) 
    /// before the consumer subscribes to the completion callback (SetCompleted).
    /// It ensures that the status is updated correctly and the callback is invoked immediately upon subscription.

    auto asyncAction = std::make_shared<AsyncAction>(nullptr);

    // Act 1: SetDone (Producer finishes)
    asyncAction->SetDone();

    // Verify status
    EXPECT_EQ(asyncAction->GetStatus(), AsyncStatus::Completed);

    // Act 2: SetCompleted (Consumer subscribes)
    bool callbackInvoked = false;
    asyncAction->SetCompleted([&](IAsyncAction* action) 
        {
            EXPECT_EQ(action, asyncAction.get());
            callbackInvoked = true;
        });

    // Assert
    EXPECT_TRUE(callbackInvoked);

    // Also GetResult should return immediately
    asyncAction->GetResult();
}

TEST(AsyncActionTest, Sequential_SetCompleted_Then_SetDone)
{
    /// This test verifies the scenario where the consumer subscribes to the completion callback (SetCompleted)
    /// before the operation completes (SetDone).
    /// It ensures that the callback is invoked only when SetDone is called.

    auto asyncAction = std::make_shared<AsyncAction>(nullptr);

    // Act 1: SetCompleted (Consumer subscribes)
    bool callbackInvoked = false;
    asyncAction->SetCompleted([&](IAsyncAction* action) 
        {
            EXPECT_EQ(action, asyncAction.get());
            callbackInvoked = true;
        });

    // Asset: Not called yet
    EXPECT_FALSE(callbackInvoked);
    EXPECT_EQ(asyncAction->GetStatus(), AsyncStatus::Started);

    // Act 2: SetDone (Producer finishes)
    asyncAction->SetDone();

    // Assert
    EXPECT_TRUE(callbackInvoked);
    EXPECT_EQ(asyncAction->GetStatus(), AsyncStatus::Completed);
    asyncAction->GetResult();
}

TEST(AsyncActionTest, Sequential_SetError)
{
    /// This test verifies that setting an error transitions the operation to the Error state,
    /// invokes the callback, and that GetResult throws the appropriate exception 
    /// while GetException returns the stored exception.

    auto asyncAction = std::make_shared<AsyncAction>(nullptr);
    auto exception = std::make_exception_ptr(std::runtime_error("Test Error"));

    asyncAction->SetError(exception);

    EXPECT_EQ(asyncAction->GetStatus(), AsyncStatus::Error);

    // Verify callback
    bool callbackInvoked = false;
    asyncAction->SetCompleted([&](IAsyncAction* action) 
        {
            EXPECT_EQ(action->GetStatus(), AsyncStatus::Error);
            callbackInvoked = true;
        });

    EXPECT_TRUE(callbackInvoked);
    EXPECT_THROW(asyncAction->GetResult(), LibCZIAsyncOperationInvalidStateException);

    try 
    {
        asyncAction->GetResult();
    }
    catch (const LibCZIAsyncOperationInvalidStateException& e) 
    {
        EXPECT_EQ(e.GetErrorType(), LibCZIAsyncOperationInvalidStateException::ErrorType::GetResultCalledOnFailedOperation);
    }

    // Check if we can retrieve the exception
    auto retrievedEx = asyncAction->GetException();
    EXPECT_NE(retrievedEx, nullptr);
    try 
    {
        std::rethrow_exception(retrievedEx);
    }
    catch (const std::runtime_error& e) 
    {
        EXPECT_STREQ(e.what(), "Test Error");
    }
}

TEST(AsyncActionTest, Sequential_SetCanceled)
{
    /// This test verifies the cancellation flow: requesting cancellation via Cancel(),
    /// confirming the request was received, and transitioning to the Canceled state via SetCanceled().
    /// It ensures GetResult throws the expected exception for a canceled operation.

    bool cancelRequested = false;
    auto asyncAction = std::make_shared<AsyncAction>([&]() 
        {
            cancelRequested = true;
        });

    asyncAction->Cancel();
    EXPECT_TRUE(cancelRequested);

    asyncAction->SetCanceled();

    EXPECT_EQ(asyncAction->GetStatus(), AsyncStatus::Canceled);

    EXPECT_THROW(asyncAction->GetResult(), LibCZIAsyncOperationInvalidStateException);

    try 
    {
        asyncAction->GetResult();
    }
    catch (const LibCZIAsyncOperationInvalidStateException& e) 
    {
        EXPECT_EQ(e.GetErrorType(), LibCZIAsyncOperationInvalidStateException::ErrorType::GetResultCalledOnCanceledOperation);
    }
}

TEST(AsyncActionTest, Double_SetDone_Throws)
{
    /// This test verifies that calling SetDone more than once throws an InvalidStateTransition exception,
    /// ensuring the single-transition invariant.

    auto asyncAction = std::make_shared<AsyncAction>(nullptr);
    asyncAction->SetDone();
    EXPECT_THROW(asyncAction->SetDone(), LibCZIAsyncOperationInvalidStateException);
}

TEST(AsyncActionTest, Double_SetCompleted_Throws)
{
    /// This test verifies that calling SetCompleted more than once throws an exception,
    /// ensuring that only one callback can be registered.

    auto asyncAction = std::make_shared<AsyncAction>(nullptr);
    asyncAction->SetCompleted([](IAsyncAction*) {});
    EXPECT_THROW(asyncAction->SetCompleted([](IAsyncAction*) {}), std::logic_error);
}

TEST(AsyncActionTest, Concurrent_SetDone_And_SetCompleted)
{
    /// This stress test verifies thread safety when SetDone (producer) and SetCompleted (consumer)
    /// are called concurrently. It ensures that no race conditions occur and the callback is invoked exactly once.

    // Need to run this many times to try to hit race conditions
    for (int i = 0; i < 1000; ++i)
    {
        auto asyncAction = std::make_shared<AsyncAction>(nullptr);
        std::atomic<int> callbackCount{ 0 };

        std::thread consumerThread([&]() 
            {
                asyncAction->SetCompleted([&](IAsyncAction* action) 
                {
                    ++callbackCount;
                });
            });

        std::thread producerThread([&]() 
            {
                asyncAction->SetDone();
            });

        consumerThread.join();
        producerThread.join();

        EXPECT_EQ(callbackCount.load(), 1) << "Iteration " << i;
        EXPECT_EQ(asyncAction->GetStatus(), AsyncStatus::Completed);
    }
}

using IntAsyncOperation = libCZI::detail::AsyncOperation<int>;

TEST(AsyncOperationTest, Sequential_SetResult_Then_SetCompleted)
{
    /// This test verifies the scenario for AsyncOperation<T> where a result is set 
    /// before the consumer subscribes. It checks that the result is correctly stored and retrieved.

    auto asyncOp = std::make_shared<IntAsyncOperation>(nullptr);

    int expectedResult = 42;
    asyncOp->SetResult(expectedResult);

    EXPECT_EQ(asyncOp->GetStatus(), AsyncStatus::Completed);

    bool callbackInvoked = false;
    asyncOp->SetCompleted([&](IAsyncOperation<int>* op)
        {
            EXPECT_EQ(op, asyncOp.get());
            callbackInvoked = true;
        });

    EXPECT_TRUE(callbackInvoked);
    EXPECT_EQ(asyncOp->GetResult(), expectedResult);
}

TEST(AsyncOperationTest, Sequential_SetCompleted_Then_SetResult)
{
    /// This test verifies the scenario for AsyncOperation<T> where the consumer subscribes 
    /// before the result is set. It checks that the result becomes available upon completion.

    auto asyncOp = std::make_shared<IntAsyncOperation>(nullptr);
    int expectedResult = 123;

    bool callbackInvoked = false;
    asyncOp->SetCompleted([&](IAsyncOperation<int>* op)
        {
            EXPECT_EQ(op, asyncOp.get());
            callbackInvoked = true;
        });

    EXPECT_FALSE(callbackInvoked);
    EXPECT_EQ(asyncOp->GetStatus(), AsyncStatus::Started);

    asyncOp->SetResult(expectedResult);

    EXPECT_TRUE(callbackInvoked);
    EXPECT_EQ(asyncOp->GetStatus(), AsyncStatus::Completed);
    EXPECT_EQ(asyncOp->GetResult(), expectedResult);
}

TEST(AsyncOperationTest, Sequential_SetError)
{
    /// This test verifies error handling for AsyncOperation<T>, ensuring correct state transition
    /// and exception reporting when SetError is used.

    auto asyncOp = std::make_shared<IntAsyncOperation>(nullptr);
    auto exception = std::make_exception_ptr(std::runtime_error("Op Error"));

    asyncOp->SetError(exception);
    EXPECT_EQ(asyncOp->GetStatus(), AsyncStatus::Error);

    EXPECT_THROW(asyncOp->GetResult(), LibCZIAsyncOperationInvalidStateException);

    try
    {
        asyncOp->GetResult();
    }
    catch (const LibCZIAsyncOperationInvalidStateException& e)
    {
        EXPECT_EQ(e.GetErrorType(), LibCZIAsyncOperationInvalidStateException::ErrorType::GetResultCalledOnFailedOperation);
    }
}

TEST(AsyncOperationTest, Sequential_SetCanceled)
{
    /// This test verifies cancellation handling for AsyncOperation<T>, ensuring correct state transition
    /// and exception reporting when SetCanceled is used.

    bool cancelRequested = false;
    auto asyncOp = std::make_shared<IntAsyncOperation>([&]() { cancelRequested = true; });

    asyncOp->Cancel();
    EXPECT_TRUE(cancelRequested);

    asyncOp->SetCanceled();
    EXPECT_EQ(asyncOp->GetStatus(), AsyncStatus::Canceled);

    EXPECT_THROW(asyncOp->GetResult(), LibCZIAsyncOperationInvalidStateException);

    try
    {
        asyncOp->GetResult();
    }
    catch (const LibCZIAsyncOperationInvalidStateException& e)
    {
        EXPECT_EQ(e.GetErrorType(), LibCZIAsyncOperationInvalidStateException::ErrorType::GetResultCalledOnCanceledOperation);
    }
}

TEST(AsyncOperationTest, Double_SetResult_Throws)
{
    /// This test verifies that calling SetResult more than once throws an exception,
    /// ensuring the result is set exactly once.

    auto asyncOp = std::make_shared<IntAsyncOperation>(nullptr);
    asyncOp->SetResult(1);
    EXPECT_THROW(asyncOp->SetResult(2), LibCZIAsyncOperationInvalidStateException);
}

TEST(AsyncOperationTest, Concurrent_SetResult_And_SetCompleted)
{
    /// This stress test verifies thread safety for AsyncOperation<T> when SetResult and SetCompleted
    /// are called concurrently, ensuring the result is safely delivered to the callback.

    for (int i = 0; i < 1000; ++i)
    {
        auto asyncOp = std::make_shared<IntAsyncOperation>(nullptr);
        int expectedResult = i;
        std::atomic<int> callbackCount{ 0 };

        std::thread consumerThread([&]()
            {
                asyncOp->SetCompleted([&](IAsyncOperation<int>* op)
                    {
                        ++callbackCount;
                    });
            });

        std::thread producerThread([&]()
            {
                asyncOp->SetResult(expectedResult);
            });

        consumerThread.join();
        producerThread.join();

        EXPECT_EQ(callbackCount.load(), 1) << "Iteration " << i;
        EXPECT_EQ(asyncOp->GetStatus(), AsyncStatus::Completed);
        EXPECT_EQ(asyncOp->GetResult(), expectedResult);
    }
}

TEST(AsyncActionTest, GetException_BeforeCompletion_Throws)
{
    // Ensure GetException rejects calls while the action is still in Started.
    auto asyncAction = std::make_shared<AsyncAction>(nullptr);

    EXPECT_THROW(asyncAction->GetException(), LibCZIAsyncOperationInvalidStateException);

    try
    {
        asyncAction->GetException();
    }
    catch (const LibCZIAsyncOperationInvalidStateException& e)
    {
        EXPECT_EQ(e.GetErrorType(), LibCZIAsyncOperationInvalidStateException::ErrorType::GetExceptionCalledBeforeCompletion);
    }
}

TEST(AsyncActionTest, GetException_OnCanceled_Throws)
{
    // Ensure GetException rejects calls once the action was canceled.
    auto asyncAction = std::make_shared<AsyncAction>(nullptr);
    asyncAction->SetCanceled();

    EXPECT_THROW(asyncAction->GetException(), LibCZIAsyncOperationInvalidStateException);

    try
    {
        asyncAction->GetException();
    }
    catch (const LibCZIAsyncOperationInvalidStateException& e)
    {
        EXPECT_EQ(e.GetErrorType(), LibCZIAsyncOperationInvalidStateException::ErrorType::GetExceptionCalledOnCanceledOperation);
    }
}

TEST(AsyncActionTest, GetException_OnCompleted_Throws)
{
    // Ensure GetException rejects calls once the action completed successfully.
    auto asyncAction = std::make_shared<AsyncAction>(nullptr);
    asyncAction->SetDone();

    EXPECT_THROW(asyncAction->GetException(), LibCZIAsyncOperationInvalidStateException);

    try
    {
        asyncAction->GetException();
    }
    catch (const LibCZIAsyncOperationInvalidStateException& e)
    {
        EXPECT_EQ(e.GetErrorType(), LibCZIAsyncOperationInvalidStateException::ErrorType::GetExceptionCalledOnCompletedOperation);
    }
}

TEST(AsyncOperationTest, GetException_BeforeCompletion_Throws)
{
    // Ensure GetException rejects calls while the operation is still in Started.
    auto asyncOp = std::make_shared<IntAsyncOperation>(nullptr);

    EXPECT_THROW(asyncOp->GetException(), LibCZIAsyncOperationInvalidStateException);

    try
    {
        asyncOp->GetException();
    }
    catch (const LibCZIAsyncOperationInvalidStateException& e)
    {
        EXPECT_EQ(e.GetErrorType(), LibCZIAsyncOperationInvalidStateException::ErrorType::GetExceptionCalledBeforeCompletion);
    }
}

TEST(AsyncOperationTest, GetException_OnCanceled_Throws)
{
    // Ensure GetException rejects calls once the operation was canceled.
    auto asyncOp = std::make_shared<IntAsyncOperation>(nullptr);
    asyncOp->SetCanceled();

    EXPECT_THROW(asyncOp->GetException(), LibCZIAsyncOperationInvalidStateException);

    try
    {
        asyncOp->GetException();
    }
    catch (const LibCZIAsyncOperationInvalidStateException& e)
    {
        EXPECT_EQ(e.GetErrorType(), LibCZIAsyncOperationInvalidStateException::ErrorType::GetExceptionCalledOnCanceledOperation);
    }
}

TEST(AsyncOperationTest, GetException_OnCompleted_Throws)
{
    // Ensure GetException rejects calls once the operation completed successfully.
    auto asyncOp = std::make_shared<IntAsyncOperation>(nullptr);
    asyncOp->SetResult(10);

    EXPECT_THROW(asyncOp->GetException(), LibCZIAsyncOperationInvalidStateException);

    try
    {
        asyncOp->GetException();
    }
    catch (const LibCZIAsyncOperationInvalidStateException& e)
    {
        EXPECT_EQ(e.GetErrorType(), LibCZIAsyncOperationInvalidStateException::ErrorType::GetExceptionCalledOnCompletedOperation);
    }
}
