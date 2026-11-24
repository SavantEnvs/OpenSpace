#!/usr/bin/env bash
#
# mayhem/build.sh — builds the OpenSpace fuzz harnesses and the functional test runner.
#
# OpenSpace's full CMake build needs a vcpkg tree of ~40 packages (sgct, gdal, assimp,
# cef, Qt, ...) and an OpenGL window just to run. The harnesses only need the headless
# data loaders and wire-message decoders of `openspace-core`, so this script compiles
# exactly those upstream translation units (listed in CORE_SRCS) straight from src/,
# after running upstream's own code generator (support/codegen) the way the CMake
# `run_codegen` target does. Header-only/external deps come from apt (glm, glbinding
# headers, nlohmann-json, catch2) and scnlib from /opt/toolchains/deps (vcpkg-only).
#
# Objects are built with -ffunction-sections and linked with --gc-sections: the color
# map loader also contains a GL texture constructor (loadColorMapTexture) that needs a
# GL context and the stb/glbinding runtime; it is unreachable from the harnesses and is
# discarded at link time (lld --gc-sections) instead of being stubbed.
set -euo pipefail

[ -n "${SOURCE_DATE_EPOCH:-}" ] || unset SOURCE_DATE_EPOCH

: "${SANITIZER_FLAGS=-fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer}"
: "${DEBUG_FLAGS:=-g -gdwarf-3}"
: "${CC:=clang}" ; : "${CXX:=clang++}" ; : "${LIB_FUZZING_ENGINE:=-fsanitize=fuzzer}"
: "${MAYHEM_JOBS:=$(nproc)}"
: "${COVERAGE_FLAGS=}"
: "${SRC:=/mayhem}"
: "${STANDALONE_FUZZ_MAIN:=/opt/mayhem/StandaloneFuzzTargetMain.c}"
: "${SCNLIB_DIR:=/opt/toolchains/deps/scnlib-4.0.1}"
export SANITIZER_FLAGS DEBUG_FLAGS CC CXX LIB_FUZZING_ENGINE MAYHEM_JOBS COVERAGE_FLAGS

cd "$SRC"
B="$SRC/mayhem/build"          # gitignored by mayhem/.gitignore
BIN="$B/bin"
mkdir -p "$B/codegen" "$B/fuzz" "$B/test" "$BIN"

# 1) upstream code generator (a build tool: plain flags, never instrumented), then run it
#    over modules/ and src/ exactly as the top-level CMakeLists' run_codegen target does.
CG_SRCS="codegen parsing snippets types util verifier main"
cg_objs=()
for f in $CG_SRCS; do
    "$CXX" -std=c++23 -O1 -c "support/codegen/$f.cpp" -o "$B/codegen/$f.o" &
    cg_objs+=("$B/codegen/$f.o")
done
wait
"$CXX" "${cg_objs[@]}" -o "$B/codegen/codegen-tool"
"$B/codegen/codegen-tool" modules src > "$B/codegen/codegen.log"

CORE_SRCS="
src/data/dataloader.cpp
src/data/speckloader.cpp
src/data/csvloader.cpp
src/data/colormaploader.cpp
src/data/datamapping.cpp
src/misc/csvreader.cpp
src/misc/stringhelper.cpp
src/misc/exception.cpp
src/misc/assert.cpp
src/misc/stacktrace.cpp
src/misc/crc32.cpp
src/misc/dictionary.cpp
src/logging/logmanager.cpp
src/logging/log.cpp
src/logging/consolelog.cpp
src/util/progressbar.cpp
src/network/messagestructures.cpp
src/documentation/documentation.cpp
src/documentation/verifier.cpp
src/filesystem/filesystem.cpp
src/filesystem/filesystem.linux.cpp
src/filesystem/cachemanager.cpp
"
# stacktrace.cpp uses std::array without including <array> (works through vcpkg's
# transitive includes upstream), hence the forced include.
COMMON="-std=c++23 -w -include array -include mayhem/harnesses/assert_throw.h -Iinclude -I$SCNLIB_DIR/include -ffunction-sections -fdata-sections -DOPENSPACE_THROW_ON_ASSERT"

# GNU ld keeps ASan-instrumented globals (and the GL/engine code they reference) alive
# under --gc-sections; lld strips them, so link with lld.
LDGC="-fuse-ld=lld -Wl,--gc-sections"

# compile_set <outdir> <flags...>: compile CORE_SRCS into <outdir>/libcore.a
compile_set() {
    local out="$1"; shift
    local flags="$*"
    rm -f "$out"/*.o "$out/libcore.a"
    echo "$CORE_SRCS" | grep -v '^$' | \
        xargs -P "$MAYHEM_JOBS" -I{} sh -c \
          'o="$1/$(echo "$2" | tr / _).o"; '"$CXX"' '"$flags"' -c "$2" -o "$o"' _ "$out" {}
    ar rcs "$out/libcore.a" "$out"/*.o
}

# 2) fuzz build: the project code itself with the sanitizers + DWARF 3
# SanCov: the base contract adds coverage only at link (-fsanitize=fuzzer); the project
# objects need -fsanitize=fuzzer-no-link so libFuzzer sees their edges.
COV_FLAGS="-fsanitize=fuzzer-no-link"
compile_set "$B/fuzz" -O1 $COMMON $SANITIZER_FLAGS $COV_FLAGS $DEBUG_FLAGS
"$CXX" $SANITIZER_FLAGS $DEBUG_FLAGS -c mayhem/lsan_off.cc -o "$B/fuzz/lsan_off.o.x"
"$CC" $SANITIZER_FLAGS $DEBUG_FLAGS -c "$STANDALONE_FUZZ_MAIN" -o "$B/fuzz/standalone_main.o.x"

TARGETS="fuzz_speck_loader fuzz_csv_loader fuzz_colormap_loader fuzz_dataset_cache"
for t in $TARGETS; do
    "$CXX" -O1 $COMMON $SANITIZER_FLAGS $COV_FLAGS $DEBUG_FLAGS -c "mayhem/harnesses/$t.cpp" -o "$B/fuzz/$t.o.x"
    "$CXX" $SANITIZER_FLAGS $DEBUG_FLAGS $LIB_FUZZING_ENGINE $LDGC \
        "$B/fuzz/$t.o.x" "$B/fuzz/lsan_off.o.x" "$B/fuzz/libcore.a" "$SCNLIB_DIR/lib/libscn.a" \
        -o "$BIN/$t"
    "$CXX" $SANITIZER_FLAGS $DEBUG_FLAGS $LDGC \
        "$B/fuzz/$t.o.x" "$B/fuzz/standalone_main.o.x" "$B/fuzz/lsan_off.o.x" \
        "$B/fuzz/libcore.a" "$SCNLIB_DIR/lib/libscn.a" -o "$BIN/$t-standalone"
done

# 3) functional test runner, NORMAL flags (no sanitizers): upstream's own
#    tests/test_csvreader.cpp + known-answer tests for the fuzzed loaders.
compile_set "$B/test" -O1 $COMMON $COVERAGE_FLAGS
TEST_SRCS="tests/test_csvreader.cpp mayhem/tests/test_main.cpp mayhem/tests/test_dataloaders.cpp"
t_objs=()
for f in $TEST_SRCS; do
    o="$B/test/$(echo "$f" | tr / _).o.x"
    "$CXX" -O1 $COMMON $COVERAGE_FLAGS -c "$f" -o "$o"
    t_objs+=("$o")
done
"$CXX" $COVERAGE_FLAGS $LDGC "${t_objs[@]}" "$B/test/libcore.a" \
    "$SCNLIB_DIR/lib/libscn.a" -lCatch2 -o "$BIN/openspace_tests"

echo "built: $(ls "$BIN" | tr '\n' ' ')"
