#!/bin/bash

# Test script for JANA2 plugin
# This script demonstrates how to test the TimeframeBuilder JANA2 plugin
# when JANA2 and all dependencies are available.

set -e

echo "=========================================="
echo "TimeframeBuilder JANA2 Plugin Test Script"
echo "=========================================="
echo ""

# Check if JANA2 is available
if ! command -v jana &> /dev/null; then
    echo "ERROR: JANA2 not found in PATH"
    echo "Please install JANA2 or set up your environment first."
    exit 1
fi

echo "Found JANA2: $(which jana)"
echo "JANA2 version: $(jana --version 2>&1 | head -1 || echo 'unknown')"
echo ""

# Check for required environment
if [ -z "$JANA_PLUGIN_PATH" ]; then
    echo "WARNING: JANA_PLUGIN_PATH not set"
    echo "Setting to: $(pwd)/../install/lib"
    export JANA_PLUGIN_PATH="$(pwd)/../install/lib"
fi

echo "JANA_PLUGIN_PATH: $JANA_PLUGIN_PATH"
echo ""

# Build the project
echo "Building TimeframeBuilder with JANA2 plugin..."
cd "$(dirname "$0")/.."
mkdir -p build
cd build

cmake .. \
    -DCMAKE_INSTALL_PREFIX=../install \
    -DBUILD_JANA_PLUGIN=ON

make -j$(nproc)
make install

echo ""
echo "Build complete!"
echo ""

# Check if plugin was built
PLUGIN_LIB="../install/lib/libtimeframe_builder_plugin.so"
if [ ! -f "$PLUGIN_LIB" ]; then
    echo "ERROR: Plugin library not found at: $PLUGIN_LIB"
    exit 1
fi

echo "Plugin library found: $PLUGIN_LIB"
echo ""

# Test with sample data (if available)
if [ $# -eq 0 ]; then
    echo "Usage: $0 <input_file.edm4hep.root>"
    echo ""
    echo "No input file provided. To test the plugin, provide an EDM4hep input file:"
    echo "  ./test_jana_plugin.sh /path/to/input.edm4hep.root"
    echo ""
    echo "The plugin will process the input file and generate merged timeframes."
    exit 0
fi

INPUT_FILE="$1"

if [ ! -f "$INPUT_FILE" ]; then
    echo "ERROR: Input file not found: $INPUT_FILE"
    exit 1
fi

echo "Testing JANA2 plugin with input: $INPUT_FILE"
echo ""

# Run JANA with the plugin
jana \
    -Pplugins=timeframe_builder_plugin \
    -Ptfb:max_timeframes=10 \
    -Ptfb:timeframe_duration=2000.0 \
    -Ptfb:output_file=test_merged.edm4hep.root \
    -Pnthreads=1 \
    "$INPUT_FILE"

echo ""
echo "=========================================="
echo "Test completed successfully!"
echo "=========================================="
echo ""
echo "Merged output file: test_merged.edm4hep.root"
echo ""
