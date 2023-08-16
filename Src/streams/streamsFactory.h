#pragma once

#include "../libCZI/libCZI.h"

class CStreams
{
public:
    enum class StreamType
    {
        Input,
        Output,
        InputOutput
    };

    struct StreamObjectInfo
    {
        std::string className;
        StreamType  type;
        std::string shortExplanation;
    };

    static const std::vector<StreamObjectInfo>& GetListOfAvailableStreamObjects();

    /// Creates a stream-object of the specified type and (if the className was valid) initializes it with the
    /// specified string 'filename'. If the the 'className' is not identifying a known class, then nullptr is
    /// returned.
    ///
    /// \param  className   Name of the stream-object class to create.
    /// \param  filename    A string identifying the stream to open. Interpretation depends on the actual stream-object created.
    ///
    /// \returns    The new stream.
    static std::shared_ptr<libCZI::IStream> CreateStream(const std::string& className, const std::string& filename);
};
