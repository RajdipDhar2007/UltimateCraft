#!/bin/sh
# Build ultimatecraft.exe on Windows. Run from the "MSYS2 UCRT64" terminal, inside this folder:
#   pacman -S --needed mingw-w64-ucrt-x86_64-gcc make     (once)
#   sh build_windows.sh
# The exe is written next to res/ (it needs that folder) and is statically linked: no DLLs to copy.
set -e
make -j"$(nproc 2>/dev/null || echo 4)"
echo
echo "Built: $(pwd)/ultimatecraft.exe  - double-click it, or run ./ultimatecraft.exe"
