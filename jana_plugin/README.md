# TimeframeBuilder JANA2 Plugin

This directory contains a JANA2 plugin that integrates the TimeframeBuilder event merging functionality into JANA2. The plugin provides JEventSource implementations that merge multiple events from different input sources into timeframes, which can then be consumed by JANA2 factories and processors for further analysis.

## Overview

The plugin provides two JEventSource implementations:

1. **JEventSourceTimeframeBuilderEDM4hep**: For EDM4hep/PODIO format files (`.edm4hep.root`)
2. **JEventSourceTimeframeBuilderHepMC3**: For HepMC3 format files (`.hepmc3.tree.root`) - only available if HepMC3 is found during build

These event sources use the same configuration and merging logic as the standalone `timeframe_builder` executable, but integrate seamlessly with JANA2's event processing framework.

## Building

The plugin is built automatically when JANA2 is found during the CMake configuration. To build:

```bash
mkdir build && cd build
cmake .. -DCMAKE_INSTALL_PREFIX=../install
make -j$(nproc)
make install
```

To disable the JANA2 plugin build even if JANA2 is available:

```bash
cmake .. -DBUILD_JANA_PLUGIN=OFF
```

## Requirements

- **JANA2**: Required for building the plugin
- **PODIO**: Required for EDM4hep support
- **EDM4HEP**: Required for EDM4hep support
- **HepMC3**: Optional, for HepMC3 format support
- **ROOT**: Required for data I/O
- **yaml-cpp**: Required for configuration

## Usage

### Basic Usage

Run JANA with an EDM4hep input file:

```bash
jana -Pplugins=timeframe_builder_plugin input_file.edm4hep.root
```

Or with a HepMC3 input file (if HepMC3 support is compiled):

```bash
jana -Pplugins=timeframe_builder_plugin input_file.hepmc3.tree.root
```

### Configuration Parameters

**The plugin now supports the full breadth of configuration options available in the standalone tool.**

All TimeframeBuilder parameters use the `tfb:` prefix:

#### Global Timeframe Configuration

| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `tfb:timeframe_duration` | float | 2000.0 | Duration of each timeframe in nanoseconds |
| `tfb:bunch_crossing_period` | float | 10.0 | Bunch crossing period in nanoseconds |
| `tfb:max_timeframes` | int | 100 | Maximum number of timeframes to process |
| `tfb:random_seed` | uint | 0 | Random seed (0 = use random_device) |
| `tfb:introduce_offsets` | bool | true | Introduce random time offsets |
| `tfb:merge_particles` | bool | false | Merge particles (advanced feature) |
| `tfb:output_file` | string | "" | Output file (empty = no output) |

#### Multi-Source Configuration

| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `tfb:source_names` | string | "" | Comma-separated list of source names (empty = single source mode) |

#### Default Source Parameters (used when source_names is empty)

| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `tfb:static_events` | bool | false | Use static number of events per timeframe |
| `tfb:events_per_frame` | int | 1 | Events per timeframe (if static) |
| `tfb:event_frequency` | float | 1.0 | Mean event frequency in events/ns |
| `tfb:use_bunch_crossing` | bool | false | Enable bunch crossing discretization |
| `tfb:attach_to_beam` | bool | false | Enable beam attachment |
| `tfb:beam_angle` | float | 0.0 | Beam angle in radians |
| `tfb:beam_speed` | float | 299.792458 | Beam speed in mm/ns |
| `tfb:beam_spread` | float | 0.0 | Gaussian beam time spread in ns |
| `tfb:status_offset` | int | 0 | Generator status offset |
| `tfb:already_merged` | bool | false | Input is pre-merged timeframes |
| `tfb:tree_name` | string | "events" | TTree name in input file |
| `tfb:repeat_on_eof` | bool | false | Repeat source when EOF reached |

#### Per-Source Parameters (when using source_names)

For each source named `SOURCENAME`, parameters use the format `tfb:SOURCENAME:parameter`:

| Parameter | Type | Description |
|-----------|------|-------------|
| `tfb:SOURCENAME:input_files` | string | Comma-separated list of input files |
| `tfb:SOURCENAME:static_events` | bool | Use static event count |
| `tfb:SOURCENAME:events_per_frame` | int | Events per timeframe (if static) |
| `tfb:SOURCENAME:event_frequency` | float | Mean event frequency |
| `tfb:SOURCENAME:use_bunch_crossing` | bool | Enable bunch crossing |
| `tfb:SOURCENAME:attach_to_beam` | bool | Enable beam attachment |
| `tfb:SOURCENAME:beam_angle` | float | Beam angle in radians |
| `tfb:SOURCENAME:beam_speed` | float | Beam speed in mm/ns |
| `tfb:SOURCENAME:beam_spread` | float | Beam time spread in ns |
| `tfb:SOURCENAME:status_offset` | int | Generator status offset |
| `tfb:SOURCENAME:already_merged` | bool | Pre-merged input |
| `tfb:SOURCENAME:tree_name` | string | TTree name |
| `tfb:SOURCENAME:repeat_on_eof` | bool | Repeat on EOF |

### Example Configurations

#### Basic Single Source (Backward Compatible)

```bash
jana -Pplugins=timeframe_builder_plugin \
     -Ptfb:timeframe_duration=5000.0 \
     -Ptfb:max_timeframes=500 \
     input.edm4hep.root
```

#### Static Event Count (Single Source)

Merge exactly 3 events per timeframe:

```bash
jana -Pplugins=timeframe_builder_plugin \
     -Ptfb:static_events=true \
     -Ptfb:events_per_frame=3 \
     -Ptfb:max_timeframes=100 \
     input.edm4hep.root
```

#### Multiple Sources with Different Configurations

Signal and background with different properties:

```bash
jana -Pplugins=timeframe_builder_plugin \
     -Ptfb:source_names=signal,background \
     -Ptfb:signal:input_files=signal1.root,signal2.root \
     -Ptfb:signal:event_frequency=0.5 \
     -Ptfb:signal:use_bunch_crossing=true \
     -Ptfb:background:input_files=bg.root \
     -Ptfb:background:static_events=true \
     -Ptfb:background:events_per_frame=2 \
     -Ptfb:background:status_offset=1000 \
     -Ptfb:timeframe_duration=10000.0 \
     -Ptfb:max_timeframes=1000
```

#### Three Sources: Signal, BH Background, DIS Background

```bash
jana -Pplugins=timeframe_builder_plugin \
     -Ptfb:source_names=signal,bh_background,dis_background \
     -Ptfb:signal:input_files=signal.root \
     -Ptfb:signal:event_frequency=0.1 \
     -Ptfb:bh_background:input_files=bh_bg.root \
     -Ptfb:bh_background:static_events=true \
     -Ptfb:bh_background:events_per_frame=5 \
     -Ptfb:bh_background:status_offset=1000 \
     -Ptfb:dis_background:input_files=dis_bg.root \
     -Ptfb:dis_background:event_frequency=0.05 \
     -Ptfb:dis_background:status_offset=2000

#### Bunch Crossing Mode

Enable bunch crossing with realistic timing:

```bash
jana -Pplugins=timeframe_builder_plugin \
     -Ptfb:use_bunch_crossing=true \
     -Ptfb:bunch_crossing_period=1000.0 \
     -Ptfb:timeframe_duration=20000.0 \
     input.edm4hep.root
```

#### Full Beam Physics

Enable beam attachment with time-of-flight calculations:

```bash
jana -Pplugins=timeframe_builder_plugin \
     -Ptfb:attach_to_beam=true \
     -Ptfb:beam_spread=0.5 \
     -Ptfb:beam_speed=299.792458 \
     -Ptfb:timeframe_duration=10000.0 \
     input.edm4hep.root
```

## Architecture

The plugin integrates with JANA2 using the following design:

1. **JEventSource**: Each format has its own JEventSource implementation that handles file opening, event emission, and resource cleanup.

2. **Configuration**: Uses JANA's parameter system for all configuration, allowing settings to be passed via command-line arguments.

3. **Event Merging**: Uses the existing TimeframeBuilder, DataHandler, and DataSource classes to perform the actual event merging.

4. **Event Emission**: Each call to `Emit()` generates one merged timeframe and provides it to JANA2 for processing.

### Event Flow

```
Input Files → DataSource → TimeframeBuilder → DataHandler → JEventSource → JANA2 Event → Factories → Processors
```

## Integration with Downstream Factories

**Current Implementation Status**: The merged timeframe data is generated internally but not yet fully exposed to JANA2 factories. The current implementation has the following characteristics:

1. **Timeframe Generation**: Events are successfully merged into timeframes using the existing TimeframeBuilder logic
2. **Event Numbering**: Each timeframe is assigned a unique event number in JANA2
3. **Data Access**: The merged collections are available internally but require additional work to expose them to JANA2 factories

**Recommended Usage Pattern**: 

The current implementation is best used in one of these ways:

1. **Write-then-Read Pattern**: Configure the plugin to write merged timeframes to an output file using `tfb:output_file`, then use a standard PODIO JEventSource to read the merged data:
   ```bash
   # First pass: Generate merged timeframes
   jana -Pplugins=timeframe_builder_plugin \
        -Ptfb:output_file=merged.edm4hep.root \
        input.edm4hep.root
   
   # Second pass: Process merged timeframes with standard PODIO source
   jana -Pplugins=podio merged.edm4hep.root
   ```

2. **Future Enhancement**: Direct exposure of merged collections to JANA2 factories. This would require:
   - Converting the internal `*Data` types back to proper PODIO collection types
   - Creating a podio::Frame from the merged collections
   - Inserting collections into JEvent for factory access

**Why This Matters**: This limitation exists because the TimeframeBuilder uses ROOT's native vector types for efficiency during merging, while JANA2 factories typically expect PODIO collection types. The conversion layer has not yet been implemented but is planned for future versions.

## Differences from Standalone Tool

The JANA2 plugin differs from the standalone `timeframe_builder` executable in the following ways:

1. **Configuration**: Uses JANA parameters instead of command-line arguments or YAML files
2. **Output**: By default, does not write output files (controlled by `tfb:output_file` parameter)
3. **Event-by-Event**: Processes timeframes one at a time through JANA's event loop
4. **Integration**: Allows downstream JANA2 factories to process merged events directly

## Development

### Adding New Event Sources

To add support for a new data format:

1. Create a new JEventSource class (e.g., `JEventSourceTimeframeBuilderNewFormat.h/cc`)
2. Implement the required methods: `Open()`, `Close()`, `Emit()`, `GetDescription()`
3. Implement the `CheckOpenable()` template specialization
4. Register the event source in `timeframe_builder_plugin.cc`
5. Update the CMakeLists.txt if new dependencies are required

### Testing

To test the plugin, use the provided test script:

```bash
# Build and test (requires JANA2 environment)
cd jana_plugin
./test_jana_plugin.sh /path/to/input.edm4hep.root
```

Or manually test:

```bash
# Build and install
cd build
cmake .. && make install

# Run with test input
jana -Pplugins=timeframe_builder_plugin \
     -Ptfb:max_timeframes=10 \
     path/to/test_input.edm4hep.root
```

The test script (`test_jana_plugin.sh`) will:
1. Check for JANA2 availability
2. Build the project with plugin support
3. Verify the plugin library was created
4. Run JANA with a sample input file (if provided)

## Troubleshooting

### Plugin Not Found

If JANA reports that the plugin cannot be found:

```bash
export JANA_PLUGIN_PATH=/path/to/TimeframeBuilder/install/lib:$JANA_PLUGIN_PATH
```

### JANA2 Not Found During Build

If CMake cannot find JANA2:

```bash
cmake .. -DCMAKE_PREFIX_PATH=/path/to/jana2/install
```

Or disable the plugin build:

```bash
cmake .. -DBUILD_JANA_PLUGIN=OFF
```

### Missing Dependencies

Ensure all required dependencies are installed and findable by CMake:

```bash
export CMAKE_PREFIX_PATH=/path/to/podio:/path/to/edm4hep:/path/to/hepmc3:$CMAKE_PREFIX_PATH
```

## License

This plugin is part of the TimeframeBuilder project and is subject to the same license terms as the main project.

## Support

For questions, issues, or contributions, please refer to the main TimeframeBuilder repository.
