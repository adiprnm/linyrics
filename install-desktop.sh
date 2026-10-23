#!/usr/bin/env bash
# Installs the launcher + icon and a symlink in ~/.local/bin.
set -euo pipefail
DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
"$DIR/run.sh" --version >/dev/null

chmod +x "$DIR/run.sh" "$DIR/build/linyrics"

BIN="${XDG_BIN_HOME:-$HOME/.local/bin}"
APPS="${XDG_DATA_HOME:-$HOME/.local/share}/applications"
ICONS="${XDG_DATA_HOME:-$HOME/.local/share}/icons/hicolor/256x256/apps"
mkdir -p "$BIN" "$APPS" "$ICONS"

ln -sf "$DIR/build/linyrics" "$BIN/linyrics"
cp "$DIR/assets/icon.png" "$ICONS/linyrics.png"

cat > "$APPS/linyrics.desktop" <<DESKTOP
[Desktop Entry]
Type=Application
Name=Linyrics
GenericName=Karaoke Lyrics
Comment=Lyrics that follow what Linamp is playing
Exec="$BIN/linyrics"
Icon=linyrics
Terminal=false
StartupWMClass=linyrics
Categories=AudioVideo;Audio;Player;
Keywords=lyrics;karaoke;lrc;linamp;minilyrics;
DESKTOP

if command -v update-desktop-database >/dev/null 2>&1; then
    update-desktop-database "$APPS" >/dev/null 2>&1 || true
fi
if command -v gtk-update-icon-cache >/dev/null 2>&1; then
    gtk-update-icon-cache -q "${XDG_DATA_HOME:-$HOME/.local/share}/icons/hicolor" >/dev/null 2>&1 || true
fi

echo "Installed: $BIN/linyrics"
echo "Launcher : $APPS/linyrics.desktop"
echo "Start it with 'linyrics' (running it again toggles the window)."
