#include "include_gtest.h"
#include "inc_libCZI.h"

#include "async_action.h"

using namespace libCZI;
using namespace libCZI::detail;
using namespace std;

TEST(AsyncActionTest, Sequential_SetDone_Then_SetCompleted)
{
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
    auto asyncAction = std::make_shared<AsyncAction>(nullptr);
    asyncAction->SetDone();
    EXPECT_THROW(asyncAction->SetDone(), LibCZIAsyncOperationInvalidStateException);
}

TEST(AsyncActionTest, Double_SetCompleted_Throws)
{
    auto asyncAction = std::make_shared<AsyncAction>(nullptr);
    asyncAction->SetCompleted([](IAsyncAction*) {});
    EXPECT_THROW(asyncAction->SetCompleted([](IAsyncAction*) {}), std::logic_error);
}

TEST(AsyncActionTest, Concurrent_SetDone_And_SetCompleted)
{
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


