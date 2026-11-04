Public test server of Elemental MOBA (Hubert's PC).
Address: see SERVER.txt  (in the game: Multiplayer → Direct connect → paste it)
- Runs the build of the commit named below; your game must be built from the same commit (git pull + cmake --build build).
  Server build: bbb5e1b (2026-11-04)
- Online only while that PC is on (after a restart: bash install/run-server.sh). From outside the home network the router must forward UDP 7777 to the PC.
