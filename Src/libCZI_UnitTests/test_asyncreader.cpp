#include "include_gtest.h"
#include "inc_libCZI.h"
#include "MemOutputStream.h"
#include "MemAsyncInputStream.h"
#include "utils.h"

using namespace libCZI;
using namespace std;

namespace
{
    tuple<shared_ptr<void>, size_t> CreateCziDocumentOneSubblock4x4Gray8()
    {
        auto writer = CreateCZIWriter();
        auto outStream = make_shared<CMemOutputStream>(0);
        auto spWriterInfo = make_shared<CCziWriterInfo >(GUID{ 0x1234567,0x89ab,0xcdef,{ 1,2,3,4,5,6,7,8 } });
        writer->Create(outStream, spWriterInfo);
        auto bitmap = CreateTestBitmap(PixelType::Gray8, 4, 4);
        ScopedBitmapLockerSP lockBm{ bitmap };
        AddSubBlockInfoStridedBitmap addSbBlkInfo;
        addSbBlkInfo.Clear();
        addSbBlkInfo.coordinate = CDimCoordinate::Parse("C0");
        addSbBlkInfo.mIndexValid = true;
        addSbBlkInfo.mIndex = 0;
        addSbBlkInfo.x = 0;
        addSbBlkInfo.y = 0;
        addSbBlkInfo.logicalWidth = bitmap->GetWidth();
        addSbBlkInfo.logicalHeight = bitmap->GetHeight();
        addSbBlkInfo.physicalWidth = bitmap->GetWidth();
        addSbBlkInfo.physicalHeight = bitmap->GetHeight();
        addSbBlkInfo.PixelType = bitmap->GetPixelType();
        addSbBlkInfo.ptrBitmap = lockBm.ptrDataRoi;
        addSbBlkInfo.strideBitmap = lockBm.stride;
        writer->SyncAddSubBlock(addSbBlkInfo);
        writer->Close();

        size_t size_data;
        const auto data = outStream->GetCopy(&size_data);
        return make_tuple(data, size_data);
    }
}

TEST(TestAsyncReader, ReadCziDocumentOneSubblock4x4Gray8)
{
    auto test_czi = CreateCziDocumentOneSubblock4x4Gray8();
    auto inStream = make_shared<MemAsyncInputStream>(get<0>(test_czi).get(), get<1>(test_czi));
    auto reader = CreateCZIReaderAsync();
    reader->Open(inStream);

}
