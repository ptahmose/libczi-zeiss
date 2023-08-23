#include "cprInputStream.h"

#if STREAMS_CURL_AVAILABLE
#include <sstream>

using namespace cpr;
using namespace std;

CprInputStream::CprInputStream(const std::string& url)
{
    this->session.SetUrl(Url{ url });
}

/*virtual*/void CprInputStream::Read(std::uint64_t offset, void* pv, std::uint64_t size, std::uint64_t* ptrBytesRead)
{
    if (size == 0)
    {
        throw invalid_argument("size must not be 0");
    }

    this->session.SetRange(Range{ static_cast<long long>(offset),static_cast<long long>(offset + size - 1) });

    WriteCallbackInfo callbackInfo(pv, size);
    const WriteCallback wb(CprInputStream::WriteCallbackFunction, reinterpret_cast<intptr_t>(&callbackInfo));

    const Response response = this->session.Download(wb);

    if (response.status_code >= 400)
    {
        stringstream ss;
        ss << "Error reading data - StatusCode:" << response.status_code << " , Reason:\"" << response.reason << "\", StatusLine: \"" << response.status_line << "\"";
        throw std::runtime_error(ss.str());
    }

    if (ptrBytesRead != nullptr)
    {
        *ptrBytesRead = callbackInfo.bytesRead_;
    }
}

/*static*/bool CprInputStream::WriteCallbackFunction(std::string data, intptr_t userdata)
{
    WriteCallbackInfo* callbackInfo = reinterpret_cast<WriteCallbackInfo*>(userdata);
    if (callbackInfo->sizeDest_ <= callbackInfo->bytesRead_)
    {
        // this is not really expected, and probably is indicating an error
        return false;
    }

    size_t bytesToCopy = min(callbackInfo->sizeDest_ - callbackInfo->bytesRead_, data.size());
    memcpy(callbackInfo->ptrDest_ + callbackInfo->bytesRead_, data.c_str(), bytesToCopy);
    callbackInfo->bytesRead_ += bytesToCopy;

    return callbackInfo->bytesRead_ < callbackInfo->sizeDest_;
}

#endif
