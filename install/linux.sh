#!/usr/bin/env bash
# Elemental MOBA — install on Linux (Arch / CachyOS / Manjaro, Debian / Ubuntu, Fedora, openSUSE).
#   bash install/linux.sh            inside a downloaded copy of the repo, or:
#   curl -fsSL https://raw.githubusercontent.com/Gyerchak/elemental-moba/main/install/linux.sh | bash
# Installs the build tools, gets / updates the game in ~/elemental-moba (or the copy it runs in), builds it,
# unpacks own content (aatrox-local.tar.gz next to the script or in ~/Downloads, if present) and prints the server.
set -euo pipefail
echo "== 1/4 build tools"
if command -v pacman >/dev/null; then sudo pacman -S --needed git cmake ninja clang sdl3 sdl3_ttf enet
elif command -v apt-get >/dev/null; then sudo apt-get update && sudo apt-get install -y git cmake ninja-build clang libsdl3-dev libsdl3-ttf-dev libenet-dev
elif command -v dnf >/dev/null; then sudo dnf install -y git cmake ninja-build clang SDL3-devel SDL3_ttf-devel enet-devel
elif command -v zypper >/dev/null; then sudo zypper install -y git cmake ninja clang SDL3-devel SDL3_ttf-devel enet-devel
else echo "Unknown package manager: install git, cmake, ninja, a C++23 compiler, SDL3, SDL3_ttf, enet yourself."; fi

echo "== 2/4 the game"
SELF_DIR="$(cd "$(dirname "${BASH_SOURCE[0]:-$0}")" 2>/dev/null && pwd || echo .)"
if [ -f "$SELF_DIR/../CMakeLists.txt" ] && grep -q elemental "$SELF_DIR/../CMakeLists.txt" 2>/dev/null; then GAME="$(cd "$SELF_DIR/.." && pwd)"
else GAME="$HOME/elemental-moba"; fi
REPO=https://github.com/Gyerchak/elemental-moba.git
if [ -d "$GAME/.git" ]; then git -C "$GAME" pull --ff-only
elif [ -f "$GAME/CMakeLists.txt" ]; then
  # a copy without git data (e.g. from a pendrive): bring its game files up to date; own files (Aatrox, settings) stay
  git -C "$GAME" init -q -b main
  git -C "$GAME" remote add origin "$REPO" 2>/dev/null || true
  git -C "$GAME" fetch -q --depth 1 origin main
  git -C "$GAME" reset -q --hard FETCH_HEAD
else git clone "$REPO" "$GAME"; fi
cd "$GAME"

echo "== 3/4 own content (local files that are not on GitHub)"
for t in "$SELF_DIR/aatrox-local.tar.gz" "$HOME/Downloads/aatrox-local.tar.gz"; do
  [ -f "$t" ] && { tar xzf "$t" -C "$GAME"; echo "   unpacked $t"; break; }
done

echo "== 4/4 build"
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
echo
echo "Done. Start:   $GAME/build/elemental_moba"
echo "Server:        Multiplayer → Direct connect → $(cat SERVER.txt 2>/dev/null || echo '(see SERVER.txt)')"
