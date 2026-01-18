#!/bin/bash

# Lithium Browser Engine Build Script
# Usage: ./build.sh [options]
#   -c, --clean       Clean build (remove build directory first)
#   -d, --debug       Build with Debug configuration (default: Release)
#   -p, --profile     Build with profiling enabled (gprof + VM profiler)
#   -t, --test        Run tests after building
#   -j, --jobs N      Number of parallel jobs (default: auto-detect)
#   -h, --help        Show this help message

set -e

# Default values
BUILD_TYPE="Release"
CLEAN_BUILD=false
RUN_TESTS=false
PROFILE_BUILD=false
JOBS=$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)

# Parse arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        -c|--clean)
            CLEAN_BUILD=true
            shift
            ;;
        -d|--debug)
            BUILD_TYPE="Debug"
            shift
            ;;
        -p|--profile)
            BUILD_TYPE="Profile"
            PROFILE_BUILD=true
            shift
            ;;
        -t|--test)
            RUN_TESTS=true
            shift
            ;;
        -j|--jobs)
            JOBS="$2"
            shift 2
            ;;
        -h|--help)
            head -10 "$0" | tail -8
            exit 0
            ;;
        *)
            echo "Unknown option: $1"
            exit 1
            ;;
    esac
done

# Get script directory
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

# Set build directory based on build type
BUILD_DIR="build/$BUILD_TYPE"

echo "========================================"
echo "Lithium Browser Engine Build"
echo "========================================"
echo "Build type: $BUILD_TYPE"
echo "Build dir:  $BUILD_DIR"
echo "Jobs:       $JOBS"
echo ""

# Clean build if requested
if [ "$CLEAN_BUILD" = true ]; then
    echo "Cleaning build directory..."
    rm -rf "$BUILD_DIR"
fi

# Configure
echo "Configuring CMake..."
if [ "$PROFILE_BUILD" = true ]; then
    # Profile build: RelWithDebInfo + gprof flags
    cmake -B "$BUILD_DIR" \
        -DCMAKE_BUILD_TYPE=RelWithDebInfo \
        -DCMAKE_CXX_FLAGS="-pg -fno-omit-frame-pointer" \
        -DCMAKE_C_FLAGS="-pg -fno-omit-frame-pointer" \
        -DCMAKE_EXE_LINKER_FLAGS="-pg" \
        -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
else
    cmake -B "$BUILD_DIR" \
        -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
        -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
fi

# Build
echo ""
echo "Building..."
cmake --build "$BUILD_DIR" -j"$JOBS"

echo ""
echo "========================================"
echo "Build completed successfully!"
echo "========================================"

# Run tests if requested
if [ "$RUN_TESTS" = true ]; then
    echo ""
    echo "Running tests..."
    cd "$BUILD_DIR"
    ctest --output-on-failure
fi
