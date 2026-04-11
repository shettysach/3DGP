#!/usr/bin/env bash
set -e

echo "Building Terrain Demo..."

cmake -S . -B build
cmake --build build -j

echo "Done! Run: ./build/terrain_demo"
echo "Press ESC to close window."
