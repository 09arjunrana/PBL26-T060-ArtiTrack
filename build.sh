#!/usr/bin/env bash
# Build without make:  ./build.sh   (works in Git Bash / Linux / macOS)
set -e
CFLAGS="-std=c11 -Wall -Wextra -O2"
CXXFLAGS="-std=c++17 -Wall -Wextra -O2"
gcc $CFLAGS -c m3_core.c -o m3_core.o
gcc $CFLAGS -c m4.c -o m4.o
for f in main m0 m1 m2 m3 bridge; do g++ $CXXFLAGS -c $f.cpp -o $f.o; done
g++ -o artitrack main.o m0.o m1.o m2.o m3.o m3_core.o m4.o bridge.o
echo "Built ./artitrack"
