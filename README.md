# Standalone TimesliceExample Plugin

This is a standalone version of the JANA2 TimesliceExample plugin that can be compiled independently of the JANA2 source tree.

## Prerequisites

- JANA2 installed on your system
- PODIO library and headers
- CMake 3.16 or later
- C++17 compatible compiler

## Building

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
