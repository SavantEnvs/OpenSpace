// Fuzzes OpenSpace's .cmap color map loader (src/data/colormaploader.cpp) through
// color::loadFile, then round-trips the result through the binary color map cache
// (src/data/dataloader.cpp), the code changed by upstream commit 8fddd55851.
#include "fuzz_common.h"

#include <openspace/data/dataloader.h>

using namespace openspace::dataloader;

extern "C" int LLVMFuzzerInitialize(int*, char***) {
    fuzzhelper::initLogging();
    return 0;
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    std::filesystem::path p = fuzzhelper::writeInput(data, size, ".cmap");
    std::filesystem::path cache = p;
    cache += ".cache";
    FUZZ_GUARD(
        ColorMap cm = color::loadFile(p);
        color::saveCachedFile(cm, cache);
        color::loadCachedFile(cache);
    )
    std::error_code ec;
    std::filesystem::remove(p, ec);
    std::filesystem::remove(cache, ec);
    return 0;
}
