// Known-answer tests for the OpenSpace loaders that the Mayhem harnesses fuzz. Inputs
// are upstream's own example datasets (data/assets/examples/...) plus a small color
// map; each test asserts parsed values, and round-trips through the binary cache.
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <openspace/data/csvloader.h>
#include <openspace/data/dataloader.h>
#include <openspace/filesystem/filesystem.h>
#include <openspace/network/messagestructures.h>

#include <filesystem>
#include <set>

using namespace openspace;
using namespace openspace::dataloader;
using Catch::Matchers::WithinAbs;

namespace {
    const std::string Examples =
        "${BASE}/data/assets/examples/renderable/renderablepointcloud/data";

    std::filesystem::path tmpPath(const char* name) {
        return std::filesystem::temp_directory_path() / name;
    }
} // namespace

TEST_CASE("Dataloader: speck file", "[dataloader]") {
    const Dataset ds = data::loadFile(
        absPath(Examples + "/multitextured_speck/textures_points.speck")
    );
    REQUIRE(ds.variables.size() == 3);
    CHECK(ds.variables[0].name == "a");
    CHECK(ds.variables[2].name == "texture");
    CHECK(ds.textureDataIndex == 2);
    REQUIRE(ds.textures.size() == 4);
    CHECK(ds.textures[3].file == "openspace-horiz-logo.png");
    REQUIRE(ds.entries.size() == 4);
    CHECK_THAT(ds.entries[0].position.x, WithinAbs(13428000.0, 1.0));
    CHECK_THAT(ds.entries[3].position.z, WithinAbs(39582000.0, 1.0));
    REQUIRE(ds.entries[1].data.size() == 3);
    CHECK_THAT(ds.entries[1].data[0], WithinAbs(45.05941924, 1e-4));
    CHECK(ds.entries[1].data[2] == 2.f);

    const std::filesystem::path cache = tmpPath("openspace-test-speck.cache");
    data::saveCachedFile(ds, cache);
    const std::optional<Dataset> back = data::loadCachedFile(cache);
    REQUIRE(back.has_value());
    CHECK(back->entries.size() == 4);
    CHECK(back->textures.size() == 4);
    CHECK(back->entries[1].data[0] == ds.entries[1].data[0]);
    std::filesystem::remove(cache);
}

TEST_CASE("Dataloader: label file", "[dataloader]") {
    const Labelset ls = label::loadFile(absPath(Examples + "/dummydata.label"));
    REQUIRE(ls.entries.size() == 100);
    CHECK(ls.entries[0].identifier == "P0");
    CHECK(ls.entries[0].text == "Point 0");
    CHECK(ls.entries[99].text == "Point 99");
    CHECK_THAT(ls.entries[99].position.y, WithinAbs(16718000.0, 1.0));

    const std::filesystem::path cache = tmpPath("openspace-test-label.cache");
    label::saveCachedFile(ls, cache);
    const std::optional<Labelset> back = label::loadCachedFile(cache);
    REQUIRE(back.has_value());
    REQUIRE(back->entries.size() == 100);
    CHECK(back->entries[42].text == "Point 42");
    std::filesystem::remove(cache);
}

TEST_CASE("Dataloader: csv file", "[dataloader]") {
    const Dataset ds = data::loadFile(absPath(Examples + "/dummydata.csv"));
    CHECK(ds.entries.size() == 100);
    CHECK(ds.index("vx") >= 0);
    CHECK(ds.index("number_withNan") >= 0);
    CHECK(ds.index("doesnotexist") == -1);
    REQUIRE(!ds.entries.empty());
    CHECK_THAT(ds.entries[0].position.x, WithinAbs(-13629320.0, 1.0));
}

TEST_CASE("Dataloader: texture map file", "[dataloader]") {
    const std::vector<Dataset::Texture> t = csv::loadTextureMapFile(
        absPath(Examples + "/multitextured_csv/texturemap.tmap"),
        std::set<int>{ 0, 1, 2, 3 }
    );
    REQUIRE(t.size() == 4);
    CHECK(t[0].file == "test3.jpg");
    CHECK(t[3].index == 3);
    CHECK(t[3].file == "openspace-horiz-logo.png");
}

TEST_CASE("Dataloader: color map", "[dataloader]") {
    const ColorMap cm = color::loadFile(absPath("${BASE}/mayhem/tests/data/sample.cmap"));
    REQUIRE(cm.entries.size() == 3);
    CHECK(cm.entries[0] == glm::vec4(1.f, 0.f, 0.f, 1.f));
    CHECK(cm.entries[2] == glm::vec4(0.f, 0.f, 1.f, 0.5f));
    REQUIRE(cm.nanColor.has_value());
    CHECK(*cm.nanColor == glm::vec4(0.5f, 0.5f, 0.5f, 0.25f));
    REQUIRE(cm.aboveRangeColor.has_value());
    CHECK(*cm.aboveRangeColor == glm::vec4(1.f));

    const std::filesystem::path cache = tmpPath("openspace-test-cmap.cache");
    color::saveCachedFile(cm, cache);
    const std::optional<ColorMap> back = color::loadCachedFile(cache);
    REQUIRE(back.has_value());
    REQUIRE(back->entries.size() == 3);
    CHECK(back->entries[2] == cm.entries[2]);
    std::filesystem::remove(cache);
}

TEST_CASE("Messagestructures: round trip", "[network]") {
    using namespace openspace::datamessagestructures;
    CameraKeyframe kf(glm::dvec3(1.0, 2.0, 3.0), glm::dquat(0.5, 0.5, 0.5, 0.5),
        "Earth", true, 2.5f);
    kf._timestamp = 42.0;
    std::vector<char> buf;
    kf.serialize(buf);
    CameraKeyframe back;
    back.deserialize(buf);
    CHECK(back._position == glm::dvec3(1.0, 2.0, 3.0));
    CHECK(back._focusNode == "Earth");
    CHECK(back._followNodeRotation);
    CHECK(back._scale == 2.5f);
    CHECK(back._timestamp == 42.0);

    ScriptMessage sm;
    sm._script = "openspace.time.setPause(true)";
    std::vector<char> sbuf;
    sm.serialize(sbuf);
    ScriptMessage sback;
    sback.deserialize(sbuf);
    CHECK(sback._script == sm._script);
}
