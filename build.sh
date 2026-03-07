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
cmake ..

# Build
echo ""
echo "Building..."
cmake --build . -j$(nproc 2>/dev/null || echo 4)

echo ""
echo "Build complete!"
echo ""
echo "Binary location: build/bin/hawk"
echo "To run: ./build/bin/hawk sample_logs/sample.csv"
echo ""
echo "Or from build directory:"
echo "  cd build"
echo "  ./bin/hawk ../sample_logs/sample.csv"
