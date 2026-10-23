#!/usr/bin/env bash
# ---------------------------------------------------------------------------
# Build a static SDL3 for Linamp.
#
#   packaging/build-sdl3.sh <install-prefix> [static|shared]
#
# Wayland/X11/ALSA/PulseAudio are always loaded at runtime through dlopen(),
# so neither variant needs extra libraries bundled (besides SDL itself when
# built shared). `static` (default) produces a single self-contained binary.
# ---------------------------------------------------------------------------
set -euo pipefail

PREFIX="${1:?usage: build-sdl3.sh <install-prefix> [static|shared]}"
LINKAGE="${2:-static}"
if [ "$LINKAGE" = "shared" ]; then
    SDL_SHARED=ON
    SDL_STATIC=OFF
else
    SDL_SHARED=OFF
    SDL_STATIC=ON
fi
VERSION="${SDL3_VERSION:-3.4.18}"
JOBS="${JOBS:-$(getconf _NPROCESSORS_ONLN 2>/dev/null || echo 4)}"
WORK="${SDL3_BUILD_DIR:-$PWD/.sdl3-build}"
TAG="release-$VERSION"

mkdir -p "$WORK" "$PREFIX"
cd "$WORK"

TARBALL="SDL3-$VERSION.tar.gz"
if [ ! -f "$TARBALL" ]; then
    URL="https://github.com/libsdl-org/SDL/releases/download/$TAG/$TARBALL"
    echo "==> downloading $URL"
    if command -v curl >/dev/null 2>&1; then
        curl -fL -o "$TARBALL" "$URL"
    else
        wget -O "$TARBALL" "$URL"
    fi
fi

SRC="SDL3-$VERSION"
if [ ! -d "$SRC" ]; then
    tar xf "$TARBALL"
fi

mkdir -p "build-$VERSION"
cd "build-$VERSION"

echo "==> configuring"
cmake "../$SRC" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX="$PREFIX" \
    -DSDL_SHARED="$SDL_SHARED" \
    -DSDL_STATIC="$SDL_STATIC" \
    -DSDL_TEST_LIBRARY=OFF \
    -DSDL_TESTS=OFF \
    -DSDL_EXAMPLES=OFF \
    -DSDL_INSTALL_TESTS=OFF \
    -DSDL_UNIX_CONSOLE_BUILD=OFF

echo "==> building with $JOBS jobs"
cmake --build . --parallel "$JOBS"
cmake --install .

echo "==> done: $PREFIX"
