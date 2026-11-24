// Fuzzes OpenSpace's SPECK point-cloud and LABEL file loaders through the public
// dataloader entry points (src/data/speckloader.cpp via src/data/dataloader.cpp).
// First byte selects the format; a successfully parsed dataset is also written to
// and read back from OpenSpace's binary cache format.
#include "fuzz_common.h"

#include <openspace/data/dataloader.h>

using namespace openspace::dataloader;

extern "C" int LLVMFuzzerInitialize(int*, char***) {
    fuzzhelper::initLogging();
    return 0;
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    if (size < 1) {
        return -1;
    }
    const bool label = data[0] & 1;
    std::filesystem::path p =
        fuzzhelper::writeInput(data + 1, size - 1, label ? ".label" : ".speck");
    std::filesystem::path cache = p;
    cache += ".cache";
    if (label) {
        FUZZ_GUARD(
            Labelset ls = label::loadFile(p);
            label::saveCachedFile(ls, cache);
            label::loadCachedFile(cache);
        )
    }
    else {
        FUZZ_GUARD(
            Dataset ds = data::loadFile(p);
            data::saveCachedFile(ds, cache);
            data::loadCachedFile(cache);
        )
    }
    std::error_code ec;
    std::filesystem::remove(p, ec);
    std::filesystem::remove(cache, ec);
    return 0;
}
