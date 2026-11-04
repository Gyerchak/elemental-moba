# Elemental MOBA — install on Windows (MSYS2 UCRT64). Run in PowerShell:
#   powershell -ExecutionPolicy Bypass -File install\windows.ps1
# Needs MSYS2 (https://www.msys2.org) installed in C:\msys64.
$msys = "C:\msys64\usr\bin\bash.exe"
if (!(Test-Path $msys)) { Write-Host "Install MSYS2 first: https://www.msys2.org (default folder C:\msys64)"; exit 1 }
& $msys -lc "pacman -S --needed --noconfirm git mingw-w64-ucrt-x86_64-cmake mingw-w64-ucrt-x86_64-ninja mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-sdl3 mingw-w64-ucrt-x86_64-sdl3-ttf mingw-w64-ucrt-x86_64-enet"
& $msys -lc "export PATH=/ucrt64/bin:`$PATH; cd ~ && (test -d elemental-moba/.git && git -C elemental-moba pull --ff-only || git clone https://github.com/Gyerchak/elemental-moba.git) && cd elemental-moba && cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release && cmake --build build"
Write-Host "Done. Start: C:\msys64\home\$env:USERNAME\elemental-moba\build\elemental_moba.exe (from the UCRT64 shell)"
