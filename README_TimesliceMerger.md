# TimesliceMerger Plugin

This document describes the **TimesliceMerger** plugin, which is the second JANA plugin in this repository designed to merge timeslice files output from the **TimesliceCreator** plugin.

## Overview

The TimesliceMerger plugin implements the following functionality:

1. **Reads multiple timeslice files** that have been output from the TimesliceCreator plugin
2. **Loads unique frames** from each input file
3. **Generates merged output collections** that are summed together from all different sources
4. **Preserves original collection names** from before the first plugin by removing the `ts_` prefix

## Plugin Architecture

### TimesliceCreator Plugin (First Plugin)
- Reads event files and creates timeslice files
- Outputs collections with `ts_` prefix (e.g., `ts_SiBarrelHits`, `ts_MCParticles`)
- Creates timeslice frames containing hits organized by time

### TimesliceMerger Plugin (Second Plugin)
- Reads multiple timeslice files from TimesliceCreator
- Merges collections from different sources
- Removes `ts_` prefix to restore original collection names
- Outputs merged timeslice files with summed collections

## Key Components

### MyFileReaderGeneratorEDM4HEP
- **Purpose**: Generates event source readers for timeslice files
- **Function**: Creates `MyTimesliceFileReader` instances for each input file
- **Configuration**: Automatically detects ROOT files with "timeslice" or "ts_" in the name

### MyTimesliceFileReader
- **Purpose**: Reads timeslice files and extracts collections
- **Function**: Reads frames from "timeslices" tree in ROOT files
- **Collections**: Automatically loads all available collections from each timeslice frame

### MyFileWriterEDM4HEP
- **Purpose**: Merges collections and writes output
- **Function**: 
  - Accumulates collections across multiple timeslice events
  - Merges collections by type (MCParticles, SimTrackerHits, etc.)
  - Removes `ts_` prefix to restore original names
  - Writes merged frames to output file
- **Configuration**: 
  - `writer:output_filename` - Output file name (default: "merged_output.root")
  - `writer:timeslices_per_merge` - How many timeslices to accumulate before writing (default: 1)

## Usage

### Basic Usage

1. **First, run TimesliceCreator** to generate timeslice files from event files:
```bash
jana -Pplugins=TimesliceCreator -Pjana:nevents=100 detector1_events.root
# This creates timeslices.root (rename to detector1_timeslices.root)

jana -Pplugins=TimesliceCreator -Pjana:nevents=100 detector2_events.root  
# This creates timeslices.root (rename to detector2_timeslices.root)
```

2. **Then, run TimesliceMerger** to merge the timeslice files:
```bash
jana -Pplugins=TimesliceMerger \
     -Pwriter:output_filename=merged_output.root \
     -Pwriter:timeslices_per_merge=2 \
     detector1_timeslices.root detector2_timeslices.root
```

### Advanced Usage with Multiple Sources

For merging timeslices from multiple detector sources:

```bash
# Create timeslices from multiple detector files
jana -Pplugins=TimesliceCreator -Pjana:nevents=50 detector1_events.root
mv timeslices.root detector1_timeslices.root

jana -Pplugins=TimesliceCreator -Pjana:nevents=50 detector2_events.root
mv timeslices.root detector2_timeslices.root

jana -Pplugins=TimesliceCreator -Pjana:nevents=50 detector3_events.root
mv timeslices.root detector3_timeslices.root

# Merge all timeslice files
jana -Pplugins=TimesliceMerger \
     -Pwriter:output_filename=merged_all_detectors.root \
     -Pwriter:timeslices_per_merge=3 \
     -Pjana:nevents=100 \
     detector1_timeslices.root detector2_timeslices.root detector3_timeslices.root
```

### Automated Example Script

Use the provided script for a complete example:

```bash
# Run complete example with sample data
./run_merger_example.sh full_example

# Or run individual steps:
./run_merger_example.sh create_sample    # Create test files
./run_merger_example.sh run_creator      # Generate timeslices  
./run_merger_example.sh run_merger       # Merge timeslices
./run_merger_example.sh verify           # Check output
```

## Configuration Parameters

### TimesliceMerger Parameters

| Parameter | Default | Description |
|-----------|---------|-------------|
| `writer:output_filename` | `merged_output.root` | Name of merged output file |
| `writer:timeslices_per_merge` | `1` | Number of timeslices to accumulate before writing |
| `writer:nevents` | `100` | Maximum number of merged frames to write |
| `jana:nevents` | unlimited | Maximum number of input timeslices to process |

## Collection Merging Behavior

### Original Names Restoration
- Input collections with `ts_` prefix → Output collections without prefix
- Example: `ts_SiBarrelHits` → `SiBarrelHits`
- Example: `ts_MCParticles` → `MCParticles`

### Supported Collection Types
- **MCParticle collections**: Merged by concatenation
- **SimTrackerHit collections**: Merged by concatenation  
- **SimCalorimeterHit collections**: Merged by concatenation
- **CaloHitContribution collections**: Merged by concatenation
- **EventHeader collections**: Merged by concatenation

### Merging Strategy
The plugin uses **additive merging**:
- All hits/particles from different sources are combined
- No deduplication is performed
- Order is preserved within each source
- Original object relationships are maintained

## Output Structure

The merged output file contains:
- **Tree name**: `merged_timeslices`
- **Collections**: Original collection names (without `ts_` prefix)
- **Content**: Summed collections from all input sources
- **Metadata**: Combined information from all input timeslices

## Example Workflow

1. **Prepare input files**: Multiple event files from different detectors
2. **Create timeslices**: Run TimesliceCreator on each detector file
3. **Merge timeslices**: Run TimesliceMerger on all timeslice files
4. **Verify output**: Check merged file contains expected collections

## Error Handling

- **Missing collections**: Optional collections are skipped if not present
- **Type mismatches**: Warnings printed for unhandled collection types
- **File errors**: Plugin gracefully handles missing or corrupted files
- **Memory management**: Automatic cleanup of temporary collections

## Performance Considerations

- **Memory usage**: Controlled by `timeslices_per_merge` parameter
- **I/O efficiency**: Batch processing reduces file operations
- **Scalability**: Can handle large numbers of input files
- **Threading**: Sequential processing ensures deterministic output

## Troubleshooting

### Common Issues

1. **"No timeslice frame available"**: Input file may not contain timeslice data
2. **"Collection not found"**: Expected collection missing from input
3. **"Memory allocation error"**: Reduce `timeslices_per_merge` value

### Debug Options
```bash
jana -Plog:global=debug -Pjana:debug_plugin_loading=1 ...
```

### Verification
Check output file structure:
```bash
root -l merged_output.root
> .ls
> merged_timeslices->Print()
```