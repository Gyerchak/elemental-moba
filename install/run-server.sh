#!/usr/bin/env bash
# Start the public server, detached (it keeps running after the terminal closes; a PC restart stops it).
#   bash install/run-server.sh [port]        default 7777 (UDP). Log: logs/server.log
set -euo pipefail
cd "$(dirname "$0")/.."
PORT="${1:-7777}"
if ss -uln | grep -q ":$PORT "; then echo "Port $PORT is already in use: a server is probably running."; exit 1; fi
mkdir -p logs
setsid nohup ./build/em_server --nogui --port "$PORT" --unranked > logs/server.log 2>&1 < /dev/null &
sleep 2
ss -uln | grep -q ":$PORT " && echo "Server running on UDP $PORT." || { echo "It did not start: see logs/server.log"; exit 1; }
