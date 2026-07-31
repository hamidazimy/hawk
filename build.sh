#!/bin/bash
set -e

echo "Hawk Log Analyzer - Build Script"
echo "================================"
echo ""

# Create build directory
mkdir -p build
cd build

# Configure with CMake
echo "Configuring with CMake..."
cmake -S .. -B . \
    -DCMAKE_INSTALL_PREFIX=$HOME/.local \
    -DHAWK_STATIC_BUILD=ON \
    -DHAWK_BUILD_TESTS=OFF \
    -DCMAKE_BUILD_TYPE=Release

# Build
echo ""
echo "Building..."
cmake --build . -j$(nproc 2>/dev/null || echo 4)

echo ""
echo "Build complete!"
echo ""
echo "Binary location: build/bin/hawk"
echo "To run: ./build/bin/hawk sample_logs/auth.csv"
echo ""
