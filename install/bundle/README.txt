ELEMENTAL MOBA — THE WHOLE GAME ON A PENDRIVE
=============================================

INSTALL (pick your system)
  Linux:    open a terminal in this folder:  bash install-linux.sh
  Windows:  double-click  install-windows.cmd
  macOS:    open a terminal in this folder:  bash install-macos.sh  (needs Homebrew)

  The game goes to the elemental-moba folder in your home folder.
  Installing again updates it: settings, replays and own champions stay.

PLAY ONLINE
  Multiplayer -> the public server is already in the list.
  Or Direct connect -> the address in SERVER.txt
  Your game and the server must be the same build. "Version / protocol" message = install the newer copy.

PLAY AT HOME (same Wi-Fi)
  One computer: Host match (port 7777).
  The other: Multiplayer -> Direct connect -> <address of the first computer>:7777

HOST YOUR OWN SERVER
  Start the server app from Multiplayer -> Server app, or the program em_server.
  Without a window, Linux and macOS:  build/em_server --nogui --port 7777
  Without a window, Windows:          em_server.exe --nogui --port 7777

THE MAKERS
  In the menu, column Create: champions, maps, items, loadouts, units. They open inside the game window.
  Each is also a program of its own: em_champion_maker, em_map_maker, em_item_maker, em_loadout_maker, em_unit_maker.
  F9 in the practice tool or a replay: the camera and what is shown. F10: record a video (needs ffmpeg).

WHAT IS HERE
  elemental-moba-source.tar.gz   the game: code, pictures, sounds, languages
  elemental-moba-local.tar.gz    Aatrox and Taliyah (private test copies, not on GitHub; do not share)
  windows-programs\              the ready Windows programs
  SERVER.txt                     address of the public server
  SHA256SUMS.txt                 check the copy on Linux:  sha256sum -c SHA256SUMS.txt
