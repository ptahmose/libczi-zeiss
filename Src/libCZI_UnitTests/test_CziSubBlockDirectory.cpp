// SPDX-FileCopyrightText: 2017-2022 Carl Zeiss Microscopy GmbH
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "pch.h"
#include "testImage.h"
#include "inc_libCZI.h"

using namespace libCZI;

struct SubBlockEntryData
{
    const char* coordinate;
    int mIndex;
    int x;
    int y;
    int width;
    int height;
    int storedWidth;
    int storedHeight;
};

static CCziSubBlockDirectory::SubBlkEntry SubBlkEntryFromSubBlockEntryData(const SubBlockEntryData* ptrData)
{
    CCziSubBlockDirectory::SubBlkEntry entry;
    entry.coordinate = CDimCoordinate::Parse(ptrData->coordinate);
    entry.mIndex = ptrData->mIndex;
    entry.x = ptrData->x;
    entry.y = ptrData->y;
    entry.width = ptrData->width;
    entry.height = ptrData->height;
    entry.storedWidth = ptrData->storedWidth;
    entry.storedHeight = ptrData->storedHeight;
    entry.PixelType = (int)PixelType::Gray16;
    entry.FilePosition = 42;
    entry.Compression = 1;
    return entry;
}


TEST(CziSubBlockDirectory, CziSubBlockDirectory1)
{
    static const SubBlockEntryData data[] =
    {
    { "C0S0",0,-5657,-6196,2048,2048,2048,2048 },
    { "C0S0",1,-3814,-6196,2048,2048,2048,2048 },
    { "C0S0",2,-1970,-6196,2048,2048,2048,2048 },
    { "C0S0",3,-127,-6196,2048,2048,2048,2048 },
    { "C0S0",4,1716,-6196,2048,2048,2048,2048 },
    { "C0S0",5,3559,-4353,2048,2048,2048,2048 },
    { "C0S0",-2147483647 - 1,4788,-4354,820,206,410,103 },
    { "C0S0",6,1716,-4353,2048,2048,2048,2048 },
    { "C0S0",-2147483647 - 1,2740,-6196,2048,2048,1024,1024 },
    { "C0S0",7,-127,-4353,2048,2048,2048,2048 },
    { "C0S0",-2147483647 - 1,692,-6196,2048,2048,1024,1024 },
    { "C0S0",8,-1970,-4353,2048,2048,2048,2048 },
    { "C0S0",-2147483647 - 1,-1356,-6196,2048,2048,1024,1024 },
    { "C0S0",9,-3814,-4353,2048,2048,2048,2048 },
    { "C0S0",-2147483647 - 1,-3404,-4148,2048,1844,1024,922 },
    { "C0S0",-2147483647 - 1,-3404,-6196,2048,2048,1024,1024 },
    { "C0S0",10,-5657,-4353,2048,2048,2048,2048 },
    { "C0S0",-2147483647 - 1,-5452,-6196,2048,2048,1024,1024 },
    { "C0S0",11,-7500,-4353,2048,2048,2048,2048 },
    { "C0S0",-2147483647 - 1,-7500,-6196,2048,2048,1024,1024 },
    { "C0S0",12,-7500,-2509,2048,2048,2048,2048 },
    { "C0S0",13,-5657,-2509,2048,2048,2048,2048 },
    { "C0S0",-2147483647 - 1,-7500,-4148,2048,2048,1024,1024 },
    { "C0S0",-2147483647 - 1,-5452,-4148,2048,2048,1024,1024 },
    { "C0S0",-2147483647 - 1,-7500,-6196,4096,4096,1024,1024 },
    { "C0S0",14,-127,-2509,2048,2048,2048,2048 },
    { "C0S0",-2147483647 - 1,-128,-2100,820,1640,410,820 },
    { "C0S0",-2147483647 - 1,-128,-2100,820,1640,205,410 },
    { "C0S0",-2147483647 - 1,-1356,-4148,2048,2048,1024,1024 },
    { "C0S0",-2147483647 - 1,-3404,-6196,4096,4096,1024,1024 },
    { "C0S0",15,1716,-2509,2048,2048,2048,2048 },
    { "C0S0",-2147483647 - 1,692,-4148,2048,2048,1024,1024 },
    { "C0S0",16,3559,-2509,2048,2048,2048,2048 },
    { "C0S0",-2147483647 - 1,4788,-4148,820,2048,410,1024 },
    { "C0S0",-2147483647 - 1,4788,-4356,820,2256,205,564 },
    { "C0S0",-2147483647 - 1,2740,-4148,2048,2048,1024,1024 },
    { "C0S0",-2147483647 - 1,692,-6196,4096,4096,1024,1024 },
    { "C0S0",17,3559,-666,2048,2048,2048,2048 },
    { "C0S0",-2147483647 - 1,4788,-2100,820,2048,410,1024 },
    { "C0S0",18,1716,-666,2048,2048,2048,2048 },
    { "C0S0",-2147483647 - 1,2740,-2100,2048,2048,1024,1024 },
    { "C0S0",-2147483647 - 1,692,-2100,2048,2048,1024,1024 },
    { "C0S0",19,-5657,-666,2048,2048,2048,2048 },
    { "C0S0",20,-7500,-666,2048,2048,2048,2048 },
    { "C0S0",-2147483647 - 1,-5452,-52,1844,1434,922,717 },
    { "C0S0",-2147483647 - 1,-7500,-52,2048,1434,1024,717 },
    { "C0S0",-2147483647 - 1,-5452,-2100,1844,2048,922,1024 },
    { "C0S0",-2147483647 - 1,-7500,-2100,2048,2048,1024,1024 },
    { "C0S0",-2147483647 - 1,-7500,-2100,3892,3484,973,871 },
    { "C0S0",-2147483647 - 1,-7500,-6196,8192,7584,1024,948 },
    { "C0S0",21,1716,1177,2048,2048,2048,2048 },
    { "C0S0",-2147483647 - 1,1716,-52,1024,2048,512,1024 },
    { "C0S0",22,3559,1177,2048,2048,2048,2048 },
    { "C0S0",-2147483647 - 1,4788,-52,820,2048,410,1024 },
    { "C0S0",-2147483647 - 1,4788,-2100,820,4096,205,1024 },
    { "C0S0",-2147483647 - 1,2740,-52,2048,2048,1024,1024 },
    { "C0S0",-2147483647 - 1,692,-2100,4096,4096,1024,1024 },
    { "C0S0",-2147483647 - 1,692,-6196,4920,8192,615,1024 },
    { "C0S0",23,3559,3020,2048,2048,2048,2048 },
    { "C0S0",-2147483647 - 1,4788,1996,820,2048,410,1024 },
    { "C0S0",24,1716,3020,2048,2048,2048,2048 },
    { "C0S0",-2147483647 - 1,1716,1996,1024,2048,512,1024 },
    { "C0S0",-2147483647 - 1,2740,1996,2048,2048,1024,1024 },
    { "C0S0",25,-1970,4863,2048,2048,2048,2048 },
    { "C0S0",-2147483647 - 1,-1970,4862,614,1230,307,615 },
    { "C0S0",26,-127,4863,2048,2048,2048,2048 },
    { "C0S0",-2147483647 - 1,-1356,4862,2048,1230,1024,615 },
    { "C0S0",-2147483647 - 1,-1972,4860,2664,1232,666,308 },
    { "C0S0",27,1716,4863,2048,2048,2048,2048 },
    { "C0S0",-2147483647 - 1,692,4044,2048,2048,1024,1024 },
    { "C0S0",28,3559,4863,2048,2048,2048,2048 },
    { "C0S0",-2147483647 - 1,4788,6092,820,820,410,410 },
    { "C0S0",-2147483647 - 1,4788,6092,820,820,205,205 },
    { "C0S0",-2147483647 - 1,4788,4044,820,2048,410,1024 },
    { "C0S0",-2147483647 - 1,4788,1996,820,4096,205,1024 },
    { "C0S0",-2147483647 - 1,2740,4044,2048,2048,1024,1024 },
    { "C0S0",-2147483647 - 1,692,1996,4096,4096,1024,1024 },
    { "C0S0",29,1716,6707,2048,2048,2048,2048 },
    { "C0S0",-2147483647 - 1,2740,8140,1024,616,512,308 },
    { "C0S0",-2147483647 - 1,2740,6092,2048,2048,1024,1024 },
    { "C0S0",30,-127,6707,2048,2048,2048,2048 },
    { "C0S0",-2147483647 - 1,692,8140,2048,616,1024,308 },
    { "C0S0",-2147483647 - 1,692,6092,2048,2048,1024,1024 },
    { "C0S0",-2147483647 - 1,692,6092,4096,2664,1024,666 },
    { "C0S0",-2147483647 - 1,692,1996,4920,6760,615,845 },
    { "C0S0",31,-1970,6707,2048,2048,2048,2048 },
    { "C0S0",-2147483647 - 1,-1356,8140,2048,616,1024,308 },
    { "C0S0",-2147483647 - 1,-1356,6092,2048,2048,1024,1024 },
    { "C0S0",32,-3814,6707,2048,2048,2048,2048 },
    { "C0S0",33,-5657,6707,2048,2048,2048,2048 },
    { "C0S0",-2147483647 - 1,-3404,8140,2048,616,1024,308 },
    { "C0S0",-2147483647 - 1,-3404,6092,2048,2048,1024,1024 },
    { "C0S0",-2147483647 - 1,-3404,6092,4096,2664,1024,666 },
    { "C0S0",34,-7500,6707,2048,2048,2048,2048 },
    { "C0S0",-2147483647 - 1,-5452,8140,2048,616,1024,308 },
    { "C0S0",-2147483647 - 1,-7500,8140,2048,616,1024,308 },
    { "C0S0",-2147483647 - 1,-7500,6706,2048,1434,1024,717 },
    { "C0S0",-2147483647 - 1,-5452,6706,2048,1434,1024,717 },
    { "C0S0",-2147483647 - 1,-7500,6704,4096,2052,1024,513 },
    { "C0S0",-2147483647 - 1,-7500,4860,8192,3896,1024,487 },
    { "C0S0",-2147483647 - 1,-7500,-6196,13120,14960,820,935 },
    { "C0S1",0,-14075,129,2048,2048,2048,2048 },
    { "C0S1",1,-12231,129,2048,2048,2048,2048 },
    { "C0S1",2,-3015,129,2048,2048,2048,2048 },
    { "C0S1",3,-1172,129,2048,2048,2048,2048 },
    { "C0S1",4,-1172,1972,2048,2048,2048,2048 },
    { "C0S1",-2147483647 - 1,261,2177,616,1844,308,922 },
    { "C0S1",-2147483647 - 1,261,129,616,2048,308,1024 },
    { "C0S1",5,-3015,1972,2048,2048,2048,2048 },
    { "C0S1",-2147483647 - 1,-1787,2177,2048,1844,1024,922 },
    { "C0S1",-2147483647 - 1,-1787,129,2048,2048,1024,1024 },
    { "C0S1",-2147483647 - 1,-1787,129,2664,3892,666,973 },
    { "C0S1",6,-4859,1972,2048,2048,2048,2048 },
    { "C0S1",-2147483647 - 1,-3835,129,2048,2048,1024,1024 },
    { "C0S1",7,-6702,1972,2048,2048,2048,2048 },
    { "C0S1",-2147483647 - 1,-5883,1971,2048,206,1024,103 },
    { "C0S1",8,-8545,1972,2048,2048,2048,2048 },
    { "C0S1",-2147483647 - 1,-7931,1971,2048,206,1024,103 },
    { "C0S1",9,-10388,1972,2048,2048,2048,2048 },
    { "C0S1",-2147483647 - 1,-9979,1971,2048,206,1024,103 },
    { "C0S1",10,-12231,1972,2048,2048,2048,2048 },
    { "C0S1",11,-14075,1972,2048,2048,2048,2048 },
    { "C0S1",-2147483647 - 1,-12027,129,2048,2048,1024,1024 },
    { "C0S1",-2147483647 - 1,-14075,129,2048,2048,1024,1024 },
    { "C0S1",12,-14075,3815,2048,2048,2048,2048 },
    { "C0S1",13,-12231,3815,2048,2048,2048,2048 },
    { "C0S1",-2147483647 - 1,-14075,2177,2048,2048,1024,1024 },
    { "C0S1",14,-10388,3815,2048,2048,2048,2048 },
    { "C0S1",-2147483647 - 1,-12027,2177,2048,2048,1024,1024 },
    { "C0S1",-2147483647 - 1,-14075,129,4096,4096,1024,1024 },
    { "C0S1",15,-8545,3815,2048,2048,2048,2048 },
    { "C0S1",-2147483647 - 1,-9979,4225,2048,1638,1024,819 },
    { "C0S1",-2147483647 - 1,-9979,2177,2048,2048,1024,1024 },
    { "C0S1",16,-6702,3815,2048,2048,2048,2048 },
    { "C0S1",-2147483647 - 1,-7931,4225,2048,1638,1024,819 },
    { "C0S1",-2147483647 - 1,-7931,2177,2048,2048,1024,1024 },
    { "C0S1",-2147483647 - 1,-9979,4225,4096,1640,1024,410 },
    { "C0S1",-2147483647 - 1,-9979,1969,4096,2256,1024,564 },
    { "C0S1",17,-4859,3815,2048,2048,2048,2048 },
    { "C0S1",-2147483647 - 1,-3835,4225,1024,1638,512,819 },
    { "C0S1",-2147483647 - 1,-5883,4225,2048,1638,1024,819 },
    { "C0S1",-2147483647 - 1,-3835,2177,2048,2048,1024,1024 },
    { "C0S1",-2147483647 - 1,-5883,2177,2048,2048,1024,1024 },
    { "C0S1",-2147483647 - 1,-5883,4225,3072,1640,768,410 },
    { "C0S1",-2147483647 - 1,-5883,129,4096,4096,1024,1024 },
    { "C0S1",-2147483647 - 1,-5883,129,6760,5736,845,717 },
    { "C0S1",18,-12231,5658,2048,2048,2048,2048 },
    { "C0S1",19,-14075,5658,2048,2048,2048,2048 },
    { "C0S1",-2147483647 - 1,-12027,4225,2048,2048,1024,1024 },
    { "C0S1",-2147483647 - 1,-14075,4225,2048,2048,1024,1024 },
    { "C0S1",20,-14075,7502,2048,2048,2048,2048 },
    { "C0S1",21,-12231,7502,2048,2048,2048,2048 },
    { "C0S1",-2147483647 - 1,-12027,8321,1844,1230,922,615 },
    { "C0S1",-2147483647 - 1,-14075,8321,2048,1230,1024,615 },
    { "C0S1",-2147483647 - 1,-12027,6273,1844,2048,922,1024 },
    { "C0S1",-2147483647 - 1,-14075,8321,3892,1232,973,308 },
    { "C0S1",-2147483647 - 1,-14075,8321,3896,1232,487,154 },
    { "C0S1",-2147483647 - 1,-14075,6273,2048,2048,1024,1024 },
    { "C0S1",-2147483647 - 1,-14075,4225,4096,4096,1024,1024 },
    { "C0S1",-2147483647 - 1,-14075,129,8192,8192,1024,1024 },
    { "C0S1",-2147483647 - 1,-14075,129,14960,9424,935,589 }
    };

    CCziSubBlockDirectory subBlkDir;
    for (int i = 0; i < sizeof(data) / sizeof(data[0]); ++i)
    {
        auto entry = SubBlkEntryFromSubBlockEntryData(data + i);
        subBlkDir.AddSubBlock(entry);
    }

    subBlkDir.AddingFinished();

    auto statistics = subBlkDir.GetStatistics();
    EXPECT_EQ(statistics.subBlockCount, 161) << "wrong value";
    EXPECT_EQ(statistics.minMindex, 0) << "wrong value";
    EXPECT_EQ(statistics.maxMindex, 34) << "wrong value";
    EXPECT_EQ(statistics.boundingBox.x, -14075) << "wrong value";
    EXPECT_EQ(statistics.boundingBox.y, -6196) << "wrong value";
    EXPECT_EQ(statistics.boundingBox.w, 19695) << "wrong value";
    EXPECT_EQ(statistics.boundingBox.h, 15749) << "wrong value";

    int startC, sizeC;
    bool b = statistics.dimBounds.TryGetInterval(DimensionIndex::C, &startC, &sizeC);
    EXPECT_EQ(b, true) << "wrong value";
    EXPECT_EQ(startC, 0) << "wrong value";
    EXPECT_EQ(sizeC, 1) << "wrong value";
    int startS, sizeS;
    b = statistics.dimBounds.TryGetInterval(DimensionIndex::S, &startS, &sizeS);
    EXPECT_EQ(b, true) << "wrong value";
    EXPECT_EQ(startS, 0) << "wrong value";
    EXPECT_EQ(sizeS, 2) << "wrong value";

    //auto pyramidStatistics = subBlkDir.GetPyramidStatistics();
}