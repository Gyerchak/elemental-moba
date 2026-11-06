#!/usr/bin/env bash
# Elemental MOBA — install from this folder (pendrive) on Linux. Needs no internet for the game itself;
# only the build tools come from your package manager.
#     bash install-linux.sh            installs into ~/elemental-moba
#     bash install-linux.sh ~/Games    installs into ~/Games/elemental-moba
# An older copy is updated in place: your settings, replays and own champions stay.
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
PARENT="${1:-$HOME}"
GAME="$PARENT/elemental-moba"

echo "== 1/4 build tools (asks for your password)"
if command -v pacman >/dev/null; then sudo pacman -S --needed cmake ninja clang sdl3 sdl3_ttf enet
elif command -v apt-get >/dev/null; then sudo apt-get update && sudo apt-get install -y cmake ninja-build clang libsdl3-dev libsdl3-ttf-dev libenet-dev
elif command -v dnf >/dev/null; then sudo dnf install -y cmake ninja-build clang SDL3-devel SDL3_ttf-devel enet-devel
elif command -v zypper >/dev/null; then sudo zypper install -y cmake ninja clang SDL3-devel SDL3_ttf-devel enet-devel
else echo "Unknown package manager: install cmake, ninja, a C++23 compiler, SDL3, SDL3_ttf and enet yourself, then run this again."; fi

echo "== 2/4 game files -> $GAME"
mkdir -p "$PARENT"
tar xzf "$HERE/elemental-moba-source.tar.gz" -C "$PARENT"

echo "== 3/4 local champions (not on GitHub)"
if [ -f "$HERE/elemental-moba-local.tar.gz" ]; then tar xzf "$HERE/elemental-moba-local.tar.gz" -C "$GAME"; else echo "   none here, skipped"; fi

echo "== 4/4 build (a few minutes)"
cd "$GAME"
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build

mkdir -p "$HOME/.local/share/applications"
cat > "$HOME/.local/share/applications/elemental-moba.desktop" <<DESK
[Desktop Entry]
Type=Application
Name=Elemental MOBA
Exec="$GAME/build/elemental_moba"
Path=$GAME
Terminal=false
Categories=Game;
DESK

echo
echo "Done. Start:  $GAME/build/elemental_moba   (also in your application menu)"
echo "Public server: Multiplayer -> Direct connect -> $(cat "$GAME/SERVER.txt")"
