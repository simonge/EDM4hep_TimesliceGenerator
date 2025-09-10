#!/bin/bash

# Example usage script for the enhanced timeslice merger
# This demonstrates the new collection zipping and vectorized operations

echo "=== TimeframeGenerator2 Enhanced Usage Examples ==="
echo ""

echo "The enhanced timeslice merger now includes:"
echo "1. PodioCollectionZipReader for coordinated collection processing"
echo "2. Vectorized time offset operations for improved performance"
echo "3. Collection zipping for processing related data together"
echo ""

echo "Example 1: Basic usage (unchanged from original)"
echo "./install/bin/timeslice_merger -n 10 -d 1000.0 input.root"
echo ""

echo "Example 2: Using with multiple sources and bunch crossing"
echo "./install/bin/timeslice_merger --config config.yml -n 50 -d 2000.0"
echo ""

echo "Example 3: With beam physics effects"
echo "./install/bin/timeslice_merger \\"
echo "  --beam-attachment \\"
echo "  --beam-spread 0.5 \\"
echo "  --beam-speed 299792.458 \\"
echo "  -d 10000.0 \\"
echo "  input_events.root"
echo ""

echo "Key improvements in this version:"
echo "- Faster processing through vectorized time operations"
echo "- Better memory efficiency with collection zipping"
echo "- Cleaner code structure while maintaining full compatibility"
echo "- Enhanced error handling and collection type detection"
echo ""

echo "For full documentation, see:"
echo "- README.md for general usage"
echo "- COLLECTION_ZIPPING.md for new features"
echo ""

echo "To build with Podio and EDM4HEP dependencies:"
echo "./build.sh"
echo ""