#!/bin/bash

# TimesliceMerger Usage Example
# This script demonstrates how to use the TimesliceMerger plugin to merge 
# multiple timeslice files output from the TimesliceCreator plugin

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Project directory
PROJECT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="$PROJECT_DIR/build"
INSTALL_DIR="$PROJECT_DIR/install"

echo -e "${BLUE}TimesliceMerger Plugin Usage Example${NC}"
echo "Project directory: $PROJECT_DIR"

# Function to print colored messages
print_status() {
    echo -e "${GREEN}[INFO]${NC} $1"
}

print_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

print_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# Check if JANA is available
check_jana() {
    print_status "Checking JANA installation..."
    if ! command -v jana &> /dev/null; then
        print_error "JANA not found in PATH. Please ensure JANA is installed and in your PATH."
        exit 1
    fi
    
    JANA_VERSION=$(jana --version 2>&1 | head -1 || echo "Unknown")
    print_status "Found JANA: $JANA_VERSION"
}

# Build both plugins
build_plugins() {
    print_status "Building TimesliceCreator and TimesliceMerger plugins..."
    
    # Create build directory if it doesn't exist
    mkdir -p "$BUILD_DIR"
    
    cd "$BUILD_DIR"
    
    # Configure with CMake
    print_status "Configuring with CMake..."
    cmake .. -DCMAKE_INSTALL_PREFIX="$INSTALL_DIR"
    
    # Build
    print_status "Compiling..."
    make -j$(nproc)
    
    # Install
    print_status "Installing to $INSTALL_DIR..."
    make install
    
    cd "$PROJECT_DIR"
}

# Create sample input files for TimesliceCreator
create_sample_events() {
    print_status "Creating sample event files for TimesliceCreator..."
    
    cd "$PROJECT_DIR"
    
    # Create multiple detector files to simulate multiple sources
    cat > create_multi_detector_data.py << 'EOF'
#!/usr/bin/env python3
import ROOT

# Create sample files for different detectors
detector_files = ["detector1_events.root", "detector2_events.root", "detector3_events.root"]

for i, filename in enumerate(detector_files):
    print(f"Creating {filename}")
    f = ROOT.TFile(filename, "RECREATE")
    tree = ROOT.TTree("events", f"Sample events for detector {i+1}")
    
    # Add some basic structure
    event_id = ROOT.std.vector('int')()
    detector_id = ROOT.std.vector('int')()
    tree.Branch("event_id", event_id)
    tree.Branch("detector_id", detector_id)
    
    # Fill with sample data
    for j in range(20):  # More events for better testing
        event_id.clear()
        detector_id.clear()
        event_id.push_back(j)
        detector_id.push_back(i+1)
        tree.Fill()
    
    tree.Write()
    f.Close()

print("Created sample detector event files")
EOF

    if command -v python3 &> /dev/null && python3 -c "import ROOT" &> /dev/null; then
        python3 create_multi_detector_data.py
        rm create_multi_detector_data.py
    else
        print_warning "Python3 with ROOT not available. Creating empty ROOT files..."
        # Create minimal ROOT files
        touch detector1_events.root detector2_events.root detector3_events.root
    fi
}

# Run TimesliceCreator to generate timeslice files
run_timeslice_creator() {
    print_status "Running TimesliceCreator to generate timeslice files..."
    
    # Set up environment for JANA
    export JANA_PLUGIN_PATH="$INSTALL_DIR/lib:$JANA_PLUGIN_PATH"
    export LD_LIBRARY_PATH="$INSTALL_DIR/lib:$LD_LIBRARY_PATH"
    
    # Check if plugin library exists
    PLUGIN_LIB="$INSTALL_DIR/lib/libTimesliceCreator.so"
    if [ ! -f "$PLUGIN_LIB" ]; then
        print_error "TimesliceCreator plugin library not found at $PLUGIN_LIB"
        print_error "Make sure the build was successful."
        return 1
    fi
    
    print_status "Creating timeslices from detector1_events.root..."
    jana \
        -Pplugins=TimesliceCreator \
        -Pjana:nevents=15 \
        -Pwriter:nevents=15 \
        -Plog:global=info \
        detector1_events.root
    
    # Rename output for clarity
    if [ -f "timeslices.root" ]; then
        mv timeslices.root detector1_timeslices.root
    fi
    
    print_status "Creating timeslices from detector2_events.root..."
    jana \
        -Pplugins=TimesliceCreator \
        -Pjana:nevents=15 \
        -Pwriter:nevents=15 \
        -Plog:global=info \
        detector2_events.root
    
    # Rename output for clarity
    if [ -f "timeslices.root" ]; then
        mv timeslices.root detector2_timeslices.root
    fi
    
    print_status "Creating timeslices from detector3_events.root..."
    jana \
        -Pplugins=TimesliceCreator \
        -Pjana:nevents=15 \
        -Pwriter:nevents=15 \
        -Plog:global=info \
        detector3_events.root
    
    # Rename output for clarity
    if [ -f "timeslices.root" ]; then
        mv timeslices.root detector3_timeslices.root
    fi
}

# Run TimesliceMerger to merge timeslice files
run_timeslice_merger() {
    print_status "Running TimesliceMerger to merge timeslice files..."
    
    # Check if plugin library exists
    PLUGIN_LIB="$INSTALL_DIR/lib/libTimesliceMerger.so"
    if [ ! -f "$PLUGIN_LIB" ]; then
        print_error "TimesliceMerger plugin library not found at $PLUGIN_LIB"
        print_error "Make sure the build was successful."
        return 1
    fi
    
    # Check if timeslice files exist
    for ts_file in detector1_timeslices.root detector2_timeslices.root detector3_timeslices.root; do
        if [ ! -f "$ts_file" ]; then
            print_error "Timeslice file $ts_file not found. Run TimesliceCreator first."
            return 1
        fi
    done
    
    print_status "Merging multiple timeslice files..."
    jana \
        -Pplugins=TimesliceMerger \
        -Pjana:nevents=50 \
        -Pwriter:nevents=50 \
        -Pwriter:output_filename=merged_timeslices.root \
        -Pwriter:timeslices_per_merge=3 \
        -Plog:global=info \
        detector1_timeslices.root detector2_timeslices.root detector3_timeslices.root
}

# Verify the output
verify_output() {
    print_status "Verifying merged output..."
    
    if [ -f "merged_timeslices.root" ]; then
        print_status "Success! Merged timeslices written to merged_timeslices.root"
        
        if command -v root &> /dev/null; then
            print_status "Inspecting output file structure..."
            root -l -b -q merged_timeslices.root << 'EOF'
TFile* f = (TFile*)gROOT->GetListOfFiles()->FindObject("merged_timeslices.root");
f->ls();
if (f->GetListOfKeys()->GetSize() > 0) {
    TKey* key = (TKey*)f->GetListOfKeys()->First();
    TTree* tree = (TTree*)f->Get(key->GetName());
    if (tree) {
        std::cout << "Tree: " << key->GetName() << " has " << tree->GetEntries() << " entries" << std::endl;
        tree->Print();
    }
}
.q
EOF
        fi
    else
        print_error "merged_timeslices.root not found. Check for errors in TimesliceMerger execution."
        return 1
    fi
}

# Clean up files
clean_files() {
    print_status "Cleaning up generated files..."
    rm -f detector*_events.root detector*_timeslices.root merged_timeslices.root
    rm -f events.root timeslices.root output.root
}

# Show usage information
show_usage() {
    cat << EOF
Usage: $0 {create_sample|run_creator|run_merger|verify|clean|full_example}

Commands:
  create_sample   - Create sample detector event files for testing
  run_creator     - Run TimesliceCreator on sample files to generate timeslices
  run_merger      - Run TimesliceMerger to merge multiple timeslice files
  verify          - Verify the merged output file
  clean           - Clean up all generated files
  full_example    - Run complete example: build, create samples, run both plugins

Example workflow:
  $0 full_example

Individual steps:
  $0 create_sample        # Create test input files
  $0 run_creator          # Generate timeslices from multiple detector files
  $0 run_merger           # Merge the timeslice files into one output
  $0 verify               # Check the merged output

The TimesliceMerger plugin:
- Reads multiple timeslice files output from TimesliceCreator
- Loads unique frames from each input file
- Merges collections from all sources into a single output
- Preserves original collection names (removes ts_ prefix)
- Sums together hit collections from different sources
EOF
}

# Main execution
main() {
    case "${1:-show_usage}" in
        "create_sample")
            create_sample_events
            ;;
        "run_creator")
            check_jana
            run_timeslice_creator
            ;;
        "run_merger")
            check_jana
            run_timeslice_merger
            ;;
        "verify")
            verify_output
            ;;
        "clean")
            clean_files
            ;;
        "full_example")
            check_jana
            build_plugins
            create_sample_events
            run_timeslice_creator
            run_timeslice_merger
            verify_output
            print_status "Complete example finished successfully!"
            ;;
        *)
            show_usage
            ;;
    esac
}

# Execute main function with all arguments
main "$@"