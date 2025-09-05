#!/bin/bash

# Build script for Podio-based timeslice merger

echo "=== Building Timeslice Merger ==="

# Create build directory
mkdir -p build
cd build

# Configure with CMake
cmake .. -DCMAKE_INSTALL_PREFIX=../install

# Build the application
make -j$(nproc)

# Install
make install

cd ..

echo "Build complete!"
echo ""
echo "To run the timeslice merger:"
echo "  ./install/bin/timeslice_merger [options] input_file1 [input_file2 ...]"
echo ""
echo "Example usage:"
echo "  ./install/bin/timeslice_merger -n 10 -d 1000.0 -s -e 2 input.root"
echo ""
echo "For help:"
echo "  ./install/bin/timeslice_merger -h"