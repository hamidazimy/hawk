#!/bin/bash
set -e

echo "Hawk Log Analyzer - Windows Cross-Build Script"
echo "=============================================="
echo ""

# Create a separate build directory for Windows
mkdir -p build-win
cd build-win

# Configure with CMake using the Toolchain file, forcing static build
echo "Configuring with CMake..."
cmake .. \
    -DCMAKE_TOOLCHAIN_FILE=../mingw-toolchain.cmake \
    -DHAWK_STATIC_BUILD=ON \
    -DCMAKE_BUILD_TYPE=Release

# Build
echo ""
echo "Building hawk.exe..."
cmake --build . -j$(nproc 2>/dev/null || echo 4)


echo ""
echo "Build complete!"
echo ""
echo "Binary location: build-win/bin/hawk.exe"
