#include "libCZIStream.h"
#include "streamsFactory.h"
#include <sstream>
#include <stdexcept>
#include <regex>

#if defined(WIN32ENV)
#define HAS_CODECVT
#include <Windows.h> 
#endif

#if defined(HAS_CODECVT)
#include <codecvt>
#include <stdlib.h>
#endif

using namespace libCZI;
using namespace std;

namespace
{
    static bool isValidURL(const wchar_t* url)
    {
        std::wregex urlPattern(
            L"^(https?:\\/\\/)"  // http(s)
            L"((([a-z\\d]([a-z\\d-][a-z\\d]))\\.)+[a-z]{2,}|"  // domain
            L"((\\d{1,3}\\.){3}\\d{1,3}))"  // OR ipv4
            L"(\\:\\d+)?(\\/[-a-z\\d%_.~+])"  // port and path
            L"(\\?[;&a-z\\d%_.~+=-]*)?"  // query string
            L"(\\#[-a-z\\d_]*)?$",  // fragment locator
            std::regex::icase
        );

        return std::regex_match(url, urlPattern);
    }

    static std::string convertToUtf8(const std::wstring& str)
    {
#if defined(HAS_CODECVT)
        std::wstring_convert<std::codecvt_utf8<wchar_t>> utf8_conv;
        std::string conv = utf8_conv.to_bytes(str);
        return conv;
#else
        size_t requiredSize = std::wcstombs(nullptr, str.c_str(), 0);
        std::string conv(requiredSize, 0);
        conv.resize(std::wcstombs(&conv[0], str.c_str(), requiredSize));
        return conv;
#endif
    }
}

std::shared_ptr<libCZI::IStream> libCZIStream::CreateStreamFromURL(const wchar_t* url)
{
    if(!isValidURL(url))
    {
        stringstream ss;
        ss << "Url provided is invalid:" << url <<".";
        throw runtime_error(ss.str());
    }
    shared_ptr<libCZI::IStream> stream = CStreams::CreateStream("curl", convertToUtf8(url));
    if (!stream)
    {
        stringstream ss;
        ss << "Unable to create an input-stream from url:" << url << ".";;
        throw runtime_error(ss.str());
    }
    return stream;
}
