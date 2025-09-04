# Standalone Timeslice Merger

This is a standalone application that replicates the merging logic of the JANA-based `MyTimesliceBuilder` but uses Podio directly instead of the JANA framework. It merges multiple events into timeslices with the same timing adjustments and configuration parameters as the original JANA implementation.

## Features

- **Direct Podio I/O**: Uses Podio ROOTReader and ROOTWriter directly, no JANA dependency
- **Same Merging Logic**: Implements the exact same time offset calculations, bunch crossing logic, and beam attachment as the original `MyTimesliceBuilder`
- **Configurable Parameters**: Supports all the same configuration options as the JANA version
- **Command Line Interface**: Easy-to-use command line tool with comprehensive options

## Building

### Prerequisites

- PODIO library and headers
- EDM4HEP library and headers  
- CMake 3.16 or later
- C++20 compatible compiler

### Build Instructions

1. Use the provided build script:
```bash
./build_standalone.sh
```

Or manually:

2. Create build directory:
```bash
mkdir build_standalone && cd build_standalone
```

3. Configure with CMake:
```bash
cmake .. -f ../CMakeLists_standalone.txt
```

4. Build:
```bash
make -j$(nproc)
make install
```

## Usage

### Basic Usage

```bash
./install_standalone/bin/timeslice_merger [options] input_file1 [input_file2 ...]
```

### Command Line Options

- `-o, --output FILE` - Output file name (default: merged_timeslices.root)
- `-n, --nevents N` - Maximum number of timeslices to generate (default: 100)
- `-d, --duration TIME` - Timeslice duration in ns (default: 20.0)
- `-f, --frequency FREQ` - Mean event frequency (events/ns) (default: 1.0)
- `-p, --bunch-period PERIOD` - Bunch crossing period in ns (default: 10.0)
- `-b, --use-bunch-crossing` - Enable bunch crossing logic
- `-s, --static-events` - Use static number of events per timeslice
- `-e, --events-per-slice N` - Static events per timeslice (default: 1)
- `--beam-attachment` - Enable beam attachment with Gaussian smearing
- `--beam-speed SPEED` - Beam speed in ns/mm (default: 299792.458)
- `--beam-spread SPREAD` - Beam spread for Gaussian smearing (default: 0.0)
- `--status-offset OFFSET` - Generator status offset (default: 0)
- `-h, --help` - Show help message

### Examples

1. **Basic merging with static events**:
```bash
./install_standalone/bin/timeslice_merger -s -e 2 -n 10 input.root
```

2. **Using bunch crossing logic**:
```bash
./install_standalone/bin/timeslice_merger -b -p 1000.0 -d 10000.0 input1.root input2.root
```

3. **With beam attachment and custom output**:
```bash
./install_standalone/bin/timeslice_merger \
  --beam-attachment \
  --beam-spread 0.5 \
  --status-offset 1000 \
  -o merged_output.root \
  input.root
```

## Configuration Equivalence with JANA Version

The standalone merger supports the same configuration parameters as the JANA plugin:

| JANA Parameter | Standalone Option | Description |
|----------------|------------------|-------------|
| `timeslice:duration` | `-d, --duration` | Timeslice duration in ns |
| `timeslice:bunch_crossing_period` | `-p, --bunch-period` | Bunch crossing period in ns |
| `timeslice:use_bunch_crossing` | `-b, --use-bunch-crossing` | Enable bunch crossing logic |
| `timeslice:static_number_of_events` | `-s, --static-events` | Use static events per timeslice |
| `timeslice:static_events_per_timeslice` | `-e, --events-per-slice` | Static events per timeslice |
| `timeslice:mean_event_frequency` | `-f, --frequency` | Mean event frequency |
| `timeslice:attach_to_beam` | `--beam-attachment` | Enable beam attachment |
| `timeslice:beam_speed` | `--beam-speed` | Beam speed |
| `timeslice:beam_spread` | `--beam-spread` | Beam spread |
| `timeslice:generator_status_offset` | `--status-offset` | Generator status offset |
| `writer:nevents` | `-n, --nevents` | Max number of events |
| `output_file` | `-o, --output` | Output file name |

## Implementation Details

### Core Merging Logic

The standalone merger implements the same merging algorithm as `MyTimesliceBuilder::Unfold()`:

1. **Event Accumulation**: Collects events until the required number is reached
2. **Time Offset Generation**: Uses the same random distributions for time offsets
3. **Bunch Crossing**: Applies the same floor-based bunch crossing logic
4. **Beam Attachment**: Implements the same Gaussian smearing and position-based offsets
5. **Collection Processing**: Handles MCParticles, SimTrackerHits, and SimCalorimeterHits with proper particle reference updates

### Supported Collection Types

- **MCParticle collections**: Cloned with time and status updates
- **SimTrackerHit collections**: Cloned with time updates and particle reference fixes
- **SimCalorimeterHit collections**: Cloned with time updates and contribution processing
- **CaloHitContribution collections**: Cloned with time and particle reference updates

### Differences from JANA Version

- **No JANA Framework**: Uses pure Podio API for I/O operations
- **Simplified Architecture**: Direct file processing without event processing framework
- **Command Line Configuration**: Uses command line arguments instead of JANA parameters
- **Single-threaded**: Sequential processing (JANA version may use multiple threads)

## Output Format

The output file contains:
- **Tree name**: `timeslices`  
- **Collections**: Same as input but with time adjustments applied
- **Format**: Standard Podio ROOT format

## Error Handling

- **Missing Collections**: Gracefully skips missing optional collections
- **File Errors**: Reports file access issues with clear error messages  
- **Invalid Parameters**: Validates command line arguments and provides helpful error messages