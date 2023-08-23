#include "../include/streamsFactory.h"
#include <streams_Config.h>
#include "cprInputStream.h"

using namespace std;

/*static*/const std::vector<CStreams::StreamObjectInfo>& CStreams::GetListOfAvailableStreamObjects()
{
    static const vector<StreamObjectInfo> availableStreamObjects =
    {
#if STREAMS_CURL_AVAILABLE
        StreamObjectInfo{"curl", StreamType::Input, "cpr/libcurl-based stream-reader for many protocols (http, https, ...)" }
#endif
    };

    return availableStreamObjects;
}

std::shared_ptr<libCZI::IStream> CStreams::CreateStream(const std::string& className, const std::string& filename)
{
#if STREAMS_CURL_AVAILABLE
    if (className == "curl")
    {
        return std::make_shared<CprInputStream>(filename);
    }
#endif

    return nullptr;
}
