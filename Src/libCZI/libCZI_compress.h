// SPDX-FileCopyrightText: 2017-2022 Carl Zeiss Microscopy GmbH
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <memory>
#include <cstdint>
#include <functional>
#include <map>
#include <type_traits>
#include "libCZI.h"

namespace libCZI
{
    /// Values that represent well-known keys for the compression-parameters property bag. Note that
    /// the property-bag API is modeled with an int as key, which is by intention in order to allow
    /// for private keys.
    enum class CompressionParameterKey
    {
        /// This gives the "raw" zstd compression level aka "ExplicitLevel" (type: int32). If value is out-of-range, it will be clipped.
        /// This parameter is used with "zstd0" and "zstd1" compression schemes.
        ZSTD_RAWCOMPRESSIONLEVEL = 1,

        /// Whether to do the "lo-hi-byte-packing" preprocessing (type: boolean).
        /// This parameter is used with the "zstd1" compression scheme only.
        ZSTD_PREPROCESS_DOLOHIBYTEPACKING = 2,

        /// The quality parameter for the jxrlib encoder (type: uint32). The range is from 0 to 1000, where 1000
        /// gives the best quality (i.e. loss-less compression). This parameter is used with the "jxrlib" compression scheme only.
        /// If value is out-of-range, it will be clipped.
        JXRLIB_QUALITY = 3,

        CHUNKEDCOMPRESSION_MAXCHUNKSIZE = 4, ///< The maximum chunk size (in bytes) to be used for chunked compression (type: uint32). 
                                             ///< This parameter is used with the "chunked" compression scheme only.
        CHUNKEDCOMPRESSION_CODEC = 5,        ///< The codec to be used for chunked compression (type: uint32, where the value is interpreted as a ChunkedCompressionHeaderHelper::Codec enum value). 
                                             ///< This parameter is used with the "chunked" compression scheme only.
        CHUNKEDCOMPRESSION_RAWCOMPRESSIONLEVEL_ZSTD = 6, ///< The "raw" zstd compression level aka "ExplicitLevel" (type: int32) to be used for chunked compression. If value is out-of-range, it will be clipped.
                                                         ///< This parameter is used with the "chunked" compression scheme only, and only if the codec for chunked compression is set to zstd.
    };

    /// Simple variant type used for the compression-parameters-property-bag.
    struct LIBCZI_API CompressParameter
    {
        /// Values that represent the type represented by this variant.
        enum class Type
        {
            Invalid,    ///< An enum constant representing the 'invalid' type (so this instance has no value).
            Int32,      ///< An enum constant representing the 'int32' type.
            Uint32,     ///< An enum constant representing the 'uint32' type.
            Boolean     ///< An enum constant representing the 'boolean' type.
        };

        /// Default constructor - setting the variant to 'invalid'.
        CompressParameter() : type(Type::Invalid)
        {
        }

        /// Constructor for initializing the 'int32' type.
        ///
        /// \param  v   The value to set the variant to.
        explicit CompressParameter(std::int32_t v)
        {
            this->SetInt32(v);
        }

        /// Constructor for initializing the 'uint32' type.
        ///
        /// \param  v   The value to set the variant to.
        explicit CompressParameter(std::uint32_t v)
        {
            this->SetUInt32(v);
        }

        /// Constructor for initializing the 'bool' type.
        ///
        /// \param  v   The value to set the variant to.
        explicit CompressParameter(bool v)
        {
            this->SetBoolean(v);
        }

        /// Sets the type of the variant to "int32" and the value to the specified value.
        ///
        /// \param  v   The value to be set.
        void SetInt32(std::int32_t v)
        {
            this->type = Type::Int32;
            this->int32Value = v;
        }

        /// Sets the type of the variant to "uint32" and the value to the specified value.
        ///
        /// \param  v   The value to be set.
        void SetUInt32(std::uint32_t v)
        {
            this->type = Type::Uint32;
            this->uint32Value = v;
        }

        /// Sets the type of the variant to "boolean" and the value to the specified value.
        ///
        /// \param  v   The value to be set.
        void SetBoolean(bool v)
        {
            this->type = Type::Boolean;
            this->boolValue = v;
        }

        /// Gets the type which is represented by the variant.
        ///
        /// \returns    The type.
        Type GetType() const { return this->type; }

        /// If the type of the variant is "Int32", then this value is returned. Otherwise, an exception (of type "runtime_error") is thrown.
        ///
        /// \returns    The value of the variant (of type "Int32").
        std::int32_t GetInt32() const
        {
            this->ThrowIfTypeIsUnequalTo(Type::Int32);
            return this->int32Value;
        }

        /// If the type of the variant is "Uint32", then this value is returned. Otherwise, an exception (of type "runtime_error") is thrown.
        ///
        /// \returns    The value of the variant (of type "Uint32").
        std::uint32_t GetUInt32() const
        {
            this->ThrowIfTypeIsUnequalTo(Type::Uint32);
            return this->uint32Value;
        }

        /// If the type of the variant is "Boolean", then this value is returned. Otherwise, an exception (of type "runtime_error") is thrown.
        ///
        /// \returns    The value of the variant (of type "Boolean").
        bool GetBoolean() const
        {
            this->ThrowIfTypeIsUnequalTo(Type::Boolean);
            return this->boolValue;
        }
    private:
        Type type;  ///< The type which is represented by the variant.

        union
        {
            std::int32_t int32Value;
            std::uint32_t uint32Value;
            bool boolValue;
        };

        void ThrowIfTypeIsUnequalTo(Type typeToCheck) const
        {
            if (this->type != typeToCheck)
            {
                throw std::runtime_error("Unexpected type encountered.");
            }
        }
    };

    /// This interface is used for representing "compression parameters". It is a simple property bag.
    /// Possible values for the key are defined in the "CompressionParameter" class.
    class LIBCZI_API ICompressParameters
    {
    public:
        /// Attempts to get the property for the specified key from the property bag.
        ///
        /// \param          key         The key.
        /// \param [in,out] parameter   If non-null and the key is found, then the value is put here.
        ///
        /// \returns    True if the key is found in the property bag; false otherwise.
        virtual bool TryGetProperty(int key, CompressParameter* parameter) const = 0;

        virtual ~ICompressParameters() = default;

        /// Attempts to get the property for the specified key from the property bag. This helper is
        /// casting the enum to int, facilitating the use with the enum type.
        ///
        /// \param          key         The key.
        /// \param [in,out] parameter   If non-null and the key is found, then the value is put here.
        ///
        /// \returns    True if the key is found in the property bag; false otherwise.
        bool TryGetProperty(libCZI::CompressionParameterKey key, CompressParameter* parameter) const
        {
            return this->TryGetProperty(static_cast<typename std::underlying_type<libCZI::CompressionParameterKey>::type>(key), parameter);
        }
    };

    /// Interface representing a "block of memory". It is used to hold the result of a compression-operation.
    class LIBCZI_API IMemoryBlock
    {
    public:
        /// Gets pointer to the memory block. This memory is owned by this object instance
        /// (i. e. the memory is valid as long as this object lives). The size of this
        /// memory block is given by "GetSizeOfData".
        ///
        /// \returns    Pointer to the memory block.
        virtual void* GetPtr() = 0;

        /// Gets size of the data (for which a pointer can be retrieved by calling "GetPtr"). 
        ///
        /// \returns    The size of data in bytes.
        virtual size_t GetSizeOfData() const = 0;

        virtual ~IMemoryBlock() = default;
    };

    /// The functions found here deal with zstd-compression (the compression-part in particular).
    /// Those functions are rather low-level, and the common theme is - given a source bitmap, create a blob
    /// (containing the compressed bitmap data) which is suitable to be placed in a subblock's data.
    /// Several overloads are provided, for performance critical scenarios we provide functions which write
    /// directly into caller-provided memory, and there are versions which use caller-provided functions for
    /// internal allocations. The latter may be beneficial in high-performance scenarios where pre-allocation
    /// and buffer-reuse can be leveraged in order to avoid repeated heap-allocations.
    class LIBCZI_API ZstdCompress
    {
    public:
        /// Calculates the maximum size which might be required (for the output buffer) when calling
        /// into "CompressZStd0". The guarantee here is : if calling into "CompressZStd0" with an
        /// output buffer of the size as determined here, the call will NEVER fail (for insufficient
        /// output buffer size). Note that this upper limit may be larger than the actual needed size
        /// by a huge factor (10 times or more), and it is of the order of the input size.
        ///
        /// \param  sourceWidth     The width of the bitmap in pixels.
        /// \param  sourceHeight    The height of the bitmap in pixels.
        /// \param  sourcePixeltype The pixeltype of the bitmap.
        ///
        /// \returns    The calculated maximum compressed size.
        static size_t CalculateMaxCompressedSizeZStd0(std::uint32_t sourceWidth, std::uint32_t sourceHeight, libCZI::PixelType sourcePixeltype);

        /// Compress the specified bitmap in "zstd0"-format. This method will compress the specified source-bitmap according to the "ZEN-zstd0-scheme" to
        /// a caller supplied block of memory. If successful, the used size of the memory block is returned, and the data is suitable to be put into a subblock. 
        /// Details of the operation are:
        /// - (under certain conditions) a temporary buffer is required, the size of this temporary buffer is width*size_of_pixel*height. This method allows to pass   
        ///    in functions for allocating/freeing this temp-buffer. For performance reasons, some type of buffer-pooling or reuse can be applied here.
        /// - A pointer to an output buffer must be supplied, and its size is to be given. The required size of the output buffer is in general not known (and  
        ///    not knowable) beforehand. It is only possible to query an upper limit for the output-buffer (CalculateMaxCompressedSizeZStd1). If the output buffer
        ///    size is insufficient, this method return 'false'. 
        /// - On input, the parameter 'sizeDestination' gives the size of the output buffer; on return of the function, the value is overwritten with the actual  
        ///     used size (which is always less than the size on input).
        /// - There are only two possible outcomes of this function - either the operation completed successfully and returns true, the data is put into the output buffer and  
        ///     the argument 'sizeDestination' gives the used size in the output buffer. Or, false is returned, meaning that the output buffer size is found to be
        ///     insufficient - however, note that the required size is not given, so 'sizeDestination' is unchanged in this case (and there is no indication about 
        ///     how big an output buffer is required).
        /// - All other error conditions (like e. g. invalid arguments) result in an exception being thrown.
        /// \param          sourceWidth         Width of the source bitmap in pixels.
        /// \param          sourceHeight        Height of the source bitmap in pixels.
        /// \param          sourceStride        The stride of the source bitmap in bytes.
        /// \param          sourcePixeltype     The pixeltype of the source bitmap.
        /// \param          source              Pointer to the source bitmap.
        /// \param          allocateTempBuffer  This functor is called when it is necessary to allocate a temporary buffer. The argument specifies the
        ///                                     size in bytes for the buffer. This argument must not be null.
        ///                                     If this functor returns null, then this method exception is left with an exception(of type runtime_error).
        /// \param          freeTempBuffer      This functor is called when the temporary buffer is to be released. It is guaranteed that this free-functor is called for
        ///                                     every temp-buffer-allocation before this method returns. This argument must not be null.
        /// \param [in,out] destination         The pointer to the output buffer.
        /// \param [in,out] sizeDestination     On input, this gives the size of the destination buffer in bytes. On return of this method (and provided the return value is 'true'), this gives
        ///                                     the actual used size (which is always less or equal to the value on input).
        /// \param          parameters          Property bag containing parameters controlling the operation. This argument can be null, in which case default parameters are used.
        ///
        /// \returns    True if it succeeds, and in this case the argument 'sizeDestination' will contain the size actual used in the output buffer.
        ///             False is returned in the case that the output buffer size was insufficient.
        static bool CompressZStd0(
            std::uint32_t sourceWidth,
            std::uint32_t sourceHeight,
            std::uint32_t sourceStride,
            libCZI::PixelType sourcePixeltype,
            const void* source,
            const std::function<void* (size_t)>& allocateTempBuffer,
            const std::function<void(void*)>& freeTempBuffer,
            void* destination,
            size_t& sizeDestination,
            const ICompressParameters* parameters);

        /// Compress the specified bitmap in "zstd0"-format. This method will compress the specified source-bitmap according to the "ZEN-zstd0-scheme" to newly allocated memory, 
        /// and return a blob of memory containing the data suitable to be put into a subblock. Details of the operation are:
        /// - (under certain conditions) a temporary buffer is required, the size of this temporary buffer is width*size_of_pixel*height. This method allows to pass   
        ///    in functions for allocating/freeing this temp-buffer. For performance reasons, some type of buffer-pooling or reuse can be applied here.
        /// - A pointer to an output buffer must be supplied, and its size is to be given. The required size of the output buffer is in general not known (and  
        ///    not knowable) beforehand. It is only possible to query an upper limit for the output-buffer (CalculateMaxCompressedSizeZStd1). If the output buffer
        ///    size is insufficient, this method return 'false'. 
        /// - On input, the parameter 'sizeDestination' gives the size of the output buffer; on return of the function, the value is overwritten with the actual  
        ///     used size (which is always less than the size on input).
        /// - All error conditions (like e. g. invalid arguments) result in an exception being thrown.
        /// \param          sourceWidth         Width of the source bitmap in pixels.
        /// \param          sourceHeight        Height of the source bitmap in pixels.
        /// \param          sourceStride        The stride of the source bitmap in bytes.
        /// \param          sourcePixeltype     The pixeltype of the source bitmap.
        /// \param          source              Pointer to the source bitmap.
        /// \param          allocateTempBuffer  This functor is called when it is necessary to allocate a temporary buffer. The argument specifies the
        ///                                     size in bytes for the buffer. This argument must not be null.
        ///                                     If this functor returns null, then this method exception is left with an exception(of type runtime_error).
        /// \param          freeTempBuffer      This functor is called when the temporary buffer is to be released. It is guaranteed that this free-functor is called for
        ///                                     every temp-buffer-allocation before this method returns. This argument must not be null.
        /// \param          parameters          Property bag containing parameters controlling the operation. This argument can be null, in which case default parameters are used.
        ///
        /// \returns    A shared pointer to an object representing and owning a block of memory.
        static std::shared_ptr<IMemoryBlock> CompressZStd0Alloc(
            std::uint32_t sourceWidth,
            std::uint32_t sourceHeight,
            std::uint32_t sourceStride,
            libCZI::PixelType sourcePixeltype,
            const void* source,
            const std::function<void* (size_t)>& allocateTempBuffer,
            const std::function<void(void*)>& freeTempBuffer,
            const ICompressParameters* parameters);

        /// Compress the specified bitmap in "zstd0"-format. This method will compress the specified source-bitmap according to the "ZEN-zstd0-scheme" to
        /// a caller supplied block of memory. If successful, the used size of the memory block is returned, and the data is suitable to be put into a subblock. 
        /// Details of the operation are:
        /// - (under certain conditions) a temporary buffer is required, and this memory is then allocated internally (and freed) from the standard heap.
        /// - A pointer to an output buffer must be supplied, and its size is to be given. The required size of the output buffer is in general not known (and  
        ///    not knowable) beforehand. It is only possible to query an upper limit for the output-buffer (CalculateMaxCompressedSizeZStd1). If the output buffer
        ///    size is insufficient, this method return 'false'. 
        /// - On input, the parameter 'sizeDestination' gives the size of the output buffer; on return of the function, the value is overwritten with the actual  
        ///     used size (which is always less than the size on input).
        /// - There are only two possible outcomes of this function - either the operation completed successfully and returns true, the data is put into the output buffer and  
        ///     the argument 'sizeDestination' gives the used size in the output buffer. Or, false is returned, meaning that the output buffer size is found to be
        ///     insufficient - however, note that the required size is not given, so 'sizeDestination' is unchanged in this case (and there is no indication about 
        ///     how big an output buffer is required).
        /// - All other error conditions (like e. g. invalid arguments) result in an exception being thrown.
        /// \param          sourceWidth         Width of the source bitmap in pixels.
        /// \param          sourceHeight        Height of the source bitmap in pixels.
        /// \param          sourceStride        The stride of the source bitmap in bytes.
        /// \param          sourcePixeltype     The pixeltype of the source bitmap.
        /// \param          source              Pointer to the source bitmap.
        /// \param [in,out] destination         The pointer to the output buffer.
        /// \param [in,out] sizeDestination     On input, this gives the size of the destination buffer in bytes. On return of this method (and provided the return value is 'true'), this gives
        ///                                     the actual used size (which is always less or equal to the value on input).
        /// \param          parameters          Property bag containing parameters controlling the operation. This argument can be null, in which case default parameters are used.
        ///
        /// \returns    True if it succeeds, and in this case the argument 'sizeDestination' will contain the size actual used in the output buffer.
        ///             False is returned in the case that the output buffer size was insufficient.
        static bool CompressZStd0(
            std::uint32_t sourceWidth,
            std::uint32_t sourceHeight,
            std::uint32_t sourceStride,
            libCZI::PixelType sourcePixeltype,
            const void* source,
            void* destination,
            size_t& sizeDestination,
            const ICompressParameters* parameters);

        /// Compress the specified bitmap in "zstd0"-format. This method will compress the specified source-bitmap according to the "ZEN-zstd0-scheme" to
        /// a caller supplied block of memory. If successful, the used size of the memory block is returned, and the data is suitable to be put into a subblock. 
        /// Details of the operation are:
        /// - (under certain conditions) a temporary buffer is required, and this memory is then allocated internally (and freed) from the standard heap.
        /// - A pointer to an output buffer must be supplied, and its size is to be given. The required size of the output buffer is in general not known (and  
        ///    not knowable) beforehand. It is only possible to query an upper limit for the output-buffer (CalculateMaxCompressedSizeZStd1). If the output buffer
        ///    size is insufficient, this method return 'false'. 
        /// - On input, the parameter 'sizeDestination' gives the size of the output buffer; on return of the function, the value is overwritten with the actual  
        ///     used size (which is always less than the size on input).
        /// - There are only two possible outcomes of this function - either the operation completed successfully and returns true, the data is put into the output buffer and  
        ///     the argument 'sizeDestination' gives the used size in the output buffer. Or, false is returned, meaning that the output buffer size is found to be
        ///     insufficient - however, note that the required size is not given, so 'sizeDestination' is unchanged in this case (and there is no indication about 
        ///     how big an output buffer is required).
        /// - All other error conditions (like e. g. invalid arguments) result in an exception being thrown.
        /// \param          sourceWidth         Width of the source bitmap in pixels.
        /// \param          sourceHeight        Height of the source bitmap in pixels.
        /// \param          sourceStride        The stride of the source bitmap in bytes.
        /// \param          sourcePixeltype     The pixeltype of the source bitmap.
        /// \param          source              Pointer to the source bitmap.
        /// \param          parameters          Property bag containing parameters controlling the operation. This argument can be null, in which case default parameters are used.
        ///
        /// \returns    True if it succeeds, and in this case the argument 'sizeDestination' will contain the size actual used in the output buffer.
        ///             False is returned in the case that the output buffer size was insufficient.
        static std::shared_ptr<IMemoryBlock> CompressZStd0Alloc(
            std::uint32_t sourceWidth,
            std::uint32_t sourceHeight,
            std::uint32_t sourceStride,
            libCZI::PixelType sourcePixeltype,
            const void* source,
            const ICompressParameters* parameters);

        /// Calculates the maximum size which might be required (for the output buffer) when calling into "CompressZStd0".
        /// The guarantee here is : if calling into "CompressZStd0" with a output buffer of the size as determined here, the
        /// call will NEVER fail (for insufficient output buffer size).
        /// Note that this upper limit may be larger than the actual needed size by a huge factor (10 times or more), and it is of
        /// the order of the input size.
        ///
        /// \param  sourceWidth     The width of the bitmap in pixels. 
        /// \param  sourceHeight    The height of the bitmap in pixels.
        /// \param  sourcePixeltype The pixeltype of the bitmap.
        ///
        /// \returns    The calculated maximum compressed size.
        static size_t CalculateMaxCompressedSizeZStd1(std::uint32_t sourceWidth, std::uint32_t sourceHeight, libCZI::PixelType sourcePixeltype);

        /// Compress the specified bitmap in "zstd1"-format. This method will compress the specified source-bitmap according to the "ZEN-zstd1-scheme" to
        /// a caller supplied block of memory. If successful, the used size of the memory block is returned, and the data is suitable to be put into a subblock.
        /// Details of the operation are:
        /// - (under certain conditions) a temporary buffer is required, the size of this temporary buffer is width*size_of_pixel*height. This method allows to pass   
        ///    in functions for allocating/freeing this temp-buffer. For performance reasons, some type of buffer-pooling or reuse can be applied here.
        /// - A pointer to an output buffer must be supplied, and its size is to be given. The required size of the output buffer is in general not known (and  
        ///    not knowable) beforehand. It is only possible to query an upper limit for the output-buffer (CalculateMaxCompressedSizeZStd1). If the output buffer
        ///    size is insufficient, this method return 'false'. 
        /// - On input, the parameter 'sizeDestination' gives the size of the output buffer; on return of the function, the value is overwritten with the actual  
        ///     used size (which is always less than the size on input).
        /// - There are only two possible outcomes of this function - either the operation completed successfully and returns true, the data is put into the output buffer and  
        ///     the argument 'sizeDestination' gives the used size in the output buffer. Or, false is returned, meaning that the output buffer size is found to be
        ///     insufficient - however, note that the required size is not given, so 'sizeDestination' is unchanged in this case (and there is no indication about 
        ///     how big an output buffer is required).
        /// - All other error conditions (like e. g. invalid arguments) result in an exception being thrown.
        /// \param          sourceWidth                          Width of the source bitmap in pixels.
        /// \param          sourceHeight                         Height of the source bitmap in pixels.
        /// \param          sourceStride                         The stride of the source bitmap in bytes.
        /// \param          sourcePixeltype                      The pixeltype of the source bitmap.
        /// \param          source                               Pointer to the source bitmap.
        /// \param          allocateTempBuffer                   This functor is called when it is necessary to allocate a temporary buffer. The argument specifies the
        ///                                                      size in bytes for the buffer. This argument must not be null.
        ///                                                      If this functor returns null, then this method exception is left with an exception (of type runtime_error).
        /// \param          freeTempBuffer                       This functor is called when the temporary buffer is to be released. It is guaranteed that this free-functor is called for
        ///                                                      every temp-buffer-allocation before this method returns. This argument must not be null.
        /// \param [in,out] destination                          The pointer to the output buffer.
        /// \param [in,out] sizeDestination                      On input, this gives the size of the destination buffer in bytes. On return of this method (and provided the return value is 'true'), this gives
        ///                                                      the actual used size (which is always less or equal to the value on input).
        /// \param          parameters                           Property bag containing parameters controlling the operation. This argument can be null, in which case default parameters are used.                       
        /// \returns True if it succeeds, and in this case the argument 'sizeDestination' will contain the size actual used in the output buffer. 
        ///          False is returned in the case that the output buffer size was insufficient.
        static bool CompressZStd1(
            std::uint32_t sourceWidth,
            std::uint32_t sourceHeight,
            std::uint32_t sourceStride,
            libCZI::PixelType sourcePixeltype,
            const void* source,
            const std::function<void* (size_t)>& allocateTempBuffer,
            const std::function<void(void*)>& freeTempBuffer,
            void* destination,
            size_t& sizeDestination,
            const ICompressParameters* parameters);

        /// Compress the specified bitmap in "zstd1"-format. This method will compress the specified source-bitmap according to the "ZEN-zstd1-scheme" to newly allocated memory,
        /// and return a blob of memory containing the data suitable to be put into a subblock. Details of the operation are:
        /// Details of the operation are:
        /// - (under certain conditions) a temporary buffer is required, the size of this temporary buffer is width*size_of_pixel*height. This method allows to pass   
        ///    in functions for allocating/freeing this temp-buffer. For performance reasons, some type of buffer-pooling or reuse can be applied here.
        /// - A pointer to an output buffer must be supplied, and its size is to be given. The required size of the output buffer is in general not known (and  
        ///    not knowable) beforehand. It is only possible to query an upper limit for the output-buffer (CalculateMaxCompressedSizeZStd1). If the output buffer
        ///    size is insufficient, this method return 'false'. 
        /// - On input, the parameter 'sizeDestination' gives the size of the output buffer; on return of the function, the value is overwritten with the actual  
        ///     used size (which is always less than the size on input).
        /// - There are only two possible outcomes of this function - either the operation completed successfully and returns true, the data is put into the output buffer and  
        ///     the argument 'sizeDestination' gives the used size in the output buffer. Or, false is returned, meaning that the output buffer size is found to be
        ///     insufficient - however, note that the required size is not given, so 'sizeDestination' is unchanged in this case (and there is no indication about 
        ///     how big an output buffer is required).
        /// - All error conditions (like e. g. invalid arguments) result in an exception being thrown.
        /// \param          sourceWidth                          Width of the source bitmap in pixels.
        /// \param          sourceHeight                         Height of the source bitmap in pixels.
        /// \param          sourceStride                         The stride of the source bitmap in bytes.
        /// \param          sourcePixeltype                      The pixeltype of the source bitmap.
        /// \param          source                               Pointer to the source bitmap.
        /// \param          allocateTempBuffer
        /// This functor is called when it is necessary to allocate a temporary buffer. The argument specifies the
        /// size in bytes for the buffer. This argument must not be null.
        /// If this functor returns null, then this method exception is left with an exception (of type runtime_error).
        /// \param          freeTempBuffer                       This functor is called when the temporary buffer is to be released. It is guaranteed that this free-functor is called for
        ///                                                      every temp-buffer-allocation before this method returns. This argument must not be null.
        /// \param          parameters                           Property bag containing parameters controlling the operation. This argument can be null, in which case default parameters are used.                       
        /// \returns    A shared pointer to an object representing and owning a block of memory.
        static std::shared_ptr<IMemoryBlock> CompressZStd1Alloc(
            std::uint32_t sourceWidth,
            std::uint32_t sourceHeight,
            std::uint32_t sourceStride,
            libCZI::PixelType sourcePixeltype,
            const void* source,
            const std::function<void* (size_t)>& allocateTempBuffer,
            const std::function<void(void*)>& freeTempBuffer,
            const ICompressParameters* parameters);

        /// Compress the specified bitmap in "zstd1"-format. This method will compress the specified source-bitmap according to the "ZEN-zstd1-scheme" to
        /// a caller supplied block of memory. If successful, the used size of the memory block is returned, and the data is suitable to be put into a subblock.
        /// Details of the operation are:
        /// - (under certain conditions) a temporary buffer is required, and this memory is then allocated internally (and freed) from the standard heap.
        /// - A pointer to an output buffer must be supplied, and its size is to be given. The required size of the output buffer is in general not known (and  
        ///    not knowable) beforehand. It is only possible to query an upper limit for the output-buffer (CalculateMaxCompressedSizeZStd1). If the output buffer
        ///    size is insufficient, this method return 'false'. 
        /// - On input, the parameter 'sizeDestination' gives the size of the output buffer; on return of the function, the value is overwritten with the actual  
        ///     used size (which is always less than the size on input).
        /// - There are only two possible outcomes of this function - either the operation completed successfully and returns true, the data is put into the output buffer and  
        ///     the argument 'sizeDestination' gives the used size in the output buffer. Or, false is returned, meaning that the output buffer size is found to be
        ///     insufficient - however, note that the required size is not given, so 'sizeDestination' is unchanged in this case (and there is no indication about 
        ///     how big an output buffer is required).
        /// - All other error conditions (like e. g. invalid arguments) result in an exception being thrown.
        /// \param          sourceWidth                          Width of the source bitmap in pixels.
        /// \param          sourceHeight                         Height of the source bitmap in pixels.
        /// \param          sourceStride                         The stride of the source bitmap in bytes.
        /// \param          sourcePixeltype                      The pixeltype of the source bitmap.
        /// \param          source                               Pointer to the source bitmap.
        /// \param [in,out] destination                          The pointer to the output buffer.
        /// \param [in,out] sizeDestination                      On input, this gives the size of the destination buffer in bytes. On return of this method (and provided the return value is 'true'), this gives
        ///                                                      the actual used size (which is always less or equal to the value on input).
        /// \param          parameters                           Property bag containing parameters controlling the operation. This argument can be null, in which case default parameters are used.                       
        /// \returns        True if it succeeds, and in this case the argument 'sizeDestination' will contain the size actual used in the output buffer. 
        ///                 False is returned in the case that the output buffer size was insufficient.
        static bool CompressZStd1(
            std::uint32_t sourceWidth,
            std::uint32_t sourceHeight,
            std::uint32_t sourceStride,
            libCZI::PixelType sourcePixeltype,
            const void* source,
            void* destination,
            size_t& sizeDestination,
            const ICompressParameters* parameters);

        /// Compress the specified bitmap in "zstd1"-format. This method will compress the specified source-bitmap according to the "ZEN-zstd1-scheme" to newly allocated memory,
        /// and return a blob of memory containing the data suitable to be put into a subblock. Details of the operation are:
        /// Details of the operation are:
        /// - (under certain conditions) a temporary buffer is required, and this memory is then allocated internally (and freed) from the standard heap.
        /// - A pointer to an output buffer must be supplied, and its size is to be given. The required size of the output buffer is in general not known (and  
        ///    not knowable) beforehand. It is only possible to query an upper limit for the output-buffer (CalculateMaxCompressedSizeZStd1). If the output buffer
        ///    size is insufficient, this method return 'false'. 
        /// - On input, the parameter 'sizeDestination' gives the size of the output buffer; on return of the function, the value is overwritten with the actual  
        ///     used size (which is always less than the size on input).
        /// - There are only two possible outcomes of this function - either the operation completed successfully and returns true, the data is put into the output buffer and  
        ///     the argument 'sizeDestination' gives the used size in the output buffer. Or, false is returned, meaning that the output buffer size is found to be
        ///     insufficient - however, note that the required size is not given, so 'sizeDestination' is unchanged in this case (and there is no indication about 
        ///     how big an output buffer is required).
        /// - All error conditions (like e. g. invalid arguments) result in an exception being thrown.
        /// \param          sourceWidth                          Width of the source bitmap in pixels.
        /// \param          sourceHeight                         Height of the source bitmap in pixels.
        /// \param          sourceStride                         The stride of the source bitmap in bytes.
        /// \param          sourcePixeltype                      The pixel type of the source bitmap.
        /// \param          source                               Pointer to the source bitmap.
        /// \param          parameters                           Property bag containing parameters controlling the operation. This argument can be null, in which case default parameters are used.
        /// \returns    A shared pointer to an object representing and owning a block of memory.    
        static std::shared_ptr<IMemoryBlock> CompressZStd1Alloc(
            std::uint32_t sourceWidth,
            std::uint32_t sourceHeight,
            std::uint32_t sourceStride,
            libCZI::PixelType sourcePixeltype,
            const void* source,
            const ICompressParameters* parameters);
    };

    /// The functions found here deal with JXR-compression - as implemented by jxrlib (the JPEG XR 
    /// Image Codec reference implementation library released by Microsoft under BSD-2-Clause License).
    /// Those functions are rather low-level, and the common theme is - given a source bitmap, create a blob
    /// (containing the compressed bitmap data) which is suitable to be placed in a subblock's data.
    class LIBCZI_API JxrLibCompress
    {
    public:
        /// Compress the specified bitmap in "JXR"-format. This method will compress the 
        /// specified source-bitmap according to the "JXR-scheme" to a newly allocated block of memory.
        /// Parameters controlling the operation are provided in an optional property bag.
        ///
        /// \param  pixel_type  The pixel type of the source bitmap.
        /// \param  width       Width of the source bitmap in pixels.
        /// \param  height      Height of the source bitmap in pixels.
        /// \param  stride      The stride of the source bitmap in bytes.
        /// \param  ptrData     Pointer to the source bitmap.
        /// \param  parameters  Property bag containing parameters controlling the operation. This argument can be null, in which case default parameters are used.
        ///
        /// \returns    A shared pointer to an object representing and owning a block of memory, containing the JXR-compressed bitmap.
        static std::shared_ptr<IMemoryBlock> Compress(
            libCZI::PixelType pixel_type, 
            std::uint32_t width, 
            std::uint32_t height, 
            std::uint32_t stride, 
            const void* ptrData,
            const ICompressParameters* parameters);
    };

    class LIBCZI_API ChunkedCompress
    {
    public:
        /// Compress the specified bitmap in "chunked-compression"-format. This method will compress the specified source-bitmap according to the "ZEN-chunked-compression-scheme" to a newly allocated block of memory, and return a blob of memory containing the data suitable to be put into a subblock. Details of the operation are:
        /// - The chunked-compression scheme is described in more detail in the class ChunkedCompressionHeaderHelper.
        /// - A pointer to an output buffer must be supplied, and its size is to be given. The required size of the output buffer is in general not known (and  
        ///    not knowable) beforehand. It is only possible to query an upper limit for the output-buffer (CalculateMaxCompressedSizeChunked). If the output buffer
        ///    size is insufficient, this method return 'false'. 
        /// - On input, the parameter 'sizeDestination' gives the size of the output buffer; on return of the function, the value is overwritten with the actual  
        ///     used size (which is always less than the size on input).
        /// - All error conditions (like e.g. invalid arguments) result in an exception being thrown.
        /// \param          sourceWidth         Width of the source bitmap in pixels.
        /// \param          sourceHeight        Height of the source bitmap in pixels.
        /// \param          sourceStride        The stride of the source bitmap in bytes.
        /// \param          sourcePixeltype     The pixeltype of the source bitmap.
        /// \param          source              Pointer to the source bitmap.
        /// \param [in,out] destination         The pointer to the output buffer.
        /// \param [in,out] sizeDestination     On input, this gives the size of the destination buffer in bytes. On return of this method (and provided the return value is 'true'), this gives
        ///                                     the actual used size (which is always less or equal to the value on input).
        /// \param          allocateTempBuffer  This functor is called when it is necessary to allocate a temporary buffer. The argument specifies the
        ///                                     size in bytes for the buffer. This argument must not be null.
        ///                                     If this functor returns null, then this method exception is left with an exception (of type runtime_error).
        /// \param          freeTempBuffer      This functor is called when the temporary buffer is to be released. It is guaranteed that this free-functor is called for
        ///                                     every temp-buffer-allocation before this method returns. This argument must not be null.
        /// \param          parameters          Property bag containing parameters controlling the operation. This argument can be null, in which case default parameters are used.
        ///
        /// \returns    True if it succeeds, and in this case the argument 'sizeDestination' will contain the size actual used in the output buffer.
        ///             False is returned in the case that the output buffer size was insufficient.
        static bool Compress(
            std::uint32_t sourceWidth,
            std::uint32_t sourceHeight,
            std::uint32_t sourceStride,
            libCZI::PixelType sourcePixeltype,
            const void* source,
            void* destination,
            size_t& sizeDestination,
            const std::function<void* (size_t)>& allocateTempBuffer,
            const std::function<void(void*)>& freeTempBuffer,
            const ICompressParameters* parameters);

        static bool Compress(
            std::uint32_t sourceWidth,
            std::uint32_t sourceHeight,
            std::uint32_t sourceStride,
            libCZI::PixelType sourcePixeltype,
            const void* source,
            void* destination,
            size_t& sizeDestination,
            const ICompressParameters* parameters);
    };

    /// Simplistic implementation of the compression-parameters property bag. Note that for high-performance scenarios
    /// it might be a good idea to re-use instances of this, or have a custom implementation without heap-allocation
    /// penalty.
    class LIBCZI_API CompressParametersOnMap : public ICompressParameters
    {
    public:
        std::map<int, CompressParameter> map;   ///< The key-value map containing "compression parameters".

        /// Attempts to get the property for the specified key from the property bag.
        ///
        /// \param          key         The key.
        /// \param [in,out] parameter   If non-null and the key is found, then the value is put here.
        ///
        /// \returns    True if the key is found in the property bag; false otherwise.
        bool TryGetProperty(int key, CompressParameter* parameter) const override
        {
            const auto it = this->map.find(key);
            if (it != this->map.cend())
            {
                if (parameter != nullptr)
                {
                    *parameter = it->second;
                }

                return true;
            }

            return false;
        }
    };

    /// Here we gather utilities for working with chunked-compression headers. The concept of the chunked-compression scheme
    /// is to have a header which describes the structure of the compressed data in terms of "chunks", and then the compressed
    /// data is organized in a sequence of chunks, where each chunk contains compressed data for a part of the uncompressed data.
    /// The header contains information about the size of each chunk, the compression method used for each chunk, and other parameters. 
    /// This allows to have more flexibility in how the data is compressed and stored.
    /// The binary layout is as follows:
    ///   ┌─────────────────────┐
    ///   │ HEADER              │
    ///   ├─────────────────────┤
    ///   │ CHUNK #0            │
    ///   ├─────────────────────┤
    ///   │ CHUNK #1            │
    ///   ├─────────────────────┤
    ///   │ ...                 │
    ///   ├─────────────────────┤
    ///   │ CHUNK #n            │
    ///   └─────────────────────┘
    /// The individual chunks contain the compressed data, and each chunk can be decompressed independently. The header contains 
    /// the necessary information to interpret the chunks correctly.
    /// The header itself is organized in "header chunks", where each header chunk has a chunk identifier, a size, and a payload.
    class LIBCZI_API ChunkedCompressionHeaderHelper
    {
    public:
        /// Values that represent codecs.
        enum class Codec : std::uint8_t
        {
            Invalid = 0xff, ///< Invalid codec, used to indicate an error condition.
            ZStd = 0,       ///< Zstd compression
            Lz4 = 1,        ///< Lz4 compression
        };

        /// Values that represent header chunk Identifiers.
        enum class HeaderChunkId : std::uint16_t
        {
            EndOfHeader = 0,	    ///< This header chunk indicates the end of the header. This must be the last chunk in the header, and it has no payload.
            ChunkSizes = 1,         ///< This header chunk contains the sizes of the compressed chunks.
            CompressionMethod = 2,  ///< This header chunk contains the compression method (codec) used for the chunks.
            DecompressedSizes = 3,  ///< This header chunk contains the sizes of the uncompressed data for the chunks.
            Preprocessing = 4,      ///< This header chunk contains information about preprocessing applied to the data before compression (like hi-lo byte packing).
        };

        struct CompressionHeaderChunk
        {
            std::uint16_t chunkId;
            std::uint32_t chunkSize;
            const void* chunkPayload;
            size_t chunkPayloadSize;
        };

        /// This struct defines the parameters to be stored in the chunked-compression headers for use when
        /// creating a compression header. Note that only the case of "all uncompressed chunk sizes but the last
        /// one" is currently supported.
        struct HeaderInfoForCreation
        {
            Codec codec;

            /// This flag indicates whether the "hi-lo byte packing" preprocessing is applied to the data before compression.
            /// 0 means "no hi-lo byte packing", 1 means "hi-lo byte packing applied", everything else means: unspecified.
            std::uint8_t hiLoBytePackingApplied;

            std::vector<std::uint32_t> chunkSizes;

            /// This describes the uncompressed sizes of the data for each chunk. 
            /// The first value is the uncompressed size of all data chunks but the last, and the second value is the uncompressed size of the last chunk. 
            /// (Note that we here are assuming that all the sizes but the last are the same!).
            /// If the last chunk has the same uncompressed size as the other chunks or if there is only one chunk, then the second value is zero 
            /// (and the first value gives the uncompressed size of all chunks).
            std::tuple<std::uint32_t, std::uint32_t> uncompressedSizes;
        };

        struct HeaderInfo
        {
            struct ChunkInfo
            {
                std::uint32_t compressedSize;
                std::uint32_t uncompressedSize;
            };

            Codec codec;
            bool hiLoBytePackingApplied;
            std::vector<ChunkInfo> chunks;
        };

        static bool WalkCompressionHeader(const void* data, size_t sizeData, const std::function<bool(const CompressionHeaderChunk&)>& callback, size_t* bytes_consumed);

        /// Parse the chunked-compression header, and return the size of the header (in bytes). If the given data does not contain a valid chunked-compression header,
        /// then an exception is thrown. Note that only the structure of the header is parsed here, not the semantic of the header content.
        ///
        /// \param 	data		Pointer to the data to be parsed.
        /// \param 	sizeData	The size of the data.
        ///
        /// \returns	The size of the header in units of bytes.
        static size_t GetCompressionHeaderSize(const void* data, size_t sizeData);

        static std::tuple<size_t, HeaderInfo> ParseCompressionHeader(const void* data, size_t sizeData);

        /// Creates a chunked-compression-header for the given header information. The created header is written to the memory pointed to by 
        /// 'destination', and the size of the created header is returned. The required size of the destination buffer is in general not known (and not knowable) beforehand,
        /// but it is guaranteed that the required size is less or equal to the value returned by 'DetermineMaxSizeForCompressionHeader' for the same header information.
        /// If the specified destination buffer size is insufficient, then this method throws an exception (of type runtime_error). All other error conditions 
        /// (like e.g. invalid arguments) also result in an exception being thrown.
        ///
        /// \param [in,out]	destination	   	The pointer to the memory where the created header is written to. This argument must not be null.
        /// \param 		   	sizeDestination	The size of the memory block pointed to by 'destination' in bytes.
        /// \param 		   	headerInfo	   	Information describing the header.
        ///
        /// \returns	The size of the compression header written to the memory pointed to by 'destination' in bytes.
        static size_t CreateCompressionHeader(void* destination, size_t sizeDestination, const HeaderInfoForCreation& headerInfo);

        /// Information used to determine the maximum size of a compression header.
        /// All fields must be set to valid values. The number_of_chunks field must be greater than 1.
        struct HeaderInfoForMaxSizeDetermination
        {
            Codec codec;
            std::uint8_t hiLoBytePackingApplied;

            /// The number of chunks. Must be greater than 1.
            std::uint32_t number_of_chunks;
        };

        /// This function determines the maximum size of the compression header for the given header information. The actual size of the
        ///  compression header for a given set of header information may be smaller than this maximum size, but it will never be larger. 
        ///
        /// \param 	headerInfo	Information describing the header.
        ///
        /// \returns	The max number of bytes required for constructing a CompressionHeader with 'CreateCompressionHeader'.
        static size_t DetermineMaxSizeForCompressionHeader(const HeaderInfoForMaxSizeDetermination& headerInfo);

        /// Convenience overload of the above function, which takes 'HeaderInfoForCreation' as argument. The required
        /// information for determining the max size is determined from the given 'HeaderInfoForCreation' argument, and
        /// in turn the function 'DetermineMaxSizeForCompressionHeader(const HeaderInfoForMaxSizeDetermination& headerInfo)' 
        /// is called to determine the max size.
        ///
        /// \param 	headerInfo	Information describing the header.
        ///
        /// \returns	The max number of bytes required for constructing a CompressionHeader with 'CreateCompressionHeader'.
        static size_t DetermineMaxSizeForCompressionHeader(const HeaderInfoForCreation& headerInfo);
    };
}
