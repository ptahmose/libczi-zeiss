#include "executeAsyncTest.h"

#include <Windows.h>
#include <atomic>

using namespace libCZI;
using namespace std;

namespace
{
     void WriteIntRect(stringstream& ss, const IntRect& r)
    {
        if (r.IsValid())
        {
            ss << "X=" << r.x << " Y=" << r.y << " W=" << r.w << " H=" << r.h;
        }
        else
        {
            ss << "invalid";
        }
    }

    void PrintStatistics(const CCmdLineOptions& options, const SubBlockStatistics& sbStatistics)
    {
        auto log = options.GetLog();

        stringstream ss;
        ss << "SubBlock-Statistics" << endl;
        ss << "-------------------" << endl;
        ss << endl;
        ss << "SubBlock-Count: " << sbStatistics.subBlockCount << endl;
        ss << endl;
        ss << "Bounding-Box:" << endl;
        ss << " All:    ";
        WriteIntRect(ss, sbStatistics.boundingBox);
        ss << endl;
        ss << " Layer0: ";
        WriteIntRect(ss, sbStatistics.boundingBoxLayer0Only);
        ss << endl;

        ss << endl;;
        if (sbStatistics.IsMIndexValid())
        {
            ss << "M-Index: min=" << sbStatistics.minMindex << " max=" << sbStatistics.maxMindex << endl;
        }
        else
        {
            ss << "M-Index: not valid" << endl;
        }

        ss << endl;
        ss << "Bounds:" << endl;
        sbStatistics.dimBounds.EnumValidDimensions(
            [&](libCZI::DimensionIndex dim, int start, int size)->bool
            {
                    ss << " " << Utils::DimensionToChar(dim) << " -> Start=" << start << " Size=" << size << endl;
                    return true;
            });

        if (!sbStatistics.sceneBoundingBoxes.empty())
        {
            ss << endl;
            ss << "Bounding-Box for scenes:" << endl;
            for (const auto& sceneBb : sbStatistics.sceneBoundingBoxes)
            {
                ss << " Scene" << sceneBb.first << ":" << endl;
                ss << "  All:    ";
                WriteIntRect(ss, sceneBb.second.boundingBox);
                ss << endl;
                ss << "  Layer0: ";
                WriteIntRect(ss, sceneBb.second.boundingBoxLayer0);
                ss << endl;
            }
        }

        log->WriteLineStdOut(ss.str());
    }
}

bool executeAsyncTest(const CCmdLineOptions& options)
{
    StreamsFactory::CreateStreamInfo streamInfo;
    streamInfo.class_name = "windows_apc_inputstream";
    auto async_stream = libCZI::StreamsFactory::CreateAsyncStream(streamInfo,options.GetCZIFilename());
    
    auto reader = CreateCZIReaderAsync();
    auto async_action = reader->Open(async_stream);

    //async_action->Cancel();

    std::atomic<bool> completed{ false };

    async_action->SetCompleted([&](const std::shared_ptr<IAsyncAction>& action)
        {
            completed.store(true, std::memory_order_release);
        });

    /*while (!completed.load(std::memory_order_acquire))
    {
        // Enter alertable wait state so APC-based I/O completion callbacks can run.
        SleepEx(10, TRUE);
    }*/
    async_action->WaitForCompletion();

    auto statístics = reader->GetStatistics();
    PrintStatistics(options, statístics);

    return true;
}
