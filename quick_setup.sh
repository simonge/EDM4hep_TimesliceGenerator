#!/bin/bash

# Quick setup script for EDM4HEPTimesliceExample
# This script provides the minimal commands to build and run

echo "=== EDM4HEPTimesliceExample Quick Setup ==="

# Build the plugin
echo "1. Building plugin..."
mkdir -p build
cd build
cmake .. -DCMAKE_INSTALL_PREFIX=../install
make -j$(nproc)
make install
cd ..

# Set environment variables
export JANA_PLUGIN_PATH="$(pwd)/install/lib/EDM4HEPTimesliceExample/plugins/:$JANA_PLUGIN_PATH"
export LD_LIBRARY_PATH="$(pwd)/install/lib:$LD_LIBRARY_PATH"

echo "2. Environment set:"
echo "   JANA_PLUGIN_PATH=$JANA_PLUGIN_PATH"
echo "   LD_LIBRARY_PATH=$LD_LIBRARY_PATH"

# Create a minimal test file if none exists
if [ ! -f "events.root" ]; then
    echo "3. Creating minimal test file..."
    touch events.root
fi

echo "4. Plugin built and ready!"
echo ""
echo "To run the plugin:"
echo "   jana -Pplugins=EDM4HEPTimesliceExample -Pjana:nevents=10 events.root"
echo ""
echo "To run with debug info:"
echo "   jana -Pplugins=EDM4HEPTimesliceExample -Pjana:nevents=10 -Pjana:debug_plugin_loading=1 -Plog:global=info events.root"
