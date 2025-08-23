# EDM4HEPTimesliceExample Plugin

This is a standalone JANA2 TimesliceExample plugin that has been updated to use EDM4HEP collections instead of the original PodioDatamodel collections. The plugin name has been changed to "EDM4HEPTimesliceExample" to avoid conflicts with JANA's built-in TimesliceExample plugin.

## What's Changed

The plugin has been converted to use EDM4HEP collections:
- `ExampleHit` → `edm4hep::CalorimeterHit`
- `ExampleCluster` → `edm4hep::Cluster`
- `EventInfo`/`TimesliceInfo` → `edm4hep::EventHeader`

## Prerequisites

- JANA2 installed and available in PATH
- EDM4HEP library and headers
- PODIO library and headers
- CMake 3.16 or later
- C++17 compatible compiler

## Quick Start

### Option 1: Use the automated script
```bash
# Build and run everything automatically
./run_example.sh all

# Or step by step:
./run_example.sh build     # Build the plugin
./run_example.sh sample    # Create sample input files
./run_example.sh run       # Run with default settings
```

### Option 2: Use the quick setup
```bash
# Minimal setup and build
./quick_setup.sh

# Then run manually
jana -Pplugins=EDM4HEPTimesliceExample -Pjana:nevents=10 events.root
```

### Option 3: Manual build and run
```bash
# 1. Build
mkdir -p build && cd build
cmake .. -DCMAKE_INSTALL_PREFIX=../install
make -j$(nproc)
make install
cd ..

# 2. Set environment
export JANA_PLUGIN_PATH="$(pwd)/install/lib:$JANA_PLUGIN_PATH"
export LD_LIBRARY_PATH="$(pwd)/install/lib:$LD_LIBRARY_PATH"

# 3. Run
jana -Pplugins=EDM4HEPTimesliceExample -Pjana:nevents=10 your_file.root
```

## Script Usage

The `run_example.sh` script provides several commands:

```bash
./run_example.sh check                    # Check JANA installation
./run_example.sh build                    # Build and install plugin
./run_example.sh sample                   # Create sample input files
./run_example.sh run [file] [nevents]     # Run the plugin
./run_example.sh clean                    # Clean build artifacts
./run_example.sh all                      # Build, create samples, and run
```

## Running Examples

### With Physics Events
```bash
# Run with events (physics events)
./run_example.sh run events.root 10
```

### With Timeslices
```bash
# Run with timeslices (will unfold to physics events)
./run_example.sh run timeslices.root 5
```

### With Debug Information
```bash
# Run with detailed debug output
jana -Pplugins=EDM4HEPTimesliceExample \
     -Pjana:nevents=10 \
     -Pjana:debug_plugin_loading=1 \
     -Plog:global=debug \
     your_file.root
```

## Plugin Components

The plugin includes several factories and processors:

1. **MyFileReader**: Generates EDM4HEP CalorimeterHits and EventHeaders
2. **MyProtoclusterFactory**: Creates Clusters from CalorimeterHits
3. **MyClusterFactory**: Processes Clusters (adds energy offset)
4. **MyTimesliceSplitter**: Unfolds timeslices into physics events
5. **MyFileWriter**: Writes output to ROOT files

## Environment Variables

The plugin uses these environment variables:

- `JANA_PLUGIN_PATH`: Path to plugin libraries
- `LD_LIBRARY_PATH`: Path for dynamic library loading

Both are automatically set by the provided scripts.

## Troubleshooting

### Plugin Not Found
If you get "Plugin not found" errors:
```bash
# Check plugin path
echo $JANA_PLUGIN_PATH
ls -la install/lib/

# Verify plugin library exists
file install/lib/libEDM4HEPTimesliceExample.so
```

### EDM4HEP Not Found
If you get EDM4HEP-related compile errors:
```bash
# Check if EDM4HEP is available
pkg-config --exists EDM4HEP && echo "EDM4HEP found" || echo "EDM4HEP not found"

# Or check cmake
cmake --find-package -DNAME=EDM4HEP -DCOMPILER_ID=GNU -DLANGUAGE=CXX -DMODE=EXIST
```

### Missing Dependencies
Make sure all dependencies are installed:
- JANA2
- EDM4HEP
- PODIO
- ROOT (with Python bindings for sample data creation)

## Output

The plugin will:
1. Read input events/timeslices
2. Generate CalorimeterHits with EDM4HEP format
3. Create Clusters from hits
4. Write output to `output.root`
5. Display detailed logging information

## File Structure

```
EDM4HEPTimesliceExample/
├── CMakeLists.txt              # Build configuration
├── run_example.sh              # Main run script
├── quick_setup.sh              # Quick setup script
├── README_EDM4HEP.md          # This file
├── include/                    # Header files
│   ├── CollectionTabulators.h  # EDM4HEP collection display utilities
│   ├── MyClusterFactory.h      # Cluster processing factory
│   ├── MyFileReader.h          # Event source (generates EDM4HEP data)
│   ├── MyFileReaderGenerator.h # Source generator
│   ├── MyFileWriter.h          # Output processor
│   ├── MyProtoclusterFactory.h # Hit-to-cluster factory
│   └── MyTimesliceSplitter.h   # Timeslice unfolder
└── src/                        # Source files
    ├── CollectionTabulators.cc  # Implementation
    └── TimesliceExample.cc      # Main plugin registration
```
