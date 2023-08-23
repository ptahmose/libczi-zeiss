#pragma once
#include "ImportExport.h"
#include "../libCZI/libCZI.h"
#include <memory>

namespace libCZIStream
{
    /// Creates a stream-object for the specified url.
    /// A stock-implementation of a stream-object (for reading a file from url) is provided here.
    /// \param url The URL of the file.
    /// \return The new stream object.
    LIBCZISTREAM_API std::shared_ptr<libCZI::IStream> CreateStreamFromURL(const wchar_t* url);
}
