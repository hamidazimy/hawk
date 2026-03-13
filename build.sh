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
cmake .. \
    -DCMAKE_INSTALL_PREFIX=$HOME/.local \
    -DHAWK_STATIC_BUILD=ON \
    -DCMAKE_BUILD_TYPE=Release

# Build
echo ""
echo "Building..."
cmake --build . -j$(nproc 2>/dev/null || echo 4)
cmake --install .

echo ""
echo "Build complete!"
echo ""
echo "Binary location: build/bin/hawk (or ~/.local/bin/hawk if installed)"
echo "To run: ./build/bin/hawk sample_logs/sample.csv"
echo ""
