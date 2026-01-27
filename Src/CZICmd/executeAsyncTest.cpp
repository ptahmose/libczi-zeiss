#include "executeAsyncTest.h"

#include <Windows.h>
#include <atomic>

using namespace libCZI;
using namespace std;

bool executeAsyncTest(const CCmdLineOptions& options)
{
    StreamsFactory::CreateStreamInfo streamInfo;
    streamInfo.class_name = "windows_apc_inputstream";
    auto async_stream = libCZI::StreamsFactory::CreateAsyncStream(streamInfo,options.GetCZIFilename());
    
    auto reader = CreateCZIReaderAsync();
    auto async_action = reader->Open(async_stream);

    std::atomic<bool> completed{ false };

    async_action->SetCompleted([&](IAsyncAction* action)
        {
            completed.store(true, std::memory_order_release);
        });

    while (!completed.load(std::memory_order_acquire))
    {
        // Enter alertable wait state so APC-based I/O completion callbacks can run.
        SleepEx(10, TRUE);
    }

    return true;
}
