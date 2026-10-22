#!/bin/bash
set -e
cd "$(dirname "$0")"
echo "=================================================="
echo "      MedEvac Emergency Dispatch Portal"
echo "=================================================="
echo "Building C++17 server and SQLite..."
cc -O2 -c include/sqlite3.c -o sqlite3.o
g++ -std=c++17 -O2 -I. server.cpp sqlite3.o -lpthread -o server
echo "Build successful. Existing dispatch_system.db will be preserved."
( sleep 2; if command -v open >/dev/null 2>&1; then open "http://localhost:18080"; elif command -v xdg-open >/dev/null 2>&1; then xdg-open "http://localhost:18080"; fi ) >/dev/null 2>&1 &
echo "Opening http://localhost:18080"
echo "Driver portal: http://localhost:18080/driver"
echo "For trusted Wi-Fi demo only: MEDEVAC_LAN=1 ./run_portal.command"
echo "Hospital demo: http://localhost:18080/hospital"
./server
