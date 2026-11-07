Public test server of Elemental MOBA (Hubert's PC).
Address: see SERVER.txt  (in the game: Multiplayer → Direct connect → paste it)
- Your game must have the same network protocol as the server (git pull + cmake --build build); a mismatch is refused when you join.
  Server build: see git log (2026-11-07, protocol 31)
- Online only while that PC is on (after a restart: bash install/run-server.sh). From outside the home network the router must forward UDP 7777 to the PC.
