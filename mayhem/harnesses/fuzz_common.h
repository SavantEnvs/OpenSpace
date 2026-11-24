// Shared helpers for the OpenSpace fuzz harnesses (Mayhem integration).
//
// Every OpenSpace data loader is path based and picks its parser from the file
// extension, so each input is written to a scratch file (with the extension the
// loader expects) under $TMPDIR and handed to the loader's public entry point.
#pragma once

#include <openspace/logging/logmanager.h>
#include <openspace/misc/assert.h>

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <string>
#include <unistd.h>

namespace fuzzhelper {

// The loaders log through LogManager, which must exist. No log sink is attached,
// so messages are dropped; this is read-only after initialization.
inline void initLogging() {
    if (!openspace::logging::LogManager::isInitialized()) {
        openspace::logging::LogManager::initialize(
            openspace::logging::LogLevel::NoLogging,
            openspace::logging::LogManager::ImmediateFlush::No
        );
    }
}

// Writes `data` to a per-process scratch file ending in `ext` and returns its path.
// The same file is overwritten for every input, so nothing carries over.
inline std::filesystem::path writeInput(const uint8_t* data, size_t size,
                                        const char* ext)
{
    const char* tmp = std::getenv("TMPDIR");
    std::string dir = (tmp && *tmp) ? tmp : "/tmp";
    std::filesystem::path p = std::filesystem::path(dir) /
        ("openspace-fuzz-" + std::to_string(getpid()) + ext);
    FILE* f = std::fopen(p.c_str(), "wb");
    if (!f) {
        std::abort();
    }
    if (size > 0 && std::fwrite(data, 1, size, f) != size) {
        std::abort();
    }
    std::fclose(f);
    return p;
}

} // namespace fuzzhelper

// The project is built with OPENSPACE_THROW_ON_ASSERT (upstream's own switch), so a
// failed internal assertion throws AssertionException instead of prompting on stdin.
// An assertion failure is a bug, so it is lifted to abort(); every other exception is
// the loader's documented way of rejecting malformed input.
#define FUZZ_GUARD(...)                                                     \
    try {                                                                   \
        __VA_ARGS__;                                                        \
    }                                                                       \
    catch (const openspace::AssertionException& e) {                        \
        std::fprintf(stderr, "OpenSpace assertion: %s\n", e.what());       \
        std::abort();                                                       \
    }                                                                       \
    catch (const std::exception&) {                                         \
    }
