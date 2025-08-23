#!/bin/bash

# EDM4HEPTimesliceExample Run Script
# This script builds, installs, and runs the EDM4HEPTimesliceExample plugin with proper JANA paths

set -e  # Exit on any error

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

echo -e "${BLUE}EDM4HEPTimesliceExample Plugin Setup and Run Script${NC}"
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

# Build the plugin
build_plugin() {
    print_status "Building EDM4HEPTimesliceExample plugin..."
    
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

# Create sample input files
create_sample_files() {
    print_status "Creating sample input files..."
    
    cd "$PROJECT_DIR"
    
    # Create a simple events.root file for testing
    cat > create_sample_data.py << 'EOF'
#!/usr/bin/env python3
import ROOT

# Create a simple ROOT file with some dummy data for testing
f = ROOT.TFile("events.root", "RECREATE")
tree = ROOT.TTree("events", "Sample events")

# Add some basic structure
event_id = ROOT.std.vector('int')()
tree.Branch("event_id", event_id)

# Fill with sample data
for i in range(10):
    event_id.clear()
    event_id.push_back(i)
    tree.Fill()

tree.Write()
f.Close()

# Create timeslices.root as well
f2 = ROOT.TFile("timeslices.root", "RECREATE")
tree2 = ROOT.TTree("timeslices", "Sample timeslices")

timeslice_id = ROOT.std.vector('int')()
tree2.Branch("timeslice_id", timeslice_id)

for i in range(5):
    timeslice_id.clear()
    timeslice_id.push_back(i)
    tree2.Fill()

tree2.Write()
f2.Close()

print("Created events.root and timeslices.root")
EOF

    if command -v python3 &> /dev/null && python3 -c "import ROOT" &> /dev/null; then
        python3 create_sample_data.py
        rm create_sample_data.py
    else
        print_warning "Python3 with ROOT not available. Creating empty ROOT files..."
        # Create minimal ROOT files
        touch events.root timeslices.root
    fi
}

# Run the plugin
run_plugin() {
    local input_file="$1"
    local nevents="${2:-10}"
    
    print_status "Running EDM4HEPTimesliceExample plugin..."
    
    # Set up environment for JANA
    export JANA_PLUGIN_PATH="$INSTALL_DIR/lib:$JANA_PLUGIN_PATH"
    export LD_LIBRARY_PATH="$INSTALL_DIR/lib:$LD_LIBRARY_PATH"
    
    # Check if plugin library exists
    PLUGIN_LIB="$INSTALL_DIR/lib/libEDM4HEPTimesliceExample.so"
    if [ ! -f "$PLUGIN_LIB" ]; then
        print_error "Plugin library not found at $PLUGIN_LIB"
        print_error "Make sure the build was successful."
        return 1
    fi
    
    print_status "Plugin library found: $PLUGIN_LIB"
    print_status "JANA_PLUGIN_PATH: $JANA_PLUGIN_PATH"
    
    # Run JANA with the plugin
    print_status "Executing: jana -Pplugins=EDM4HEPTimesliceExample -Pjana:nevents=$nevents $input_file"
    
    jana \
        -Pplugins=EDM4HEPTimesliceExample \
        -Pjana:nevents=$nevents \
        -Pjana:debug_plugin_loading=1 \
        -Plog:global=info \
        "$input_file"
}

# Main execution
main() {
    case "${1:-build}" in
        "check")
            check_jana
            ;;
        "build")
            check_jana
            build_plugin
            print_status "Build complete! Plugin installed to: $INSTALL_DIR"
            ;;
        "sample")
            create_sample_files
            ;;
        "run")
            input_file="${2:-events.root}"
            nevents="${3:-10}"
            
            if [ ! -f "$input_file" ]; then
                print_warning "Input file '$input_file' not found. Creating sample files..."
                create_sample_files
                input_file="events.root"
            fi
            
            run_plugin "$input_file" "$nevents"
            ;;
        "clean")
            print_status "Cleaning build and install directories..."
            rm -rf "$BUILD_DIR" "$INSTALL_DIR"
            rm -f events.root timeslices.root output.root
            ;;
        "all")
            check_jana
            build_plugin
            create_sample_files
            run_plugin "events.root" 10
            ;;
        *)
            echo "Usage: $0 {check|build|sample|run [input_file] [nevents]|clean|all}"
            echo ""
            echo "Commands:"
            echo "  check   - Check if JANA is available"
            echo "  build   - Build and install the plugin"
            echo "  sample  - Create sample input files"
            echo "  run     - Run the plugin (default: events.root, 10 events)"
            echo "  clean   - Clean build artifacts"
            echo "  all     - Do everything: build, create samples, and run"
            echo ""
            echo "Examples:"
            echo "  $0 build                    # Build the plugin"
            echo "  $0 run events.root 20       # Run with events.root, 20 events"
            echo "  $0 run timeslices.root      # Run with timeslices.root"
            echo "  $0 all                      # Build and run with samples"
            exit 1
            ;;
    esac
}

# Execute main function with all arguments
main "$@"
