#!/usr/bin/env bash
# ---------------------------------------------------------------------------
# Assemble the Linux AppImage.
#
#   packaging/make-appimage.sh <binary> <sdl3-prefix> [output.AppImage]
#
# libcurl is linked statically (packaging/build-curl.sh), so SDL3 is the only
# shared library that has to travel with the binary — SDL3 itself dlopen()s the
# Wayland/X11/ALSA/PipeWire of the host at runtime.
# ---------------------------------------------------------------------------
set -euo pipefail

BIN="${1:?usage: make-appimage.sh <binary> <sdl3-prefix> [output.AppImage]}"
SDL3_PREFIX="${2:?usage: make-appimage.sh <binary> <sdl3-prefix> [output.AppImage]}"
OUT="${3:-linyrics-linux-x86_64.AppImage}"
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT
APPDIR="$WORK/AppDir"

install -Dm755 "$BIN" "$APPDIR/usr/bin/linyrics"
strip --strip-unneeded "$APPDIR/usr/bin/linyrics" || true

mkdir -p "$APPDIR/usr/lib"
cp -a "$SDL3_PREFIX"/lib/libSDL3.so* "$APPDIR/usr/lib/"

install -Dm644 "$ROOT/packaging/linyrics.desktop" "$APPDIR/usr/share/applications/linyrics.desktop"
install -Dm644 "$ROOT/assets/icon.png" "$APPDIR/usr/share/icons/hicolor/256x256/apps/linyrics.png"
install -m644 "$ROOT/packaging/linyrics.desktop" "$APPDIR/linyrics.desktop"
install -m644 "$ROOT/assets/icon.png" "$APPDIR/linyrics.png"

cat > "$APPDIR/AppRun" <<'APPRUN'
#!/bin/sh
HERE="$(dirname "$(readlink -f "$0")")"
export LD_LIBRARY_PATH="$HERE/usr/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
exec "$HERE/usr/bin/linyrics" "$@"
APPRUN
chmod +x "$APPDIR/AppRun"

TOOL="$WORK/appimagetool"
if [ ! -x "$TOOL" ]; then
    curl -fsSL -o "$TOOL" \
        https://github.com/AppImage/appimagetool/releases/download/continuous/appimagetool-x86_64.AppImage
    chmod +x "$TOOL"
fi

ARCH=x86_64 "$TOOL" --appimage-extract-and-run "$APPDIR" "$OUT"
echo "==> $OUT ($(du -h "$OUT" | cut -f1))"
