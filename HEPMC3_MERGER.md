# HepMC3 Timeslice Merger

## Overview

The HepMC3 Timeslice Merger (`hepmc3_timeslice_merger`) is an alternative to the EDM4hep merger that works with HepMC3 format event files. It is based on the [EPIC HEPMC_Merger](https://github.com/eic/HEPMC_Merger) implementation but uses the same configuration structure as the EDM4hep merger for consistency.

## Key Features

- **Same Configuration Format**: Uses identical YAML and command-line configuration as the EDM4hep merger
- **Multiple Input Sources**: Support for signal and multiple background sources
- **Flexible Event Placement**: 
  - Static: Fixed number of events per timeslice
  - Frequency-based: Poisson-distributed events based on rate
  - Weighted: Event selection based on weights in the input file
- **Generator Status Offset**: Automatic shifting of particle status codes to distinguish sources
- **Bunch Crossing Support**: Optional time discretization to bunch boundaries
- **File Cycling**: Can repeat sources when end-of-file is reached
- **Multiple Output Formats**: HepMC3 ROOT tree format or ASCII format

## Differences from EPIC HEPMC_Merger

### Configuration

The original HEPMC_Merger uses a different argument structure:

```bash
# Original HEPMC_Merger style
./SignalBackgroundMerger --signalFile signal.root --signalFreq 0 \
    --bgFile bg1.root 2000 0 1000 \
    --bgFile bg2.root 20 0 2000
```

This implementation uses the same configuration as the EDM4hep merger:

```bash
# New unified configuration style
./hepmc3_timeslice_merger \
    --source:signal:input_files signal.root \
    --source:signal:frequency 0 \
    --source:bg1:input_files bg1.root \
    --source:bg1:frequency 2.0 \
    --source:bg1:status_offset 1000 \
    --source:bg2:input_files bg2.root \
    --source:bg2:frequency 0.02 \
    --source:bg2:status_offset 2000
```

Or using YAML:

```yaml
sources:
  - name: signal
    input_files: [signal.root]
    mean_event_frequency: 0.0
    generator_status_offset: 0
    
  - name: bg1
    input_files: [bg1.root]
    mean_event_frequency: 0.002  # 2000 kHz → 0.002 events/ns
    generator_status_offset: 1000
```

### Frequency Units

- **Original HEPMC_Merger**: Uses kHz (kilohertz) 
- **This Implementation**: Uses events/ns (events per nanosecond)

To convert: `frequency_events_per_ns = frequency_kHz * 1e-6`

Explanation:
- 1 kHz = 1000 events/second
- 1 second = 1e9 nanoseconds
- Therefore: 1 kHz = 1000 events / 1e9 ns = 1e-6 events/ns

Examples:
- 2000 kHz → 2000 * 1e-6 = 0.002 events/ns
- 20 kHz → 20 * 1e-6 = 0.00002 events/ns (2e-5 events/ns)
- 31900 kHz → 31900 * 1e-6 = 0.0319 events/ns

### Skip Events

The original HEPMC_Merger supports skipping initial events. This implementation does not currently support this feature, but it can be added if needed.

### Time Window

- **Original HEPMC_Merger**: Uses `intWindow` parameter (default 2000 ns)
- **This Implementation**: Uses `time_slice_duration` parameter (default 2000 ns)

These are equivalent - just different naming.

## Usage Examples

### Example 1: Simple Signal + Background

```bash
./install/bin/hepmc3_timeslice_merger \
    --source:signal:input_files signal.hepmc3.tree.root \
    --source:signal:frequency 0 \
    --source:bg:input_files background.hepmc3.tree.root \
    --source:bg:frequency 0.02 \
    --source:bg:status_offset 1000 \
    --source:bg:repeat_on_eof true \
    -n 1000 \
    -d 2000.0 \
    -o merged_output.hepmc3.tree.root
```

### Example 2: Multiple Background Sources

```bash
# Note: Original HEPMC_Merger uses kHz, convert to events/ns: freq_ns = freq_kHz * 1e-6
# Example: 2000 kHz → 0.002 events/ns, 3177 kHz → 0.003177 events/ns
./install/bin/hepmc3_timeslice_merger \
    --source:signal:input_files signal.root \
    --source:signal:frequency 0 \
    --source:hgas:input_files hgas.root \
    --source:hgas:frequency 0.002 \
    --source:hgas:status_offset 2000 \
    --source:egas:input_files egas.root \
    --source:egas:frequency 0.003177 \
    --source:egas:status_offset 3000 \
    --source:synrad:input_files synrad.root \
    --source:synrad:frequency 0.000025 \
    --source:synrad:status_offset 6000
```

### Example 3: Static Event Count

```bash
./install/bin/hepmc3_timeslice_merger \
    --source:signal:input_files signal.root \
    --source:signal:static_events true \
    --source:signal:events_per_slice 1 \
    --source:bg:input_files background.root \
    --source:bg:static_events true \
    --source:bg:events_per_slice 5 \
    --source:bg:status_offset 1000
```

### Example 4: Using YAML Configuration

Create a configuration file `config.yml`:

```yaml
output_file: merged_timeslices.hepmc3.tree.root
max_events: 10000
time_slice_duration: 2000.0
bunch_crossing_period: 10.0

sources:
  - name: signal
    input_files:
      - signal.hepmc3.tree.root
    mean_event_frequency: 0.0
    use_bunch_crossing: true
    generator_status_offset: 0
    repeat_on_eof: false
    
  - name: hgas
    input_files:
      - hgas.hepmc3.tree.root
    mean_event_frequency: 0.002  # 2000 kHz → 0.002 events/ns
    use_bunch_crossing: true
    generator_status_offset: 2000
    repeat_on_eof: true
    
  - name: egas
    input_files:
      - egas.hepmc3.tree.root
    mean_event_frequency: 0.003177  # 3177 kHz → 0.003177 events/ns
    use_bunch_crossing: true
    generator_status_offset: 3000
    repeat_on_eof: true
```

Then run:

```bash
./install/bin/hepmc3_timeslice_merger --config config.yml
```

## Weighted Events

For sources with event weights (such as legacy synchrotron radiation files), set the frequency to a negative value to trigger weighted mode:

```yaml
sources:
  - name: synrad
    input_files: [synrad.hepmc3.tree.root]
    mean_event_frequency: -1.0  # Negative triggers weighted mode
    generator_status_offset: 6000
```

In weighted mode:
1. All events are read into memory
2. Events with weight = 0 are discarded
3. Average rate is calculated from weights
4. Events are selected using weighted random distribution
5. Number of events per timeslice follows Poisson distribution based on average rate

## Generator Status Codes

The generator status offset allows tracking which source each particle came from. For example:

- Signal particles: status codes 1, 2, 3, ...
- Background 1 particles: status codes 1001, 1002, 1003, ... (offset = 1000)
- Background 2 particles: status codes 2001, 2002, 2003, ... (offset = 2000)

In downstream simulation (e.g., DD4hep/ddsim), you may need to configure the physics to recognize these shifted status codes:

```bash
ddsim --physics.alternativeStableStatuses="1 1001 2001" \
      --physics.alternativeDecayStatuses="2 1002 2002"
```

## Implementation Details

### Time Handling

Times in HepMC3 are stored in millimeters (as lengths). The merger:
1. Generates a random time offset in nanoseconds
2. Converts to millimeters using speed of light: `time_mm = c_light * time_ns`
3. Adds this offset to all vertex positions in the event

### Vertex and Particle Handling

The merger:
1. Creates new vertices with shifted time coordinates
2. Copies particles with potentially shifted status codes
3. Maintains all parent-daughter relationships
4. Preserves particle masses and other properties

### Memory Usage

- **Frequency-based sources**: Events are streamed, minimal memory usage
- **Weighted sources**: All events loaded into memory, higher memory usage

For large weighted source files, ensure sufficient RAM is available.

## Performance

Typical performance is 1000-10000 timeslices per second, depending on:
- Number of events per timeslice
- Number of sources
- File format (ROOT is faster than ASCII)
- Disk I/O speed
- Whether sources are weighted (slower) or frequency-based (faster)

## Limitations

- Does not support `skip` parameter from original HEPMC_Merger (can be added if needed)
- Multiple input files per source are supported but currently only the first file is used
- File chaining for multiple input files per source not yet fully implemented

## Future Enhancements

Potential improvements:
- Add event skipping support
- Implement proper file chaining for multiple input files per source
- Add more detailed logging options
- Support for time squashing (removing time information)
- XrootD streaming support for remote files
