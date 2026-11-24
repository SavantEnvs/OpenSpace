// Fuzzes OpenSpace's CSV point-dataset loader (src/data/csvloader.cpp, which uses
// src/misc/csvreader.cpp) through data::loadFile on a .csv file, and the texture map
// file reader csv::loadTextureMapFile. First byte selects the entry point.
#include "fuzz_common.h"

#include <openspace/data/csvloader.h>
#include <openspace/data/dataloader.h>

#include <set>

using namespace openspace::dataloader;

extern "C" int LLVMFuzzerInitialize(int*, char***) {
    fuzzhelper::initLogging();
    return 0;
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    if (size < 1) {
        return -1;
    }
    const bool tmap = data[0] & 1;
    std::filesystem::path p =
        fuzzhelper::writeInput(data + 1, size - 1, tmap ? ".tmap" : ".csv");
    if (tmap) {
        FUZZ_GUARD(csv::loadTextureMapFile(p, std::set<int>{ 0, 1, 2, 3 }))
    }
    else {
        FUZZ_GUARD(data::loadFile(p))
    }
    std::error_code ec;
    std::filesystem::remove(p, ec);
    return 0;
}
