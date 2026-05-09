#!/usr/bin/env bash
set -e

cmake -S . -B build
cmake --build build -j

echo "Run: ./build/terrain_demo <graph|view>"
