// Catch2 runner for the Mayhem functional tests. Mirrors the parts of upstream's
// tests/main.cpp that the data loader tests need (logging + FileSystem with the
// ${TESTDIR} token), without the engine, Lua or SPICE.
#include <catch2/catch_session.hpp>

#include <openspace/filesystem/filesystem.h>
#include <openspace/logging/logmanager.h>

#include <cstdlib>
#include <filesystem>

int main(int argc, char** argv) {
    openspace::logging::LogManager::initialize(
        openspace::logging::LogLevel::Warning,
        openspace::logging::LogManager::ImmediateFlush::Yes
    );
    openspace::filesystem::FileSystem::initialize();
    const char* src = std::getenv("SRC");
    const std::filesystem::path base = (src && *src) ? src : "/mayhem";
    FileSys.registerPathToken("${BASE}", base);
    FileSys.registerPathToken("${TESTDIR}", base / "tests");
    return Catch::Session().run(argc, argv);
}
