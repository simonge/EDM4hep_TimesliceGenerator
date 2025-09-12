#!/bin/bash

# Build script for ROOT dataframe-based timeslice merger
# Converted from podio to use ROOT libraries directly while maintaining podio format compatibility

echo "=== Building ROOT Dataframe-Based Timeslice Merger ==="

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
echo "This version uses ROOT dataframe approach instead of podio directly"
echo "while maintaining podio format compatibility through helper functions."
echo ""
echo "To run the timeslice merger:"
echo "  ./install/bin/timeslice_merger [options] input_file1 [input_file2 ...]"
echo ""
echo "Example usage:"
echo "  ./install/bin/timeslice_merger -n 10 -d 1000.0 -s -e 2 input.root"
echo ""
echo "Key improvements:"
echo "  - Uses dataframe collections with helper functions for timing/reference updates"
echo "  - Maintains podio format compatibility without direct podio dependencies"  
echo "  - Efficient index offset handling for collection references"
echo "  - Modular design allows easy integration with actual ROOT dataframes"
echo ""
echo "For help:"
echo "  ./install/bin/timeslice_merger -h"