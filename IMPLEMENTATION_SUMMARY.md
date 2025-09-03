# TimesliceMerger Plugin Implementation Summary

## Overview
Successfully implemented a second JANA plugin (**TimesliceMerger**) that meets the requirements specified in the problem statement:

✅ **Takes in a list of files** output from the first plugin (TimesliceCreator)
✅ **Loads unique frames** from each input file  
✅ **Generates output collection** that is summed together from all sources
✅ **Preserves original names** from before the first plugin (removes `ts_` prefix)

## Implementation Details

### New Files Created

1. **MyTimesliceFileReader.h** - Reads timeslice files (vs event files in original)
2. **MyTimesliceFileReaderGenerator.h** - Generates timeslice file readers
3. **MyFileReaderGeneratorEDM4HEP.h** - Alternative name for compatibility
4. **MyFileWriterEDM4HEP.h** - Comprehensive merger that combines collections from multiple sources
5. **MyTimesliceBuilderEDM4HEP.h** - EDM4HEP builder (created but not used in final design)

### Modified Files

1. **CMakeLists.txt** - Enabled TimesliceMerger plugin compilation
2. **src/TimesliceMerger.cc** - Simplified to use frame-based reading and merging

### New Documentation and Examples

1. **run_merger_example.sh** - Complete example script showing both plugins in action
2. **README_TimesliceMerger.md** - Comprehensive documentation for the new plugin

## Plugin Architecture

### TimesliceCreator → TimesliceMerger Workflow

```
[detector1_events.root] → TimesliceCreator → [detector1_timeslices.root]
[detector2_events.root] → TimesliceCreator → [detector2_timeslices.root]  
[detector3_events.root] → TimesliceCreator → [detector3_timeslices.root]
                                                    ↓
                              TimesliceMerger → [merged_timeslices.root]
```

### Collection Name Preservation

**TimesliceCreator outputs:**
- `ts_SiBarrelHits`
- `ts_VertexBarrelHits` 
- `ts_MCParticles`

**TimesliceMerger outputs:**
- `SiBarrelHits` (original name restored)
- `VertexBarrelHits` (original name restored)
- `MCParticles` (original name restored)

## Key Features

### Automatic Collection Merging
- **Frame-based processing**: Reads complete PODIO frames from timeslice files
- **Type-aware merging**: Handles MCParticles, SimTrackerHits, SimCalorimeterHits, etc.
- **Additive combining**: Sums all collections from different sources
- **Memory efficient**: Configurable batching with `timeslices_per_merge`

### Original Name Restoration  
- Automatically removes `ts_` prefix from collection names
- Preserves original detector collection naming scheme
- Maintains collection relationships and metadata

### Flexible Input Handling
- Accepts multiple timeslice files on command line
- Automatically detects timeslice files vs event files
- Graceful handling of missing or optional collections

## Usage Examples

### Basic Usage
```bash
# Create timeslices from multiple sources
jana -Pplugins=TimesliceCreator detector1_events.root
jana -Pplugins=TimesliceCreator detector2_events.root

# Merge timeslices with original names restored  
jana -Pplugins=TimesliceMerger \
     -Pwriter:output_filename=merged.root \
     detector1_timeslices.root detector2_timeslices.root
```

### Advanced Configuration
```bash
jana -Pplugins=TimesliceMerger \
     -Pwriter:output_filename=merged_all.root \
     -Pwriter:timeslices_per_merge=3 \
     -Pjana:nevents=100 \
     det1_timeslices.root det2_timeslices.root det3_timeslices.root
```

### Complete Example Script
```bash
./run_merger_example.sh full_example
```

## Technical Implementation

### MyFileWriterEDM4HEP Core Logic
1. **Frame Reading**: Extracts PODIO frames from timeslice events  
2. **Collection Accumulation**: Stores collections by type in maps
3. **Name Processing**: Removes `ts_` prefix to restore original names
4. **Batch Writing**: Writes merged frames when accumulation threshold reached
5. **Memory Management**: Clears accumulators after each write

### Collection Merging Strategy
- **MCParticles**: Clone and concatenate from all sources
- **TrackerHits**: Clone and concatenate preserving hit information  
- **CalorimeterHits**: Clone and concatenate with contributions
- **Headers**: Combine event metadata appropriately

## Verification

The implementation can be verified by:

1. **Running example script**: `./run_merger_example.sh full_example`
2. **Checking output structure**: Merged file contains expected collections
3. **Validating names**: Collection names match original (pre-TimesliceCreator) names
4. **Confirming content**: Hit counts are sum of all input sources

## Benefits

1. **Modular Design**: Clean separation between file reading and merging logic
2. **Extensible**: Easy to add support for new collection types
3. **Configurable**: Multiple parameters for different use cases  
4. **Robust**: Error handling for missing files and collections
5. **Documented**: Comprehensive documentation and examples

## Conclusion

The TimesliceMerger plugin successfully implements a second JANA plugin that:
- Reads output files from TimesliceCreator
- Merges collections from multiple sources  
- Preserves original collection names
- Provides flexible configuration options
- Includes comprehensive documentation and examples

The plugin is ready for use and can be extended as needed for additional functionality.