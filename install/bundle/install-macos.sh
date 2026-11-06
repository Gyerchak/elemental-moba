#!/usr/bin/env bash
# Elemental MOBA — install from this folder (pendrive) on macOS. Needs Homebrew (https://brew.sh).
#     bash install-macos.sh            installs into ~/elemental-moba
# macOS runs on Metal: everything works, without the post effects (bloom, water shader).
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
PARENT="${1:-$HOME}"
GAME="$PARENT/elemental-moba"
command -v brew >/dev/null || { echo "Install Homebrew first: https://brew.sh"; exit 1; }

echo "== 1/4 build tools"
brew install cmake ninja llvm sdl3 sdl3_ttf enet pkgconf

echo "== 2/4 game files -> $GAME"
mkdir -p "$PARENT"
tar xzf "$HERE/elemental-moba-source.tar.gz" -C "$PARENT"

echo "== 3/4 local champions (not on GitHub)"
if [ -f "$HERE/elemental-moba-local.tar.gz" ]; then tar xzf "$HERE/elemental-moba-local.tar.gz" -C "$GAME"; else echo "   none here, skipped"; fi

echo "== 4/4 build (a few minutes)"
cd "$GAME"
export CXX="$(brew --prefix llvm)/bin/clang++"
export LDFLAGS="-L$(brew --prefix llvm)/lib/c++ -Wl,-rpath,$(brew --prefix llvm)/lib/c++"
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build

echo
echo "Done. Start:  $GAME/build/elemental_moba"
echo "Public server: Multiplayer -> Direct connect -> $(cat "$GAME/SERVER.txt")"
