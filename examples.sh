#!/bin/bash

# Example usage script for the standalone timeslice merger
# This demonstrates various usage patterns

echo "=== Standalone Timeslice Merger Examples ==="

MERGER="./install_standalone/bin/timeslice_merger"

# Check if the merger is built
if [ ! -f "$MERGER" ]; then
    echo "Merger not found. Please build first with: ./build_standalone.sh"
    exit 1
fi

echo ""
echo "1. Basic usage with help:"
$MERGER -h

echo ""
echo "=========================================="
echo ""
echo "2. Example command lines (without actual files):"

echo ""
echo "Basic merging with static events:"
echo "$MERGER -s -e 2 -n 10 -o output1.root input.root"

echo ""
echo "Using bunch crossing logic:"
echo "$MERGER -b -p 1000.0 -d 10000.0 -o output2.root input1.root input2.root"

echo ""
echo "With beam attachment and all options:"
echo "$MERGER \\"
echo "  --beam-attachment \\"
echo "  --beam-spread 0.5 \\"
echo "  --beam-speed 299792.458 \\"
echo "  --status-offset 1000 \\"
echo "  --use-bunch-crossing \\"
echo "  --bunch-period 1000.0 \\"
echo "  --duration 20000.0 \\"
echo "  --frequency 0.001 \\"
echo "  --nevents 50 \\"
echo "  --static-events \\"
echo "  --events-per-slice 3 \\"
echo "  --output merged_full.root \\"
echo "  input1.root input2.root input3.root"

echo ""
echo "Poisson-distributed events (dynamic):"
echo "$MERGER -f 0.005 -d 5000.0 -n 20 input.root"

echo ""
echo "=========================================="
echo ""
echo "To use with real files, replace 'input.root' with actual ROOT files containing EDM4HEP data."
echo "The merger will process MCParticles, SimTrackerHits, and SimCalorimeterHits from the input files."