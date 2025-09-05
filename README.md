# Standalone TimesliceExample Plugin

This repository contains two implementations for merging events into timeslices:

1. **JANA-based TimesliceCreator Plugin** - Uses the JANA event processing framework
2. **Standalone Timeslice Merger** - Direct Podio-based implementation without JANA dependencies

Both implementations provide identical merging logic and configuration parameters.

## Which Version to Use?

- **Use the JANA plugin** if you're already using JANA or need integration with other JANA plugins
- **Use the standalone merger** if you want minimal dependencies or simpler deployment

See [COMPARISON.md](COMPARISON.md) for detailed comparison and migration guide.

## JANA-based TimesliceCreator Plugin

This is the original JANA2 TimesliceCreator plugin that can be compiled independently of the JANA2 source tree.

## Prerequisites

- JANA2 installed on your system
- PODIO library and headers
- CMake 3.16 or later
- C++17 compatible compiler

## Standalone Timeslice Merger

For the standalone Podio-based version:

```bash
./build_standalone.sh
./install_standalone/bin/timeslice_merger [options] input_file.root
```

See [README_Standalone.md](README_Standalone.md) for detailed standalone usage instructions.

## JANA Plugin Building

1. Create a build directory:
```bash
mkdir build && cd build
```

2. Configure with CMake:
```bash
cmake ..
```

3. Build the plugin:
```bash
make -j$(nproc)
```

4. Install the plugin:
```bash
make install
```

## Running

After installation, you can run the plugin with:

```bash
jana -Pplugins=TimesliceExample -Pjana:nevents=10 your_input_file.root
```

## Project Structure

```
TimesliceExample/
├── CMakeLists.txt          # Main build configuration
├── README.md              # This file
├── include/               # Header files
│   ├── CollectionTabulators.h
│   ├── MyClusterFactory.h
│   ├── MyFileReader.h
│   ├── MyFileReaderGenerator.h
│   ├── MyFileWriter.h
│   ├── MyProtoclusterFactory.h
│   └── MyTimesliceSplitter.h
└── src/                   # Source files
    ├── TimesliceExample.cc
    └── CollectionTabulators.cc
```

## Key Features

- **File Reading**: Automatically detects timeslice vs event files
- **Timeslice Splitting**: Converts timeslices into physics events
- **Clustering**: Implements protoclustering and clustering algorithms
- **File Writing**: Saves processed data to ROOT files
- **PODIO Integration**: Uses PODIO for data I/O
