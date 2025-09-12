# Podio-based Timeslice Merger (ROOT Dataframe Edition)

A standalone application for merging multiple physics events into timeslices using a ROOT dataframe approach instead of direct Podio libraries, while maintaining full Podio format compatibility. This tool provides precise control over timing adjustments, bunch crossing logic, and beam attachment without requiring the JANA framework or direct Podio dependencies.

## Key Changes in This Version

🔄 **NEW: ROOT Dataframe Approach**
- Uses ROOT dataframe-like collections instead of direct Podio calls
- Helper functions handle timing updates and reference index offsets
- Maintains full Podio format compatibility in output
- Modular design allows easy integration with actual ROOT dataframes when available
- No direct Podio dependencies required for build

## Architecture

The new implementation uses a **dataframe-based approach** with the following key components:

- **DataFrameCollection<T>**: Template wrapper that acts like ROOT dataframes
- **Helper Functions**: Specialized functions for timing updates and reference handling
- **Index Offset Management**: Proper handling of collection references when merging
- **Podio Format Compatibility**: Output maintains the expected Podio structure

## Features

- **Direct ROOT Integration**: Uses ROOT-style dataframes and helper functions instead of Podio libraries directly
- **Configurable Timing**: Full control over timeslice duration, bunch crossing periods, and time offset generation  
- **Reference Management**: Automatic index offset handling when merging collections
- **Beam Physics**: Support for beam attachment with Gaussian smearing and configurable beam parameters
- **Multiple Input Formats**: Handles both event files and pre-existing timeslice files
- **Comprehensive Configuration**: Command line interface with extensive parameter options
- **Error Handling**: Graceful handling of missing collections and invalid parameters

## Prerequisites

- **C++20 compatible compiler** - Required for compilation
- **yaml-cpp library** - Required for configuration file support
- **CMake 3.16 or later** - Required for building
- **ROOT libraries** (optional) - Can be integrated for full ROOT dataframe support

*Note: This version does not require Podio or EDM4HEP libraries directly, making it easier to build and deploy.*

## Building

### Quick Build

Use the provided build script for automatic configuration and compilation:

```bash
./build.sh
```

### Manual Build

For more control over the build process:

1. Create and enter build directory:
```bash
mkdir build && cd build
```

2. Configure with CMake:
```bash
cmake .. -DCMAKE_INSTALL_PREFIX=../install
```

3. Build and install:
```bash
make -j$(nproc)
make install
```

## Usage

### Basic Usage

```bash
./install/bin/timeslice_merger [options] input_file1 [input_file2 ...]
```

### Command Line Options

#### File I/O Options
| Option | Description | Default |
|--------|-------------|---------|
| `input_files` | Input ROOT files containing events | (required) |
| `-o, --output <file>` | Output ROOT file for timeslices | `merged_timeslices.root` |
| `-n, --nevents <number>` | Maximum number of events to process | `100` |

#### Timeslice Configuration
| Option | Description | Default |
|--------|-------------|---------|
| `-d, --duration <ns>` | Timeslice duration in nanoseconds | `20.0` |
| `-s, --static-events` | Use static number of events per timeslice | `false` |
| `-e, --events-per-slice <number>` | Static events per timeslice (requires `-s`) | `1` |
| `-f, --frequency <rate>` | Mean event frequency (events/ns) | `1.0` |

#### Bunch Crossing Options
| Option | Description | Default |
|--------|-------------|---------|
| `-b, --use-bunch-crossing` | Enable bunch crossing time discretization | `false` |
| `-p, --bunch-period <ns>` | Bunch crossing period in nanoseconds | `10.0` |

#### Beam Physics Options
| Option | Description | Default |
|--------|-------------|---------|
| `--beam-attachment` | Enable beam attachment for particles | `false` |
| `--beam-speed <mm/ns>` | Beam speed for time-of-flight calculations | `299792.458` |
| `--beam-spread <ns>` | Gaussian beam time spread (std deviation) | `0.0` |
| `--beam-angle <rad>` | Beam angle (currently unused) | `0.0` |

#### Generator Configuration
| Option | Description | Default |
|--------|-------------|---------|
| `--generator-status-offset <value>` | Offset added to MCParticle generator status | `0` |

#### Utility Options
| Option | Description |
|--------|-------------|
| `-h, --help` | Display help message and exit |
| `-v, --verbose` | Enable verbose output for debugging |

## Configuration Examples

### Basic Event Merging

Merge events using default settings:
```bash
./install/bin/timeslice_merger input_events.root
```

### Static Event Count

Create timeslices with exactly 3 events each:
```bash
./install/bin/timeslice_merger -s -e 3 -n 50 input_events.root
```

### Bunch Crossing Mode

Enable bunch crossing with 1000 ns period:
```bash
./install/bin/timeslice_merger -b -p 1000.0 -d 20000.0 input_events.root
```

### Full Beam Physics

Enable beam attachment with realistic parameters:
```bash
./install/bin/timeslice_merger \
  --beam-attachment \
  --beam-spread 0.5 \
  --beam-speed 299792.458 \
  -d 10000.0 \
  input_events.root
```

### High Statistics Processing

Process large number of events with custom output:
```bash
./install/bin/timeslice_merger \
  -n 10000 \
  -o large_timeslices.root \
  --use-bunch-crossing \
  --bunch-period 2000.0 \
  input1.root input2.root input3.root
```

## Configuration Parameters Details

### Timeslice Duration (`-d, --duration`)
- **Units**: nanoseconds
- **Purpose**: Sets the total time span of each output timeslice
- **Usage**: Larger values create longer timeslices with more temporal coverage
- **Typical Values**: 10000-50000 ns for physics applications

### Event Accumulation Modes

#### Static Mode (`-s, --static-events`)
- Uses a fixed number of events per timeslice (set by `-e`)
- Guarantees consistent event count across all timeslices
- Useful for systematic studies and detector calibration

#### Poisson Mode (default)
- Uses Poisson distribution based on mean frequency (`-f`)
- More realistic for simulating random event timing
- Better for physics analysis with realistic trigger rates

### Bunch Crossing Logic (`-b, --use-bunch-crossing`)
- **Purpose**: Discretizes event timing to bunch crossing boundaries
- **Implementation**: Uses `floor(time / bunch_period) * bunch_period`
- **Physics**: Simulates accelerator bunch structure
- **Use Case**: Required for realistic collider simulations

### Beam Attachment (`--beam-attachment`)
- **Purpose**: Adjusts particle timing based on time-of-flight from interaction vertex
- **Physics**: Accounts for finite beam propagation speed
- **Implementation**: Adds Gaussian-smeared time-of-flight corrections
- **Required For**: Realistic detector timing simulations

## Output Format

The output ROOT file contains:
- **Tree Name**: `timeslices`
- **Collections**: All input collections with time adjustments applied
- **Format**: Standard Podio ROOT format compatible with EDM4HEP readers
- **Additional Data**: Preserved metadata and collection relationships

### Supported Collection Types

The merger processes the following EDM4HEP collection types:
- **MCParticle**: Time and generator status updates
- **SimTrackerHit**: Time updates and particle reference mapping
- **SimCalorimeterHit**: Time updates and contribution processing
- **CaloHitContribution**: Time updates with proper particle references

## Technical Implementation

### Time Offset Generation
1. **Random Distribution**: Uniform random offsets within timeslice duration
2. **Bunch Crossing**: Optional discretization to bunch boundaries
3. **Beam Physics**: Time-of-flight corrections for realistic timing

### Particle Reference Mapping
- Maintains proper relationships between particles and detector hits
- Updates all cross-references when merging events
- Preserves parent-daughter relationships in particle hierarchies

### Memory Management
- Efficient streaming of large files without loading entire datasets
- Automatic cleanup of temporary objects
- Optimized for processing large-scale simulation outputs

## Error Handling

### Common Issues and Solutions

#### Missing Collections
- **Behavior**: Gracefully skips missing optional collections
- **Logging**: Reports which collections were not found
- **Impact**: Processing continues with available data

#### File Access Errors
- **Detection**: Automatic validation of input/output file accessibility
- **Reporting**: Clear error messages with file paths
- **Recovery**: Fails fast with descriptive error information

#### Invalid Parameters
- **Validation**: Command line arguments checked at startup
- **Examples**: Negative durations, invalid file paths
- **Response**: Helpful error messages with correct usage examples

#### Memory Issues
- **Large Files**: Automatic detection of memory constraints
- **Streaming**: Uses Podio streaming for memory-efficient processing
- **Monitoring**: Optional verbose mode for resource usage tracking

## Performance Considerations

### File Size Scaling
- **Input**: Linear scaling with number of input events
- **Output**: Depends on timeslice duration and event rate
- **Memory**: Constant memory usage regardless of file size

### Processing Speed
- **Typical Rate**: 1000-10000 events/second depending on complexity
- **Bottlenecks**: I/O operations and random number generation
- **Optimization**: Use SSDs and optimize bunch crossing parameters

## Troubleshooting

### Build Issues
1. **Missing Dependencies**: Ensure PODIO and EDM4HEP are installed and findable
2. **Compiler Errors**: Verify C++20 support in your compiler
3. **CMake Issues**: Check CMake version (3.16+ required)

### Runtime Issues
1. **File Not Found**: Verify input file paths and permissions
2. **Segmentation Faults**: Check input file format compatibility
3. **Memory Errors**: Reduce batch size or enable streaming mode

### Performance Issues
1. **Slow Processing**: Check disk I/O speeds and file system type
2. **Memory Usage**: Monitor with verbose mode enabled
3. **Output Size**: Verify timeslice parameters are reasonable

## Development and Contributing

### Code Structure
- `src/StandaloneTimesliceMerger.cc`: Core merging logic
- `src/timeslice_merger_main.cc`: Command line interface
- `include/StandaloneTimesliceMerger.h`: Public API
- `include/StandaloneMergerConfig.h`: Configuration structures

### Testing
```bash
# Build with test support
cmake .. -DBUILD_TESTING=ON
make -j$(nproc)

# Run tests
./src/test_standalone
```

### Debugging
Enable verbose mode for detailed logging:
```bash
./install/bin/timeslice_merger -v [other options] input.root
```

## License

This software is provided under the terms specified in the LICENSE file found in the top-level directory.

## Support

For questions, bug reports, or feature requests, please refer to the project's issue tracker or contact the development team.