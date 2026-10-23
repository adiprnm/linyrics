#!/usr/bin/env bash
# ---------------------------------------------------------------------------
# Build a static, minimal libcurl for Linyrics.
#
#   packaging/build-curl.sh <install-prefix>
#
# Linyrics only needs HTTPS GET with the system CA store. Everything else
# (HTTP/2, ssh, ldap, brotli, zstd, idn) is disabled so the static library stays
# small enough to link into a single ~1 MB binary.
#
# Linux/macOS: OpenSSL from the system (libssl-dev / brew openssl).
# ---------------------------------------------------------------------------
set -euo pipefail

PREFIX="${1:?usage: build-curl.sh <install-prefix>}"
VERSION="${CURL_VERSION:-8.23.0}"
JOBS="${JOBS:-$(getconf _NPROCESSORS_ONLN 2>/dev/null || echo 4)}"
WORK="${CURL_BUILD_DIR:-$PWD/.curl-build}"

mkdir -p "$WORK" "$PREFIX"
cd "$WORK"

TARBALL="curl-$VERSION.tar.gz"
if [ ! -f "$TARBALL" ]; then
    URL="https://curl.se/download/$TARBALL"
    echo "==> downloading $URL"
    if command -v curl >/dev/null 2>&1; then
        curl -fL -o "$TARBALL" "$URL"
    else
        wget -O "$TARBALL" "$URL"
    fi
fi

SRC="curl-$VERSION"
if [ ! -d "$SRC" ]; then
    tar xf "$TARBALL"
fi
cd "$SRC"

echo "==> configuring"
# Windows has its own TLS stack; using it avoids shipping OpenSSL DLLs.
TLS_ARGS="--with-openssl"
if [ "${CURL_TLS:-openssl}" = "schannel" ]; then
    TLS_ARGS="--with-schannel"
fi
./configure \
    --prefix="$PREFIX" \
    --disable-shared --enable-static \
    $TLS_ARGS \
    --without-libpsl --without-brotli --without-zstd --without-nghttp2 \
    --without-libssh2 --without-libidn2 \
    --disable-ldap --disable-ldaps --disable-rtsp --disable-dict \
    --disable-telnet --disable-tftp --disable-pop3 --disable-imap \
    --disable-smtp --disable-gopher --disable-mqtt \
    --disable-manual --disable-docs --disable-versioned-symbols \
    --enable-ipv6

echo "==> building with $JOBS jobs"
make -j"$JOBS"
make install

echo "==> done: $PREFIX"
