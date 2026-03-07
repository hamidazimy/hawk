# Hawk Log Analyzer

Interactive command-line tool for analyzing large log files.

## Quick Start

```bash
# Build
./build.sh

# Run
./build/hawk sample_logs/sample.csv
```

## Build Instructions

### Prerequisites
- C++17 compatible compiler (GCC 7+, Clang 5+, MSVC 2017+)
- CMake 3.15+

### Building

```bash
# Easy way (recommended)
./build.sh

# Manual way
mkdir build
cd build
cmake ..
cmake --build .
```

### Build Options

```bash
# Release build (optimized)
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
cmake --build .

# Debug build
cmake -DCMAKE_BUILD_TYPE=Debug ..
cmake --build .

# Build with API server (future)
cmake -DHAWK_BUILD_API=ON ..
cmake --build .

# Disable static linking
cmake -DHAWK_STATIC_BUILD=OFF ..
cmake --build .
```

## Project Structure

```
hawk/
├── libhawk/              # Core library
│   ├── include/hawk/     # Public API
│   └── src/              # Implementation
├── hawk-cli/             # CLI interface
│   └── src/
├── sample_logs/          # Test data
└── build/                # Build artifacts (generated)
```

## Usage

```bash
./build/hawk <log_file>

# Interactive commands:
hawk> help       # Show available commands
hawk> head 10    # Show first 10 lines
hawk> exit       # Exit
```

## Development

The project uses CMake for building. Each component is in its own directory:

- **libhawk**: Core library (no console I/O)
- **hawk-cli**: Command-line interface

## License

MIT License
