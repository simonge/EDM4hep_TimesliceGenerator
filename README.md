# Podio-based Timeslice Merger

A standalone application for merging multiple physics events into timeslices using the Podio data model. This tool provides precise control over timing adjustments, bunch crossing logic, and beam attachment without requiring the JANA framework.

## Features

- **Direct Podio I/O**: Uses Podio ROOTReader and ROOTWriter directly, no external framework dependencies
- **Configurable Timing**: Full control over timeslice duration, bunch crossing periods, and time offset generation
- **Beam Physics**: Support for beam attachment with Gaussian smearing and configurable beam parameters
- **Multiple Input Formats**: Handles both event files and pre-existing timeslice files
- **Comprehensive Configuration**: Command line interface with extensive parameter options
- **Error Handling**: Graceful handling of missing collections and invalid parameters

## Prerequisites

- **PODIO library and headers** - Required for data I/O operations
- **EDM4HEP library and headers** - Required for the EDM4HEP data model
- **yaml-cpp library** - Required for configuration file support
- **CMake 3.16 or later** - Required for building
- **C++20 compatible compiler** - Required for compilation

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

#### Configuration and File I/O Options
| Option | Description | Default |
|--------|-------------|---------|
| `--config <file>` | YAML configuration file | (none) |
| `input_files` | Input ROOT files containing events | (required) |
| `-o, --output <file>` | Output ROOT file for timeslices | `merged_timeslices.root` |
| `-n, --nevents <number>` | Maximum number of timeslices to generate | `100` |

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
| `--beam-speed <ns/mm>` | Beam speed for time-of-flight calculations | `299792.458` |
| `--beam-spread <ns>` | Gaussian beam time spread (std deviation) | `0.0` |

#### Generator Configuration
| Option | Description | Default |
|--------|-------------|---------|
| `--status-offset <value>` | Offset added to MCParticle generator status | `0` |

#### Utility Options
| Option | Description |
|--------|-------------|
| `-h, --help` | Display help message and exit |

## Configuration Files

The application supports YAML configuration files that can be used independently or in combination with command-line arguments.

### YAML Configuration Structure

```yaml
# Global merger configuration
output_file: merged_timeslices.root
max_events: 100
time_slice_duration: 200.0
bunch_crossing_period: 10.0
introduce_offsets: true

# Source-specific configuration
sources:
  - input_files:
      - input1.root
      - input2.root
    name: signal
    static_number_of_events: false
    mean_event_frequency: 0.1
    use_bunch_crossing: true
    attach_to_beam: false
    beam_speed: 299792.458
    beam_spread: 0.0
    generator_status_offset: 0
    already_merged: false
    tree_name: events
  - input_files:
      - background.root
    name: background
    static_number_of_events: true
    static_events_per_timeslice: 2
    use_bunch_crossing: true
    generator_status_offset: 1000
    already_merged: false
```

### YAML Configuration Parameters

#### Global Parameters
- `output_file`: Output ROOT file name
- `max_events`: Maximum number of timeslices to generate
- `time_slice_duration`: Duration of each timeslice in nanoseconds
- `bunch_crossing_period`: Bunch crossing period for discretization
- `introduce_offsets`: Whether to introduce random time offsets

#### Source-Specific Parameters
- `input_files`: List of input ROOT files for this source
- `name`: Human-readable name for the source (e.g., "signal", "background")
- `already_merged`: Set to `true` if input files are already timeslice files
- `static_number_of_events`: Use fixed number of events per timeslice
- `static_events_per_timeslice`: Number of events per timeslice (if static mode)
- `mean_event_frequency`: Mean event frequency in events/ns (if not static mode)
- `use_bunch_crossing`: Enable bunch crossing time discretization
- `attach_to_beam`: Enable beam attachment physics
- `beam_speed`: Beam speed in ns/mm
- `beam_spread`: Gaussian time spread for beam smearing
- `generator_status_offset`: Offset to add to MCParticle generator status
- `tree_name`: Name of the input TTree (default: "events")

## Mixed Command Line and Configuration Usage

Command-line arguments override YAML configuration values, allowing flexible usage patterns:

### Pattern 1: Config File with CLI Override
```bash
# Use config.yml but override output file and number of events
./install/bin/timeslice_merger --config config.yml -o different_output.root -n 500
```

### Pattern 2: Config File with Additional Input Files
```bash
# Use config file and add more input files from command line
./install/bin/timeslice_merger --config config.yml additional_input.root more_files.root
```

### Pattern 3: Minimal Config with CLI Parameters
```bash
# Use config file for complex source setup, override timing parameters
./install/bin/timeslice_merger --config multi_source.yml -d 1000.0 -p 25.0
```


## Usage Examples

### Basic Command Line Usage

#### Basic Event Merging

Merge events using default settings:
```bash
./install/bin/timeslice_merger input_events.root
```

#### Static Event Count

Create timeslices with exactly 3 events each:
```bash
./install/bin/timeslice_merger -s -e 3 -n 50 input_events.root
```

#### Bunch Crossing Mode

Enable bunch crossing with 1000 ns period:
```bash
./install/bin/timeslice_merger -b -p 1000.0 -d 20000.0 input_events.root
```

#### Full Beam Physics

Enable beam attachment with realistic parameters:
```bash
./install/bin/timeslice_merger \
  --beam-attachment \
  --beam-spread 0.5 \
  --beam-speed 299792.458 \
  -d 10000.0 \
  input_events.root
```

#### High Statistics Processing

Process large number of events with custom output:
```bash
./install/bin/timeslice_merger \
  -n 10000 \
  -o large_timeslices.root \
  --use-bunch-crossing \
  --bunch-period 2000.0 \
  input1.root input2.root input3.root
```

### YAML Configuration Usage

#### Using Configuration Files

Use a YAML configuration file for complex setups:
```bash
./install/bin/timeslice_merger --config config.yml
```

#### Mixed Configuration and Command Line

Use configuration file but override specific parameters:
```bash
# Override output file and number of timeslices
./install/bin/timeslice_merger --config config.yml -o custom_output.root -n 500

# Add additional input files to those specified in config
./install/bin/timeslice_merger --config config.yml extra_input.root

# Override timing parameters
./install/bin/timeslice_merger --config config.yml -d 5000.0 -p 500.0
```

#### Working with Pre-merged Timeslices

Process already-merged timeslice files:
```bash
# Configuration file with already_merged: true
./install/bin/timeslice_merger --config config_continue.yml
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

### Architecture Overview

The application has been recently refactored from a Podio Frame-based approach to a more efficient vector-based ROOT processing system:

- **Direct ROOT I/O**: Uses ROOT TChain/TTree directly instead of Podio Frame readers
- **Vector-Based Processing**: Works with `std::vector<edm4hep::*Data>` types directly
- **Object-Oriented Design**: Clean separation between data sources and merged collections
- **Memory Efficient**: Streams data without loading entire files into memory

### Core Components

#### DataSource Class
Encapsulates input file management and data reading:
- Handles multiple input files through ROOT TChain
- Manages branch pointers for different collection types
- Provides event processing and time offset calculation methods

#### MergedCollections Structure  
Organizes all merged output data:
- Event and particle collections (MCParticle, EventHeader)
- Hit collections (SimTrackerHit, SimCalorimeterHit, CaloHitContribution)
- Reference collections maintaining object relationships
- Global parameter (GP) branches for metadata

#### StandaloneTimesliceMerger
Main orchestration class:
- Initializes and manages multiple DataSource instances
- Coordinates timeslice generation and event merging
- Handles output file creation and metadata preservation

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
- **Streaming**: Uses ROOT streaming for memory-efficient processing
- **Monitoring**: Use system tools like `htop` or `top` for resource monitoring

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
2. **Memory Usage**: Monitor with system tools (htop, top)
3. **Output Size**: Verify timeslice parameters are reasonable

## Development and Contributing

### Code Structure
- `src/StandaloneTimesliceMerger.cc`: Core merging logic and orchestration
- `src/DataSource.cc`: Input file management and data reading  
- `src/timeslice_merger_main.cc`: Command line interface and configuration parsing
- `include/StandaloneTimesliceMerger.h`: Main API and data structures
- `include/DataSource.h`: Input data source abstraction
- `include/StandaloneMergerConfig.h`: Configuration structures

### Testing
```bash
# Build the project
./build.sh

# Run basic configuration tests
./install/bin/test_standalone

# Test with sample data
./install/bin/timeslice_merger --config configs/config.yml
```

### Configuration Files
Example configuration files are provided in the `configs/` directory:
- `config.yml`: Basic multi-source configuration
- `config_continue.yml`: Working with pre-merged timeslice files

## License

This software is provided under the terms specified in the LICENSE file found in the top-level directory.

## Support

For questions, bug reports, or feature requests, please refer to the project's issue tracker or contact the development team.