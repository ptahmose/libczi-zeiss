#include "../libCZI/libCZI.h"
#include <iostream>
#include <vector>

using namespace std;
using namespace libCZI;

/**
 * Normalizes the pyramids within a scene to have the same 'height' i.e. the same minimum zoom and the same number of zoom levels.
 *
 * @param reader_writer A shared pointer to an ICziReaderWriter object.
 *
 * Note: This function may discard image data at lower zoom levels while it is only the lower resolution image data which is discarded
 * and this image data can be recovered from the more detailed data at higher zooms.
 */
static void NormaliseScenePyramids(const std::shared_ptr<ICziReaderWriter>& reader_writer)
{
    std::cout << "Performing pyramid normalisation" << std::endl;

    // Do an initial pass of all the blocks, figuring out what is the ‘height’ of each pyramid (minimumZoomPerScene)
    std::map<int, double> minimumZoomPerScene;
    reader_writer->EnumerateSubBlocks(
        [&](int index, const SubBlockInfo& info) -> bool
    {
        libCZI::CDimCoordinate coord = info.coordinate;
        int sValue;
        if (coord.TryGetPosition(DimensionIndex::S, &sValue))
        {
            double zoom = info.GetZoom();
            // Check if sValue is already in minimumZoomPerScene and if zoom is smaller
            auto it = minimumZoomPerScene.find(sValue);
            if (it == minimumZoomPerScene.end() || it->second > zoom) // if not found or found with a larger zoom
            {
                minimumZoomPerScene[sValue] = zoom; // Update with the new smaller zoom
            }
        }
        return true;
    });

    // Figure out which pyramid is the shortest (minimumCommonZoom)
    double minimumCommonZoom = 0;
    for(const auto& pair : minimumZoomPerScene) {
        std::cout << "Scene: " << pair.first << ", Minimum Zoom: " << pair.second << std::endl;
        if(pair.second > minimumCommonZoom){
            minimumCommonZoom = pair.second;
        }
    }
    std::cout << "Minimum Common Zoom : " << minimumCommonZoom << std::endl;

    // Do another pass of the blocks and mark for removal, any blocks which are zoomed out more than this value.
    std::vector<int> indexesToRemove;
    reader_writer->EnumerateSubBlocks(
        [&](int index, const SubBlockInfo& info) ->bool
    {
        double zoom = info.GetZoom();
        if(zoom<minimumCommonZoom){ 
            std::cout << "Marking subblock for removal Index " << index << " Zoom " << zoom << ": " << libCZI::Utils::DimCoordinateToString(&info.coordinate) << " Rect=" << info.logicalRect << " PhysicalSize=" << info.physicalSize << std::endl;
            indexesToRemove.push_back(index); 
        }
        return true;
    });

    // Finally, do the actual sub block removal.  Note that this is done out of the EnumerateSubBlocks loop to avoid a segmentation fault
    if(!indexesToRemove.empty()){
        std::cout << "Removing sub blocks" << std::endl;
        for(int index : indexesToRemove) {
            reader_writer->RemoveSubBlock(index);
        }
    }
}

/**
 * Update the S dimension and m index of a block.
 *
 * @param reader_writer A shared pointer to an ICziReaderWriter object.
 * @param index The index of the block as seen by the reader.
 * @param info Sub Block info for the block.
 */
static void MergeSIndex(const std::shared_ptr<ICziReaderWriter>& reader_writer, int index, const SubBlockInfo& info)
{
    AddSubBlockInfo add_sub_block_info;
    add_sub_block_info.Clear();
    add_sub_block_info.coordinate = info.coordinate;
    add_sub_block_info.coordinate.Set(DimensionIndex::S,0); //Set all Scenes to 0, effectively 'merging' the scenes into one

    add_sub_block_info.mIndexValid = true;
    add_sub_block_info.mIndex = index;  //reset mIndex using the index of the block enumeration
    add_sub_block_info.x = info.logicalRect.x;
    add_sub_block_info.y = info.logicalRect.y;
    add_sub_block_info.logicalWidth = info.logicalRect.w;
    add_sub_block_info.logicalHeight = info.logicalRect.h;
    add_sub_block_info.physicalWidth = info.physicalSize.w;
    add_sub_block_info.physicalHeight = info.physicalSize.h;
    add_sub_block_info.PixelType = info.pixelType;
    add_sub_block_info.pyramid_type = info.pyramidType;
    add_sub_block_info.compressionModeRaw = info.compressionModeRaw;

    auto subBlock = reader_writer->ReadSubBlock(index);
    size_t sub_block_data_size;
    auto sub_block_data = subBlock->GetRawData(ISubBlock::MemBlkType::Data, &sub_block_data_size);
    size_t sub_block_metadata_size;
    auto sub_block_metadata = subBlock->GetRawData(ISubBlock::MemBlkType::Metadata, &sub_block_metadata_size);
    size_t sub_block_attachment_size;
    auto sub_block_attachment = subBlock->GetRawData(ISubBlock::MemBlkType::Attachment, &sub_block_attachment_size);

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

/**
 * Update document metadata to indicate the new size of the S dimension (1) and add a comment indicating the file has been merged.
 *
 * @param reader_writer A shared pointer to an ICziReaderWriter object.
 */
static void UpdateMetadata(const std::shared_ptr<ICziReaderWriter>& reader_writer){
    std::cout << "Updating document metadata" << std::endl;
    const auto metadata_segment = reader_writer->ReadMetadataSegment();
    const auto metadata = metadata_segment->CreateMetaFromMetadataSegment();
    const auto metadata_builder = CreateMetadataBuilderFromXml(metadata->GetXml());

    const auto comment_node = metadata_builder->GetRootNode()->GetOrCreateChildNode("Metadata/Information/Document/Comment");
    comment_node->SetValue("The scenes in this file have been merged post-instrument");

    const auto size_s_node = metadata_builder->GetRootNode()->GetOrCreateChildNode("Metadata/Information/Image/SizeS");
    size_s_node->SetValue("1");

    WriteMetadataInfo metadata_info;
    metadata_info.Clear();
    const string source_metadata_xml = metadata_builder->GetXml();
    metadata_info.szMetadata = source_metadata_xml.c_str();
    metadata_info.szMetadataSize = source_metadata_xml.size();
    reader_writer->SyncWriteMetadata(metadata_info);
}


int main()
{
    std::cout << "Opening input/output stream" << std::endl;
    const auto io_stream = libCZI::CreateInputOutputStreamForFile(LR"(D:\Data\CZI\libczi#94\test.czi)");

    // create the reader-writer-object
    auto reader_writer = libCZI::CreateCZIReaderWriter();

    // open the CZI-file
    reader_writer->Create(io_stream);

    // normalise scene pyramids so they all have the same number of zoom levels
    NormaliseScenePyramids(reader_writer);

    // merge the scenes by setting the S index of all sub blocks to 0
    std::cout << "Merging scenes" << std::endl;
    reader_writer->EnumerateSubBlocks(
        [&](int index, const SubBlockInfo& info) ->bool
    {
        MergeSIndex(reader_writer, index, info);
        return true;
    });

    // update the document metadata so FIJI and CZICheck are happy
    UpdateMetadata(reader_writer);

    reader_writer->Close();

    return 0;
}
