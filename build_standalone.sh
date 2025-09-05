#!/bin/bash

# Build script for standalone timeslice merger
# This script builds the standalone application without JANA dependencies

echo "=== Building Standalone Timeslice Merger ==="

# Create build directory for standalone version
mkdir -p build_standalone
cd build_standalone

# Use the standalone CMakeLists file
cp ../CMakeLists_standalone.txt ./CMakeLists.txt

# Configure with standalone CMakeLists
cmake . -DCMAKE_INSTALL_PREFIX=../install_standalone

# Build the standalone application
make -j$(nproc)

# Install
make install

cd ..

echo "Build complete!"
echo ""
echo "To run the standalone merger:"
echo "  ./install_standalone/bin/timeslice_merger [options] input_file1 [input_file2 ...]"
echo ""
echo "Example usage:"
echo "  ./install_standalone/bin/timeslice_merger -n 10 -d 1000.0 -s -e 2 input.root"
echo ""
echo "For help:"
echo "  ./install_standalone/bin/timeslice_merger -h"