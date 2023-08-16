#pragma once

#include <streams_Config.h>

#if STREAMS_CURL_AVAILABLE

#include "../libCZI/libCZI.h"
#include <string>
#include <cstdint>
#include <cpr/cpr.h>

class CprInputStream : public libCZI::IStream
{
private:
  cpr::Session session;
public:
  CprInputStream(const std::string& url);

  virtual void Read(std::uint64_t offset, void* pv, std::uint64_t size, std::uint64_t* ptrBytesRead) override;

  virtual ~CprInputStream() override = default;
private:
  struct WriteCallbackInfo
  {
    WriteCallbackInfo(void* pv, size_t sizeDest)
      : ptrDest_(static_cast<uint8_t*>(pv)), sizeDest_(sizeDest), bytesRead_(0)
    {}

    uint8_t* ptrDest_;
    size_t sizeDest_;
    size_t bytesRead_;
  };

  static bool WriteCallbackFunction(std::string data, std::intptr_t userdata);
};

#endif