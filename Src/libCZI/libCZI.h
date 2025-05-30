// SPDX-FileCopyrightText: 2017-2022 Carl Zeiss Microscopy GmbH
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <functional>
#include <memory>
#include <map>
#include <limits>
#include <string>
#include <vector>

#include "ImportExport.h"

#include "libCZI_exceptions.h"
#include "libCZI_DimCoordinate.h"
#include "libCZI_Pixels.h"
#include "libCZI_Metadata.h"
#include "libCZI_Metadata2.h"
#include "libCZI_Utilities.h"
#include "libCZI_Compositor.h"
#include "libCZI_Site.h"
#include "libCZI_compress.h"
#include "libCZI_StreamsLib.h"

// virtual d'tor -> https://isocpp.org/wiki/faq/virtual-functions#virtual-dtors

/// External interfaces, classes, functions and structs are found in the namespace "libCZI".
namespace libCZI
{
    /// Values that represent site-object-types.
    /// On Windows, we provide one Site-object that uses the WIC-codec and one that uses
    /// an internal JPEG-XR decoder (JXRLib).
    enum class SiteObjectType
    {
        Default,        ///< An enum constant representing the default option (which is JXRLib)
        WithJxrDecoder, ///< An enum constant representing a Site-object using the internal JXRLib.
        WithWICDecoder, ///< An enum constant representing a Site-object using the Windows WIC-codec. Note that this option is only available on Windows.
    };

    class ISite;

    /// Gets one of the available Site-objects. The objects returned are static objects with an
    /// unbounded lifetime.
    /// \param type The Site-object type.
    /// \return nullptr if it fails, else the default site object (of the specified type).
    LIBCZI_API ISite* GetDefaultSiteObject(libCZI::SiteObjectType type);

    /// Sets the global Site-object. This function must only be called once and before any other
    /// function in this library is called. The object passed in here must have a lifetime greater
    /// than any usage of the library.
    /// If no Site-object is set, then at first usage a default Site-object is created and used.
    /// \param [in] pSite The Site-object to use. It must not be nullptr.
    LIBCZI_API void SetSiteObject(libCZI::ISite* pSite);

    class ICZIReader;
    class ICziWriter;
    class ICziReaderWriter;
    class IStream;
    class IOutputStream;
    class IInputOutputStream;
    class ISubBlock;
    class IMetadataSegment;
    class ISubBlockRepository;
    class IAttachment;
    class ISubBlockCache;

    /// This structure contains information about the compiler settings and the version of the source
    /// which was used to create the library.
    struct BuildInformation
    {
        /// The compiler identification. This is a free-form string.
        std::string compilerIdentification;

        /// The URL of the repository - if available.
        std::string repositoryUrl;

        /// The branch - if available.
        std::string repositoryBranch;

        /// The tag or hash of the repository - if available.
        std::string repositoryTag;
    };

    /// Gets the version of the library. For versioning libCZI, SemVer2 (<https://semver.org/>) is used.
    /// Note that the value of the tweak version number does not have a meaning (as far as SemVer2 is concerned).
    ///
    /// \param [out] pMajor If non-null, will receive the major version number.
    /// \param [out] pMinor If non-null, will receive the minor version number.
    /// \param [out] pPatch If non-null, will receive the patch version number.
    /// \param [out] pTweak If non-null, will receive the tweak version number.
    LIBCZI_API void GetLibCZIVersion(int* pMajor, int* pMinor = nullptr, int* pPatch = nullptr, int* pTweak = nullptr);

    /// Gets information about the libCZI-library - e.g. how it was built.
    /// \param [out] info The information.
    LIBCZI_API void GetLibCZIBuildInformation(BuildInformation& info);

    /// Creates a new instance of the CZI-reader class.
    /// \return The newly created CZI-reader.
    LIBCZI_API std::shared_ptr<ICZIReader> CreateCZIReader();

    /// Options controlling the operation of a CZI-writer object. Those options are set at construction
    /// time and cannot be mutated afterwards.
    struct CZIWriterOptions
    {
        /// True if the writer should allow that duplicate subblocks are added. In general, it is
        /// not recommended to bypass the check for duplicate subblocks.
        bool allow_duplicate_subblocks{ false };
    };

    /// Creates a new instance of the CZI-writer class.
    /// \param  options (Optional) Options for controlling the operation. This argument may
    ///                 be null, in which case default options are used.
    /// \returns The newly created CZI-writer.
    LIBCZI_API std::shared_ptr<ICziWriter> CreateCZIWriter(const CZIWriterOptions* options = nullptr);

    /// Creates a new instance of the CZI-reader-writer class.
    /// \return The newly created CZI-reader-writer.
    LIBCZI_API std::shared_ptr<ICziReaderWriter> CreateCZIReaderWriter();

    /// This structure defines how to handle mismatches and discrepancies between sub-block information and the
    /// actual pixel data. Please see the documentation about "Resolution Protocol for Ambiguous or Contradictory Information"
    /// for details. For libCZI until version 0.63.2 the behavior was to throw an exception in case of a discrepancy
    /// detected.
    struct CreateBitmapOptions
    {
        /// In case of uncompressed pixel data, apply the resolution protocol for uncompressed data.
        /// If false, an exception is thrown  (in case of a discrepancy).
        bool handle_uncompressed_data_size_mismatch{ true };

        /// In case of JpgXR compressed pixel data, apply the resolution protocol for JpgXR-compressed data.
        /// If false, an exception is thrown  (in case of a discrepancy).
        bool handle_jpgxr_bitmap_mismatch{ true };

        /// In case of zstd compressed pixel data, apply the resolution protocol for zstd-compressed data.
        /// If false, an exception is thrown  (in case of a discrepancy).
        bool handle_zstd_data_size_mismatch{ true };
    };

    /// Creates bitmap from sub block.
    /// \param      subBlk  The sub-block.
    /// \param      options (Optional) Options for controlling the operation. This controls how discrepancies
    ///                     between the actual pixel data and the information in the sub-block are handled.
    /// \returns    The newly allocated bitmap containing the image from the sub-block.
    LIBCZI_API std::shared_ptr<IBitmapData>  CreateBitmapFromSubBlock(ISubBlock* subBlk, const CreateBitmapOptions* options = nullptr);

    /// Creates metadata-object from a metadata segment.
    /// \param [in] metadataSegment The metadata segment object.
    /// \return The newly created metadata object.
    LIBCZI_API std::shared_ptr<ICziMetadata> CreateMetaFromMetadataSegment(IMetadataSegment* metadataSegment);

    /// Creates an accessor of the specified type which uses the specified sub-block repository.
    /// \param repository   The sub-block repository.
    /// \param accessorType Type of the accessor.
    /// \return The newly created accessor object.
    LIBCZI_API std::shared_ptr<IAccessor> CreateAccesor(std::shared_ptr<ISubBlockRepository> repository, AccessorType accessorType);

    /// Creates a stream-object for the specified file.
    /// A stock-implementation of a stream-object (for reading a file from disk) is provided here.
    /// \param szFilename Filename of the file.
    /// \return The new stream object.
    LIBCZI_API std::shared_ptr<IStream> CreateStreamFromFile(const wchar_t* szFilename);

    /// Creates a stream-object on a memory-block.
    /// \param ptr  Shared pointer to a memory-block.
    /// \param dataSize Size of the memory-block.
    /// \return         The new stream object.
    LIBCZI_API std::shared_ptr<IStream> CreateStreamFromMemory(std::shared_ptr<const void> ptr, size_t dataSize);

    /// Creates a stream-object on the memory-block in an attachment.
    /// \param attachment   Pointer to attachment.
    /// \return         The new stream object.
    LIBCZI_API std::shared_ptr<IStream> CreateStreamFromMemory(IAttachment* attachment);

    /// Creates an output-stream-object for the specified filename. A stock-implementation of a
    /// stream-object (for writing a file from disk) is provided here. For a more specialized and
    /// tuned version, libCZI-users should consider implementing the interface "IOutputStream" in
    /// their own code.
    /// \param szwFilename       Filename of the file (as a wide character string).
    /// \param overwriteExisting True if an existing file should be overwritten, false otherwise.
    /// \return The new output-stream object.
    LIBCZI_API std::shared_ptr<IOutputStream> CreateOutputStreamForFile(const wchar_t* szwFilename, bool overwriteExisting);

    /// Creates an output-stream-object for the specified filename. A stock-implementation of a
    /// stream-object (for writing a file from disk) is provided here. For a more specialized and
    /// tuned version, libCZI-users should consider implementing the interface "IOutputStream" in
    /// their own code.
    /// \param szFilename        Filename of the file (in UTF8 encoding).
    /// \param overwriteExisting True if an existing file should be overwritten, false otherwise.
    /// \return The new output-stream object.
    LIBCZI_API std::shared_ptr<IOutputStream> CreateOutputStreamForFileUtf8(const char* szFilename, bool overwriteExisting);

    /// Creates an input-output-stream for the specified filename.
    /// A stock-implementation of a stream-object (for modifying a file from disk) is provided here. For a more specialized and tuned version, libCZI-users should consider
    /// implementing the interface "IInputOutputStream" in their own code.
    /// \param szFilename Filename of the file.
    /// \return The newly created input-output-stream object for the file if successful.
    LIBCZI_API std::shared_ptr<IInputOutputStream> CreateInputOutputStreamForFile(const wchar_t* szFilename);

    /// Creates a metadata-builder-object.
    /// \return The newly created metadata-builder-object.
    LIBCZI_API std::shared_ptr<ICziMetadataBuilder> CreateMetadataBuilder();

    /// Creates a sub block cache object.
    /// \returns    The newly created sub block cache.
    LIBCZI_API std::shared_ptr<ISubBlockCache> CreateSubBlockCache();

    /// Creates metadata builder object from the specified UTF8-encoded XML-string. If the XML is
    /// invalid or if the root-node "ImageDocument" is not present, then an exception is thrown.
    /// \param  xml The UTF8-encoded XML string.
    /// \return The newly created metadata-builder-object.
    LIBCZI_API std::shared_ptr<ICziMetadataBuilder> CreateMetadataBuilderFromXml(const std::string& xml);

    /// Interface used for accessing the data-stream.  
    /// Implementations of this interface are expected to be thread-safe - it should be possible to
    /// call the Read-method from multiple threads simultaneously.
    /// In libCZI-usage, exceptions thrown by Read-method are wrapped into a libCZI::LibCZIIOException-exception,
    /// where the exception thrown by the Read-method is stored as the inner exception.
    class IStream
    {
    public:
        /// Reads the specified amount of data from the stream at the specified position. This method
        /// is expected to throw an exception for any kind of I/O-related error. It must not throw
        /// an exception if reading past the end of a file - instead, it must return the number of
        /// bytes actually read accordingly.
        /// For the special case of size==0, the behavior should be as follows: the method should
        /// operate as for a size>0, but it should not read any data. The method should return 0 in
        /// ptrBytesRead.
        ///
        /// \param offset                The offset to start reading from.
        /// \param [out] pv              The caller-provided buffer for the data. Must be non-null.
        /// \param size                  The size of the buffer.
        /// \param [out] ptrBytesRead    If non-null, the variable pointed to will receive the number of bytes actually read.
        virtual void Read(std::uint64_t offset, void* pv, std::uint64_t size, std::uint64_t* ptrBytesRead) = 0;

        virtual ~IStream() = default;
    };

    /// Interface used for writing a data-stream. The abstraction used is:
    /// - It is possible to write to arbitrary positions.  
    /// - The end of the stream is defined by the highest position written to.  
    /// - If write-operations occur on non-consecutive positions, then the gaps can be assumed to have arbitrary content.  
    class IOutputStream
    {
    public:

        /// Writes the specified data to the stream, starting at the position specified by "offset".
        ///
        /// \param          offset          The offset into the stream where to start the write-operation.
        /// \param          pv              Pointer to the data.
        /// \param          size            The size of the data.
        /// \param [in,out] ptrBytesWritten If non-null, then the number of bytes actually written will be put here.
        virtual void Write(std::uint64_t offset, const void* pv, std::uint64_t size, std::uint64_t* ptrBytesWritten) = 0;
        virtual ~IOutputStream() = default;
    };

    /// Interface for a read-write-stream. 
    class IInputOutputStream : public IStream, public IOutputStream
    {
    };

    /// Information about a sub-block.
    struct SubBlockInfo
    {
        /// The (raw) compression mode identification of the sub-block. This value is not interpreted, use "GetCompressionMode" to have it
        /// converted to the CompressionMode-enumeration. Note that unknown compression-mode identifiers (unknown to libCZI) 
        /// are mapped to CompressionMode::Invalid.
        std::int32_t            compressionModeRaw;

        /// The pixel type of the sub-block.
        PixelType               pixelType;

        /// The coordinate of the sub-block.
        libCZI::CDimCoordinate  coordinate;

        /// The rectangle where the bitmap (in this sub-block) is located.
        libCZI::IntRect         logicalRect;

        /// The physical size of the bitmap (which may be different to the size of logicalRect).
        libCZI::IntSize         physicalSize;

        /// The M-index of the sub-block (if available). If not available, it has the value std::numeric_limits<int>::max() or std::numeric_limits<int>::min().
        int                     mIndex;

        /// This field indicates the "pyramid-type" of the sub-block. The significance and importance of this field is unclear, and is considered
        /// legacy. It is recommended to ignore this field.
        SubBlockPyramidType     pyramidType;

        /// Calculate a zoom-factor from the physical- and logical size.
        /// \remark
        /// This calculation not really well-defined.
        /// \return The zoom factor.
        double                  GetZoom() const
        {
            if (this->physicalSize.w > this->physicalSize.h)
            {
                return static_cast<double>(this->physicalSize.w) / this->logicalRect.w;
            }

            return static_cast<double>(this->physicalSize.h) / this->logicalRect.h;
        }

        /// Gets compression mode enumeration. Note that unknown compression-mode identifiers (unknown to libCZI)
        /// are mapped to CompressionMode::Invalid.
        /// \returns The compression mode enumeration.
        CompressionMode         GetCompressionMode() const
        {
            return Utils::CompressionModeFromRawCompressionIdentifier(this->compressionModeRaw);
        }

        /// Query if the M-index is valid.
        /// \returns True if the M-index is valid, false if not.
        bool                    IsMindexValid() const
        {
            return libCZI::Utils::IsValidMindex(this->mIndex);
        }
    };

    /// This structure is extending SubBlockInfo with information specific to the 
    /// subblock-directory.
    struct DirectorySubBlockInfo : SubBlockInfo
    {
        std::uint64_t filePosition; ///< The file position of the subblock.
    };

    /// Representation of a sub-block. A sub-block can contain three types of data: the bitmap-data,
    /// an attachment and metadata. The presence of an attachment is optional.
    class ISubBlock
    {
    public:
        /// Values that represent the three different data types found in a sub-block.
        enum MemBlkType
        {
            Metadata,   ///< An enum constant representing the metadata.
            Data,       ///< An enum constant representing the bitmap-data.
            Attachment  ///< An enum constant representing the attachment (of a sub-block).
        };

        /// Gets sub-block information.
        /// \return The sub-block information.
        virtual const SubBlockInfo& GetSubBlockInfo() const = 0;

        ///  Get a pointer to the raw data. Note that the pointer returned is only valid during the
        ///  lifetime of the sub-block-object.
        /// \param type          The sub-block data-type.
        /// \param [out] ptr     The pointer to the data is stored here.
        /// \param [out] size    The size of the data.
        virtual void DangerousGetRawData(MemBlkType type, const void*& ptr, size_t& size) const = 0;

        /// Gets raw data.
        /// \param type             The type.
        /// \param [out] ptrSize    If non-null, size of the data buffer is stored here.
        /// \return The raw data.
        virtual std::shared_ptr<const void> GetRawData(MemBlkType type, size_t* ptrSize) = 0;

        /// Creates a bitmap (from the data of this sub-block).
        /// \remark
        /// Within this call the bitmap is decoded (if necessary).
        /// In current implementation, the sub-block does not hold a reference to the returned
        /// bitmap here (and, if called twice, a new bitmap is created). One should not rely
        /// on this behavior, it is conceivable that in a later version the sub-block will
        /// keep a reference (and return the same bitmap if called twice).
        /// In current version this method is equivalent to calling CreateBitmapFromSubBlock.
        /// \return The bitmap (contained in this sub-block).
        virtual std::shared_ptr<IBitmapData> CreateBitmap(const CreateBitmapOptions* options = nullptr) = 0;

        virtual ~ISubBlock() = default;

        /// A helper method used to cast the pointer to a specific type.
        /// \param type          The sub-block data-type.
        /// \param [out] ptr     The pointer to the data is stored here.
        /// \param [out] size    The size of the data.
        template <class Q>
        void DangerousGetRawData(MemBlkType type, const Q*& ptr, size_t& size) const
        {
            const void* p;
            this->DangerousGetRawData(type, p, size);
            ptr = static_cast<const Q*>(p);
        }
    };

    /// Information about an attachment.
    struct AttachmentInfo
    {
        libCZI::GUID    contentGuid;            ///< A Guid identifying the content of the attachment.
        char            contentFileType[9];     ///< A null-terminated character array identifying the content of the attachment.
        std::string     name;                   ///< A string identifying the content of the attachment.
    };

    /// Representation of an attachment. An attachment is a binary blob, its inner structure is opaque.
    class IAttachment
    {
    public:
        /// Gets information about the attachment.
        /// \return The attachment information.
        virtual const AttachmentInfo& GetAttachmentInfo() const = 0;

        ///  Get a pointer to the raw data. Note that the pointer returned is only valid during the
        ///  lifetime of the sub-block-object.
        /// \param [out] ptr     The pointer to the data is stored here.
        /// \param [out] size    The size of the data.
        virtual void DangerousGetRawData(const void*& ptr, size_t& size) const = 0;

        /// Gets raw data.
        /// \param [out] ptrSize    If non-null, size of the data buffer is stored here.
        /// \return The raw data.
        virtual std::shared_ptr<const void> GetRawData(size_t* ptrSize) = 0;

        virtual ~IAttachment() = default;

        /// A helper method used to cast the pointer to a specific type.
        /// \param [out] ptr     The pointer to the data is stored here.
        /// \param [out] size    The size of the data.
        template <class Q>
        void DangerousGetRawData(const Q*& ptr, size_t& size) const
        {
            const void* p;
            this->DangerousGetRawData(p, size);
            ptr = static_cast<Q*>(p);
        }
    };

    /// Interface representing the metadata-segment.
    class IMetadataSegment
    {
    public:
        /// Values that represent the two different data types found in the metadata-segment.
        enum MemBlkType
        {
            XmlMetadata,    ///< The metadata (in UTF8-XML-format)
            Attachment      ///< The attachment (not currently used).
        };

        /// Gets raw data.
        /// \param type             The metadata-segment memory-block type.
        /// \param [out] ptrSize    If non-null, thus size of the data (in bytes) is stored here.
        /// \return The raw data.
        virtual std::shared_ptr<const void> GetRawData(MemBlkType type, size_t* ptrSize) = 0;

        ///  Get a pointer to the raw data. Note that the pointer returned is only valid during the
        ///  lifetime of the sub-block-object.
        /// \param type          The metadata-segment memory-block type.
        /// \param [out] ptr     The pointer to the data is stored here.
        /// \param [out] size    The size of the data.
        virtual void DangerousGetRawData(MemBlkType type, const void*& ptr, size_t& size) const = 0;

        virtual ~IMetadataSegment() = default;

        /// Creates metadata object from this metadata segment.
        /// \return The newly created metadata object.
        std::shared_ptr<ICziMetadata> CreateMetaFromMetadataSegment() { return libCZI::CreateMetaFromMetadataSegment(this); }
    };

    /// This structure gathers the bounding-boxes determined from all sub-blocks and only be those on pyramid-layer 0.
    struct BoundingBoxes
    {
        /// The bounding-box determined from all sub-blocks.
        IntRect boundingBox;

        /// The bounding-boxes determined only from sub-blocks of pyramid-layer 0.
        IntRect boundingBoxLayer0;
    };

    /// Statistics about all sub-blocks found in a CZI-document.
    struct SubBlockStatistics
    {
        /// The total number of sub-blocks in the CZI-document.
        /// We are counting here all sub-block (no matter on which pyramid-layer).
        int subBlockCount;

        /// The minimum M-index (determined from all sub-blocks in the document with a valid M-index).
        /// If no valid M-index was present, then this member will have the value std::numeric_limits<int>::max().
        int minMindex;

        /// The maximum M-index (determined from all sub-blocks in the document with a valid M-index).
        /// If no valid M-index was present, then this member will have the value std::numeric_limits<int>::min().
        int maxMindex;

        /// The minimal axis-aligned-bounding-box determined from all logical coordinates of all sub-blocks in the 
        /// document.
        IntRect boundingBox;

        /// The minimal axis-aligned-bounding box determined only from the logical coordinates of the sub-blocks on pyramid-layer0 in the 
        /// document. The top-left corner of this bounding-box gives the coordinate of the origin of the 'CZI-Pixel-Coordinate-System' in
        /// the coordinate system used by libCZI (which is referred to as 'raw-subblock-coordinate-system'). See [Coordinate Systems](../pages/coordinate_systems.html) for
        /// additional information.
        IntRect boundingBoxLayer0Only;

        /// The dimension bounds - the minimum and maximum dimension index determined
        /// from all sub-blocks in the CZI-document.
        CDimBounds dimBounds;

        /// A map with key scene-index and value bounding box of the scene.
        /// Two bounding-boxes are determined - one from checking all sub-blocks (with the specific scene-index)
        /// and another one by only considering sub-blocks on pyramid-layer 0.
        /// If no scene-indices are present, this map is empty.
        std::map<int, BoundingBoxes> sceneBoundingBoxes;

        /// Query if the members minMindex and maxMindex are valid. They may be
        /// invalid in the case that the sub-blocks do not define an M-index.
        ///
        /// \return True if minMindex and maxMindex are valid, false if not.
        bool IsMIndexValid() const
        {
            return this->minMindex <= this->maxMindex ? true : false;
        }

        /// Invalidates this object.
        void Invalidate()
        {
            this->subBlockCount = -1;
            this->boundingBox.Invalidate();
            this->boundingBoxLayer0Only.Invalidate();
            this->dimBounds.Clear();
            this->sceneBoundingBoxes.clear();
            this->minMindex = (std::numeric_limits<int>::max)();
            this->maxMindex = (std::numeric_limits<int>::min)();
        }
    };

    /// Statistics about the pyramid-layers.
    struct PyramidStatistics
    {
        /// Information about the pyramid-layer.
        /// It consists of two parts: the minification factor and the layer number.
        /// The minification factor specifies by which factor two adjacent pyramid-layers are shrunk. Commonly used in
        /// CZI are 2 or 3.
        /// The layer number starts with 0 with the highest resolution layer.
        /// The lowest level (layer 0) is denoted by pyramidLayerNo == 0 AND minificationFactor==0.
        /// Another special case is pyramidLayerNo == 0xff AND minificationFactor==0xff which means that the
        /// pyramid-layer could not be determined (=the minification factor could not unambiguously be correlated to
        /// a pyramid-layer).
        struct PyramidLayerInfo
        {
            std::uint8_t minificationFactor;    ///< Factor by which adjacent pyramid-layers are shrunk. Commonly used in CZI are 2 or 3.
            std::uint8_t pyramidLayerNo;        ///< The pyramid layer number.

            /// Query if this object represents layer 0 (=no minification).
            ///
            /// \return True if representing layer 0, false if not.
            bool IsLayer0() const { return this->minificationFactor == 0 && this->pyramidLayerNo == 0; }

            /// Query if this object represents the set of subblocks which cannot be represented as pyramid-layers.
            ///
            /// \return True if the set of "not representable as pyramid-layer" is represented by this object, false if not.
            bool IsNotIdentifiedAsPyramidLayer() const { return this->minificationFactor == 0xff && this->pyramidLayerNo == 0xff; }
        };

        /// Information about a pyramid-layer.
        struct PyramidLayerStatistics
        {
            PyramidLayerInfo    layerInfo;  ///< This identifies the pyramid-layer.
            int                 count;      ///< The number of sub-blocks which are present in the pyramid-layer.
        };

        /// A map with key "scene-index" and value "list of subblock-counts per pyramid-layer".
        /// A key with value std::numeric_limits<int>::max() is used in case that the scene-index is not valid.
        std::map<int, std::vector<PyramidLayerStatistics>> scenePyramidStatistics;
    };

    /// Interface for sub-block repository. This interface is used to access the sub-blocks in a CZI-file.
    class LIBCZI_API ISubBlockRepository
    {
    public:
        /// Enumerate all sub-blocks. 
        /// \param funcEnum The functor which will be called for every sub-block. If the return value of the
        ///                 functor is true, the enumeration is continued, otherwise it is stopped.
        ///                 The first argument is the index of the sub-block and the second is providing
        ///                 information about the sub-block.
        virtual void EnumerateSubBlocks(const std::function<bool(int index, const SubBlockInfo& info)>& funcEnum) = 0;

        /// Enumerate the subset of sub-blocks defined by the parameters.
        /// \param planeCoordinate The plane coordinate. Only sub-blocks on this plane will be considered.
        /// \param roi             The ROI - only sub-blocks which intersects with this ROI will be considered.
        /// \param onlyLayer0      If true, then only sub-blocks on pyramid-layer 0 will be considered.
        /// \param funcEnum The functor which will be called for every sub-block. If the return value of the
        ///                 functor is true, the enumeration is continued, otherwise it is stopped.
        ///                 The first argument is the index of the sub-block and the second is providing
        ///                 information about the sub-block.
        virtual void EnumSubset(const IDimCoordinate* planeCoordinate, const IntRect* roi, bool onlyLayer0, const std::function<bool(int index, const SubBlockInfo& info)>& funcEnum) = 0;

        /// Reads the sub-block identified by the specified index. If there is no sub-block present (for
        /// the specified index) then an empty shared_ptr is returned. If a different kind of problem
        /// occurs (e. g. I/O error or corrupted data) an exception is thrown.
        /// \param index Index of the sub-block (as reported by the Enumerate-methods).
        /// \return If successful, the sub-block object; otherwise an empty shared_ptr.
        virtual std::shared_ptr<ISubBlock> ReadSubBlock(int index) = 0;

        /// Attempts to get subblock information of an arbitrary subblock in of the specified channel.
        /// The purpose is that it is quite often necessary to determine the pixeltype of a channel - and
        /// if we do not want to/cannot rely on metadata for determining this, then the obvious way is to
        /// look at an (arbitrary) subblock. In order to allow the repository to have this information
        /// available fast (i. e. cached) we introduce a specific method for this purpose. A cornerstone
        /// case is when no subblock has a channel-index - the rule is: if no subblock has channel-
        /// information, then a channelIndex of 0 fits. Otherwise a subblock is a match if the channel-
        /// index is an exact match.
        /// \param channelIndex  The channel index.
        /// \param [out] info The sub-block information (will be set only if the method is successful).
        /// \return true if it succeeds, false if it fails.
        virtual bool TryGetSubBlockInfoOfArbitrarySubBlockInChannel(int channelIndex, SubBlockInfo& info) = 0;

        /// Attempts to get the subblock information of the subblock with the specified index. If the specified
        /// index is not valid, then false is returned.
        /// \param          index   Index of the subblock to query information for.
        /// \param [in,out] info    If non-null and operation is successful, then the information is put here.
        /// \returns    True if it succeeds; false otherwise.
        virtual bool TryGetSubBlockInfo(int index, SubBlockInfo* info) const = 0;

        /// Gets the statistics about the sub-blocks (determined from examining all sub-blocks).
        /// \return The sub-block statistics.
        virtual SubBlockStatistics GetStatistics() = 0;

        /// Gets the statistics about the pyramid-layers. This information is constructed from all T, Z, C, ...
        /// Pyramids are constructed per scene in CZI.
        ///
        /// \return The pyramid statistics.
        virtual PyramidStatistics GetPyramidStatistics() = 0;

        /// Transform the specified point from one coordinate system to another.
        ///
        /// \param  source_point                    Source point and specification of the coordinate system it is defined in.
        /// \param  destination_frame_of_reference  Identifies the coordinate system to which the point should be transformed.
        ///
        /// \returns    The transformed point.
        virtual libCZI::IntPointAndFrameOfReference TransformPoint(const libCZI::IntPointAndFrameOfReference& source_point, libCZI::CZIFrameOfReference destination_frame_of_reference) = 0;

        virtual ~ISubBlockRepository() = default;

        /// Transform the specified rectangle from one coordinate system to another.
        ///
        /// \param  source_rectangle                Source rectangle and specification of the coordinate system it is defined in.
        /// \param  destination_frame_of_reference  Identifies the coordinate system to which the point should be transformed.
        ///
        /// \returns    The transformed rectangle.
        libCZI::IntRectAndFrameOfReference TransformRectangle(const libCZI::IntRectAndFrameOfReference& source_rectangle, libCZI::CZIFrameOfReference destination_frame_of_reference)
        {
            libCZI::IntPointAndFrameOfReference source_point_and_frame_of_reference;
            source_point_and_frame_of_reference.frame_of_reference = source_rectangle.frame_of_reference;
            source_point_and_frame_of_reference.point = { source_rectangle.rectangle.x, source_rectangle.rectangle.y };
            libCZI::IntPoint transformed_point_upper_left = this->TransformPoint(source_point_and_frame_of_reference, destination_frame_of_reference).point;
            source_point_and_frame_of_reference.point = { source_rectangle.rectangle.x + source_rectangle.rectangle.w, source_rectangle.rectangle.y + source_rectangle.rectangle.h };
            libCZI::IntPointAndFrameOfReference transformed_point_lower_right = this->TransformPoint(source_point_and_frame_of_reference, destination_frame_of_reference);
            return
            {
                transformed_point_lower_right.frame_of_reference,
                {
                    transformed_point_upper_left.x,
                    transformed_point_upper_left.y,
                    transformed_point_lower_right.point.x - transformed_point_upper_left.x,
                    transformed_point_lower_right.point.y - transformed_point_upper_left.y
                }
            };
        }
    };

    /// Additional functionality for the subblock-repository, providing some specialized and not commonly used functionality.
    class LIBCZI_API ISubBlockRepositoryEx
    {
    public:
        /// Enumerate all sub-blocks and provide extended information. 
        /// \param funcEnum The functor which will be called for every sub-block. If the return value of the
        ///                 functor is true, the enumeration is continued, otherwise it is stopped.
        ///                 The first argument is the index of the sub-block and the second is providing
        ///                 information about the sub-block.
        virtual void EnumerateSubBlocksEx(const std::function<bool(int index, const DirectorySubBlockInfo& info)>& funcEnum) = 0;

        virtual ~ISubBlockRepositoryEx() = default;
    };

    /// Interface for the attachment repository. This interface is used to access the attachments in a CZI-file.
    class LIBCZI_API IAttachmentRepository
    {
    public:
        /// Gets the number of attachments available in the repository.
        ///
        /// \returns    The attachment count.
        virtual int GetAttachmentCount() const = 0;

        /// Attempts to get the attachment information of the attachment with the specified index. If the specified
        /// index is not valid, then false is returned.
        /// \param          index   Index of the attachment to query information for.
        /// \param [out]    info    If non-null and operation is successful, then the information is put here.
        /// \returns    True if it succeeds; false otherwise.
        virtual bool TryGetAttachmentInfo(int index, AttachmentInfo* info) const = 0;

        /// Enumerate all attachments.
        ///
        /// \param funcEnum The functor which will be called for every attachment. If the return value of the
        ///                 functor is true, the enumeration is continued, otherwise it is stopped.
        ///                 The first argument is the index of the attachment and the second is providing
        ///                 information about the attachment.
        virtual void EnumerateAttachments(const std::function<bool(int index, const AttachmentInfo& info)>& funcEnum) = 0;

        /// Enumerate the subset of the attachments defined by the parameters.
        /// \param contentFileType If non-null, only attachments with this contentFileType will be considered.
        /// \param name            If non-null, only attachments with this name will be considered.
        /// \param funcEnum The functor which will be called for every attachment (within the subset). If the return value of the
        ///                 functor is true, the enumeration is continued, otherwise it is stopped.
        ///                 The first argument is the index of the attachment and the second is providing
        ///                 information about the attachment.
        virtual void EnumerateSubset(const char* contentFileType, const char* name, const std::function<bool(int index, const AttachmentInfo& info)>& funcEnum) = 0;

        /// Reads the attachment identified by the specified index. If there is no attachment present (for
        /// the specified index) then an empty shared_ptr is returned. If a different kind of problem
        /// occurs (e. g. I/O error or corrupted data) an exception is thrown.
        /// \param index Index of the attachment (as reported by the Enumerate-methods).
        /// \return If successful, the attachment object; otherwise an empty shared_ptr.
        virtual std::shared_ptr<IAttachment> ReadAttachment(int index) = 0;

        virtual ~IAttachmentRepository() = default;
    };

    /// Global information about the CZI-file (from the CZI-fileheader-segment).
    struct FileHeaderInfo
    {
        /// The file-GUID of the CZI. Note: CZI defines two GUIDs, this is the "FileGuid". Multi-file containers 
        /// (for which the other GUID "PrimaryFileGuid" is used) are not supported by libCZI currently.
        libCZI::GUID fileGuid;
        int majorVersion;   ///< The major version.
        int minorVersion;   ///< The minor version.
    };

    /// This interface is used to represent the CZI-file.
    /// A note on thread-safety - all methods of this interface may be called from multiple threads concurrently.
    class LIBCZI_API ICZIReader : public ISubBlockRepository, public ISubBlockRepositoryEx, public IAttachmentRepository
    {
    public:
        /// This structure gathers the settings for controlling the 'Open' operation of the CZIReader-class.
        struct LIBCZI_API OpenOptions
        {
            /// This enum is used to specify the policy which defines which information is considered authoritative (in the description
            /// of a sub-block) - either the information in the sub-block directory or in the sub-block header. Also, it
            /// controls how to handle a discrepancy here - either throw an exception if a discrepancy is encountered or ignore
            /// a discrepancy (and go with the respective information for decoding a bitmap as is).
            /// Note that the values defined here are used to define a bit-field. The first bit (bit 0) is used to
            /// distinguish between sub-block-directory precedence and sub-block-header precedence. The value 'PrecedenceMask'
            /// is used to mask this bit. Bit 7 is used to indicate whether a discrepancy is to be ignored or whether an error
            /// is to be reported.
            /// Historically, libCZI (up to version 0.63.2) used to give precedence fo the sub-block header information,
            /// and it did not report a discrepancy.
            enum class SubBlockDirectoryInfoPolicy : std::uint8_t
            {
                SubBlockDirectoryPrecedence = 0, ///< The sub-block-directory information is used for the sub-blocks.
                SubBlockHeaderPrecedence = 1,    ///< The sub-block information is used for the sub-blocks.
                PrecedenceMask = 1,              ///< Bit-mask allowing to extract the relevant bits for "precedence".
                IgnoreDiscrepancy = 0x80,        ///< Flag allowing to choose whether a discrepancy is to be ignored (true) or whether an exception is to be thrown (false) when accessing the sub-block.
            };

            /// This option controls whether the lax parameter validation when parsing the dimension-entry of a subblock is to be used.
            /// Previous versions of libCZI did not check whether certain values in the file have the expected value. If those values
            /// are different than expected, this meant that libCZI would not be able to deal with the document properly.
            /// If lax checking of this is disabled, then Open will fail with a corresponding exception.
            /// The default is to enable lax checking (for compatibility with previous libCZI-versions), but users are encouraged to
            /// disable this for new code.
            bool lax_subblock_coordinate_checks{ true };

            /// This option controls whether the size-M-attribute of a pyramid-subblocks is to be ignored (when parsing and validating
            /// the dimension-entry of a subblock). This flag is only relevant if strict validation is enabled (i.e. lax_subblock_coordinate_checks
            /// is 'false'). If lax_subblock_coordinate_checks is true, then this flag has no effect.
            /// This is useful as some versions of software creating CZI-files used to write bogus values for size-M, and those files
            /// would otherwise not be usable with strict validation enabled. If this bogus size-M is ignored, then the files can be used
            /// without problems.
            bool ignore_sizem_for_pyramid_subblocks{ false };

            /// The default frame-of-reference which is to be used by the reader-object. This determines which frame-of-reference
            /// is used when the enum value "CZIFrameOfReference::Default" is used with an operation of the reader-object.
            /// If the value specified here is "CZIFrameOfReference::Invalid" or "CZIFrameOfReference::Default", then
            /// "CZIFrameOfReference::RawSubBlockCoordinateSystem" will be used.
            libCZI::CZIFrameOfReference default_frame_of_reference{ libCZI::CZIFrameOfReference::Invalid };

            /// This bitfield is used to specify the policy which information is considered authoritative in the construction of a sub-block -
            /// either the information in the sub-block directory or in the sub-block header. Also, it controls how to handle a discrepancy
            /// in this respect - either throw an exception if a discrepancy is encountered or ignore it.
            SubBlockDirectoryInfoPolicy subBlockDirectoryInfoPolicy{ SubBlockDirectoryInfoPolicy::SubBlockDirectoryPrecedence };

            /// Sets the default.
            void SetDefault()
            {
                this->lax_subblock_coordinate_checks = true;
                this->ignore_sizem_for_pyramid_subblocks = false;
                this->default_frame_of_reference = libCZI::CZIFrameOfReference::Invalid;
                this->subBlockDirectoryInfoPolicy = SubBlockDirectoryInfoPolicy::SubBlockDirectoryPrecedence;
            }
        };

        /// Opens the specified stream and reads the global information from the CZI-document. The stream
        /// passed in will have its refcount incremented, a reference is held until Close is called (or
        /// the instance is destroyed).
        /// 
        /// \remark
        /// If this method is called twice (assuming successful return), then an exception of type std::logic_error is
        /// thrown.
        ///
        /// \param  stream  The stream object.
        /// \param  options (Optional) Options for controlling the operation. If nullptr is given here, then the default settings are used.
        virtual void Open(const std::shared_ptr<IStream>& stream, const OpenOptions* options = nullptr) = 0;

        /// Gets the file header information.
        /// \return The file header information.
        virtual FileHeaderInfo GetFileHeaderInfo() = 0;

        /// Reads the metadata segment from the stream.
        /// \remark
        /// If the class is not operational (i.e. Open was not called or Open was not successful), then an exception of type std::logic_error is thrown.
        ///
        /// \return The metadata segment.
        virtual std::shared_ptr<IMetadataSegment> ReadMetadataSegment() = 0;

        /// Creates an accessor for the sub-blocks.
        /// See also the various typed methods: `CreateSingleChannelTileAccessor`, `CreateSingleChannelPyramidLayerTileAccessor` and `CreateSingleChannelScalingTileAccessor`.
        /// \remark
        /// If the class is not operational (i.e. Open was not called or Open was not successful), then an exception of type std::logic_error is thrown.
        ///
        /// \param accessorType The type of the accessor.
        ///
        /// \return The accessor (of the requested type).
        virtual std::shared_ptr<IAccessor> CreateAccessor(AccessorType accessorType) = 0;

        /// Closes CZI-reader. The underlying stream-object will be released, and further calls to
        /// other methods will fail. The stream is also closed when the object is destroyed, so it
        /// is usually not necessary to explicitly call `Close`. Note that the stream is not closed
        /// immediately (or - there is no guarantee that on return from this call all references to the
        /// stream object are released). Concurrently executing operations continue to use the stream
        /// and keep it referenced until they are finished.
        virtual void Close() = 0;
    public:
        /// Creates a single channel tile accessor.
        /// \return The new single channel tile accessor.
        std::shared_ptr<ISingleChannelTileAccessor>  CreateSingleChannelTileAccessor()
        {
            return std::dynamic_pointer_cast<ISingleChannelTileAccessor, IAccessor>(this->CreateAccessor(libCZI::AccessorType::SingleChannelTileAccessor));
        }

        /// Creates a single channel pyramid-layer accessor.
        /// \return The new single channel tile accessor.
        std::shared_ptr<ISingleChannelPyramidLayerTileAccessor>  CreateSingleChannelPyramidLayerTileAccessor()
        {
            return std::dynamic_pointer_cast<ISingleChannelPyramidLayerTileAccessor, IAccessor>(this->CreateAccessor(libCZI::AccessorType::SingleChannelPyramidLayerTileAccessor));
        }

        /// Creates a single channel scaling tile accessor.
        /// \return The new single channel scaling tile accessor.
        std::shared_ptr<ISingleChannelScalingTileAccessor> CreateSingleChannelScalingTileAccessor()
        {
            return std::dynamic_pointer_cast<ISingleChannelScalingTileAccessor, IAccessor>(this->CreateAccessor(libCZI::AccessorType::SingleChannelScalingTileAccessor));
        }
    };
}

#include "libCZI_Helpers.h"
#include "libCZI_Write.h"
#include "libCZI_ReadWrite.h"
