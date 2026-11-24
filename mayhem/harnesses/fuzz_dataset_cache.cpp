// Fuzzes the binary cache deserializers in src/data/dataloader.cpp. OpenSpace writes
// these caches to disk and reads them back on later runs (loadFileWithCache), so a
// cache file is input the loader does not produce in-process. First byte selects the
// dataset, label set or color map cache reader.
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
    const uint8_t which = data[0] % 3;
    std::filesystem::path p = fuzzhelper::writeInput(data + 1, size - 1, ".cache");
    switch (which) {
        case 0: FUZZ_GUARD(data::loadCachedFile(p)) break;
        case 1: FUZZ_GUARD(label::loadCachedFile(p)) break;
        default: FUZZ_GUARD(color::loadCachedFile(p)) break;
    }
    std::error_code ec;
    std::filesystem::remove(p, ec);
    return 0;
}
