#!/usr/bin/env bash
# Elemental MOBA — install on macOS (Homebrew). bash install/macos.sh
set -euo pipefail
echo "NOTE: the game is Vulkan only [U-496]; macOS has no native Vulkan, so it will probably refuse to start there."
command -v brew >/dev/null || { echo "Install Homebrew first: https://brew.sh"; exit 1; }
brew install git cmake ninja llvm sdl3 sdl3_ttf enet
GAME="$HOME/elemental-moba"
[ -d "$GAME/.git" ] && git -C "$GAME" pull --ff-only || git clone https://github.com/Gyerchak/elemental-moba.git "$GAME"
cd "$GAME"
CXX="$(brew --prefix llvm)/bin/clang++" cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
echo "Done. Start: $GAME/build/elemental_moba   Server: $(cat SERVER.txt)"
