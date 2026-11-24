# syntax=docker/dockerfile:1.7
#
# OpenSpace commit image. Builds on the org C/C++ base; mayhem/build.sh compiles the
# headless data-loader / network-message subset of openspace-core plus the harnesses.
FROM ghcr.io/savantenvs/base:20261023

ARG COMMIT
ARG REPO_URL
ARG SOURCE_DATE_EPOCH
ARG SANITIZER_FLAGS
ARG COVERAGE_FLAGS=
ARG MAYHEM_JOBS=

# Header-only / test deps from apt (pinned to SOURCE_DATE_EPOCH when set). glbinding is
# only needed for the GL type headers openspace/opengl/*.h include; nothing links it.
USER root
RUN [ -n "$SOURCE_DATE_EPOCH" ] && repro-sources-list.sh || true ; \
    apt-get update && apt-get install -yq --no-install-recommends \
        libglm-dev libglbinding-dev nlohmann-json3-dev catch2 \
 && rm -rf /var/lib/apt/lists/*

# scnlib (vcpkg-only upstream, not packaged by Debian): pinned release tarball, built
# static, uninstrumented, DWARF 3, into /opt/toolchains/deps (SPEC §6.2 item 19).
ARG SCNLIB_VER=4.0.1
ARG SCNLIB_SHA256=ece17b26840894cc57a7127138fe4540929adcb297524dec02c490c233ff46a7
RUN d=/opt/toolchains/deps/scnlib-${SCNLIB_VER} && mkdir -p "$d/src" /tmp/dep-build \
 && curl -fsSL -o /tmp/dep.tgz \
      https://github.com/eliaskosunen/scnlib/archive/refs/tags/v${SCNLIB_VER}.tar.gz \
 && echo "${SCNLIB_SHA256}  /tmp/dep.tgz" | sha256sum -c - \
 && tar -xzf /tmp/dep.tgz --strip-components=1 -C "$d/src" \
 && cmake -S "$d/src" -B /tmp/dep-build -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_INSTALL_PREFIX="$d" -DCMAKE_INSTALL_LIBDIR=lib \
      -DCMAKE_CXX_FLAGS="-g -gdwarf-3" -DBUILD_SHARED_LIBS=OFF \
      -DSCN_DISABLE_TOP_PROJECT=ON -DSCN_TESTS=OFF -DSCN_INSTALL=ON \
      -DSCN_DISABLE_FAST_FLOAT=ON -DSCN_DISABLE_REGEX=ON \
 && cmake --build /tmp/dep-build -j"$(nproc)" && cmake --install /tmp/dep-build \
 && rm -rf /tmp/dep-build /tmp/dep.tgz && chmod -R a+rX "$d" && ls "$d/lib/libscn.a"

USER mayhem
WORKDIR /mayhem
COPY --chown=2000:2000 . /mayhem

RUN if [ -x /opt/mayhem/ccache/prefill ]; then /opt/mayhem/ccache/prefill bash mayhem/build.sh; \
    else bash mayhem/build.sh; fi

RUN if [ -f mayhem/test.sh ]; then bash mayhem/test.sh; else echo "no mayhem/test.sh"; fi

LABEL org.opencontainers.image.source="${REPO_URL}" \
      org.opencontainers.image.revision="${COMMIT}" \
      rlenv.tier="commit" \
      rlenv.source_date_epoch="${SOURCE_DATE_EPOCH}" \
      rlenv.user="mayhem"

USER mayhem
WORKDIR /mayhem
