#!/bin/bash
# Build script for phoenix-sdr-utils (Linux/macOS)

set -e

BUILD_DIR="build"
BUILD_TYPE="${1:-Release}"

# Parse arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        --clean)
            echo "Cleaning build directory..."
            rm -rf "$BUILD_DIR"
            shift
            ;;
        --rebuild)
            echo "Rebuilding..."
            rm -rf "$BUILD_DIR"
            shift
            ;;
        --debug)
            BUILD_TYPE="Debug"
            shift
            ;;
        *)
            echo "Unknown option: $1"
            echo "Usage: $0 [--clean|--rebuild] [--debug]"
            exit 1
            ;;
    esac
done

# Create build directory
mkdir -p "$BUILD_DIR"

# Configure with CMake
echo "Configuring with CMake (${BUILD_TYPE})..."
cd "$BUILD_DIR"
cmake -DCMAKE_BUILD_TYPE="$BUILD_TYPE" ..

# Build
echo "Building..."
cmake --build . --config "$BUILD_TYPE"

echo ""
echo "Build complete!"
echo "Executables in: $BUILD_DIR"

# List built executables
echo ""
echo "Built executables:"
ls -1 iqr_play simple_am_receiver gps_time wwv_gps_verify 2>/dev/null || true
