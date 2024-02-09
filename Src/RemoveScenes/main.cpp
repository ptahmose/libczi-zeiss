#include "../libCZI/libCZI.h"
#include <iostream>
#include <vector>

using namespace std;
using namespace libCZI;

static void RemoveSIndex(const std::shared_ptr<ICziReaderWriter>& reader_writer, int index, const SubBlockInfo& info)
{
    AddSubBlockInfo add_sub_block_info;
    add_sub_block_info.Clear();
    add_sub_block_info.coordinate = info.coordinate;
    add_sub_block_info.coordinate.Clear(DimensionIndex::S);

    // we also set the m-index to invalid - we would have to re-create an m-index scheme when removing the S-index,
    //  because "m-index is scene-scoped". So, in order to have a well-formed CZI, we'd need to re-create the m-index.
    //  We don't do this here (yet), and I reckon it is better to have no m-index than an invalid one.
    add_sub_block_info.mIndexValid = false;
    add_sub_block_info.mIndex = numeric_limits<int>::max();
    add_sub_block_info.x = info.logicalRect.x;
    add_sub_block_info.y = info.logicalRect.y;
    add_sub_block_info.logicalWidth = info.logicalRect.w;
    add_sub_block_info.logicalHeight = info.logicalRect.h;
    add_sub_block_info.physicalWidth = info.physicalSize.w;
    add_sub_block_info.physicalHeight = info.physicalSize.h;
    add_sub_block_info.PixelType = info.pixelType;
    add_sub_block_info.pyramid_type = info.pyramidType;

    // note: there is a bug in libCZI here (info.compressionModeRaw is not valid here, we work around
    //        this by using the compression mode of the sub-block which we have to read in any case)
    add_sub_block_info.compressionModeRaw = info.compressionModeRaw;

    auto subBlock = reader_writer->ReadSubBlock(index);
    size_t sub_block_data_size;
    auto sub_block_data = subBlock->GetRawData(ISubBlock::MemBlkType::Data, &sub_block_data_size);
    size_t sub_block_metadata_size;
    auto sub_block_metadata = subBlock->GetRawData(ISubBlock::MemBlkType::Metadata, &sub_block_metadata_size);
    size_t sub_block_attachment_size;
    auto sub_block_attachment = subBlock->GetRawData(ISubBlock::MemBlkType::Attachment, &sub_block_attachment_size);

    // workaround for above bug in libCZI
    add_sub_block_info.compressionModeRaw = subBlock->GetSubBlockInfo().compressionModeRaw;

    add_sub_block_info.sizeData = sub_block_data_size;
    add_sub_block_info.getData =
        [&](int callCnt, size_t offset, const void*& ptr, size_t& size) -> bool
        {
            // TODO: validate that offset is is less than sub_block_data_size (and - offset should never be non-zero)
            ptr = static_cast<const uint8_t*>(sub_block_data.get()) + offset;
            size = sub_block_data_size - offset;
            return true;
        };

    add_sub_block_info.sizeMetadata = sub_block_metadata_size;
    if (add_sub_block_info.sizeMetadata > 0)
    {
        add_sub_block_info.getMetaData =
            [&](int callCnt, size_t offset, const void*& ptr, size_t& size) -> bool
            {
                // TODO: validate offset
                ptr = static_cast<const uint8_t*>(sub_block_metadata.get()) + offset;
                size = sub_block_metadata_size - offset;
                return true;
            };
    }

    add_sub_block_info.sizeAttachment = sub_block_attachment_size;
    if (add_sub_block_info.sizeAttachment > 0)
    {
        add_sub_block_info.getAttachment =
            [&](int callCnt, size_t offset, const void*& ptr, size_t& size) -> bool
            {
                // TODO: validate offset
                ptr = static_cast<const uint8_t*>(sub_block_attachment.get()) + offset;
                size = sub_block_attachment_size - offset;
                return true;
            };
    }

    reader_writer->ReplaceSubBlock(index, add_sub_block_info);
}

int main()
{
    std::cout << "Opening input/output stream" << std::endl;
    const auto io_stream = libCZI::CreateInputOutputStreamForFile(LR"(D:\Data\CZI\libczi#94\test.czi)");

    // create the reader-writer-object
    auto reader_writer = libCZI::CreateCZIReaderWriter();

    // open the CZI-file
    reader_writer->Create(io_stream);

    reader_writer->EnumerateSubBlocks(
        [&](int index, const SubBlockInfo& info) ->bool
    {
        RemoveSIndex(reader_writer, index, info);
        return true;
    });

    reader_writer->Close();

    return 0;
}
