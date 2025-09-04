# Comparison: JANA vs Standalone Timeslice Merger

This document compares the usage and functionality between the JANA-based TimesliceCreator plugin and the new standalone Podio-based timeslice merger.

## Command Line Comparison

### JANA-based TimesliceCreator

```bash
jana -Pplugins=TimesliceCreator \
     -Pwriter:nevents=100 \
     -Ptimeslice:duration=10000.0 \
     -Ptimeslice:bunch_crossing_period=1000.0 \
     -Ptimeslice:use_bunch_crossing=true \
     -Ptimeslice:static_number_of_events=true \
     -Ptimeslice:static_events_per_timeslice=2 \
     -Poutput_file=output.root \
     input.root
```

### Standalone Timeslice Merger

```bash
./timeslice_merger \
     --nevents 100 \
     --duration 10000.0 \
     --bunch-period 1000.0 \
     --use-bunch-crossing \
     --static-events \
     --events-per-slice 2 \
     --output output.root \
     input.root
```

## Parameter Mapping

| JANA Parameter | Standalone Option | Description |
|----------------|------------------|-------------|
| `-Pwriter:nevents=N` | `-n N, --nevents N` | Number of timeslices to generate |
| `-Ptimeslice:duration=X` | `-d X, --duration X` | Timeslice duration in ns |
| `-Ptimeslice:bunch_crossing_period=X` | `-p X, --bunch-period X` | Bunch crossing period in ns |
| `-Ptimeslice:use_bunch_crossing=true` | `-b, --use-bunch-crossing` | Enable bunch crossing logic |
| `-Ptimeslice:static_number_of_events=true` | `-s, --static-events` | Use static events per timeslice |
| `-Ptimeslice:static_events_per_timeslice=N` | `-e N, --events-per-slice N` | Static events per timeslice |
| `-Ptimeslice:mean_event_frequency=X` | `-f X, --frequency X` | Mean event frequency (events/ns) |
| `-Ptimeslice:attach_to_beam=true` | `--beam-attachment` | Enable beam attachment |
| `-Ptimeslice:beam_speed=X` | `--beam-speed X` | Beam speed in ns/mm |
| `-Ptimeslice:beam_spread=X` | `--beam-spread X` | Beam spread for Gaussian smearing |
| `-Ptimeslice:generator_status_offset=N` | `--status-offset N` | Generator status offset |
| `-Poutput_file=FILE` | `-o FILE, --output FILE` | Output file name |

## Functional Equivalence

Both implementations provide identical functionality:

### 1. **Event Accumulation**
- Both collect events until the required number is reached (static or Poisson-distributed)
- Same logic for determining when to create a timeslice

### 2. **Time Offset Generation**
- Identical random distribution setup (uniform over timeslice duration)
- Same bunch crossing logic (floor-based discretization)
- Identical beam attachment with Gaussian smearing

### 3. **Collection Processing**
- Same handling of MCParticle collections (time and status updates)
- Identical SimTrackerHit processing (time updates and particle references)
- Same SimCalorimeterHit and CaloHitContribution processing
- Proper particle reference mapping maintained

### 4. **Output Format**
- JANA version outputs to "timeslices" tree via JANA event processing
- Standalone version outputs to "timeslices" tree directly via Podio
- Both produce identical ROOT file structure and content

## Advantages of Each Approach

### JANA-based TimesliceCreator

**Advantages:**
- Integrated with JANA event processing framework
- Can leverage JANA's multithreading and event scheduling
- Part of larger JANA plugin ecosystem
- Automatic event source and sink management
- Built-in parameter management system

**Use cases:**
- When you're already using JANA for event processing
- Need integration with other JANA plugins
- Want automatic multithreading support
- Part of larger analysis framework

### Standalone Timeslice Merger

**Advantages:**
- No JANA dependency - lighter weight
- Simpler build and deployment
- Direct control over file I/O
- Easier to understand and modify
- Can be easily integrated into other build systems
- Command line interface familiar to most users

**Use cases:**
- Don't need full JANA framework
- Want minimal dependencies
- Integrating into existing non-JANA workflow
- Simple batch processing scenarios
- Learning or prototyping

## Performance Considerations

### JANA Version
- Can utilize multiple threads for parallel processing
- Event-driven architecture with efficient memory management
- Built-in monitoring and profiling tools

### Standalone Version
- Single-threaded sequential processing
- Direct memory management (no framework overhead)
- Potentially faster for small jobs due to reduced overhead

## Migration Guide

To migrate from JANA to standalone:

1. **Replace parameters**: Use the mapping table above
2. **Update scripts**: Replace `jana -P...` with direct executable calls
3. **Batch processing**: Update any workflow management systems
4. **Dependencies**: Remove JANA from build requirements, keep Podio/EDM4HEP

Example migration:

```bash
# Before (JANA)
for file in inputs/*.root; do
    jana -Pplugins=TimesliceCreator -Pwriter:nevents=100 \
         -Ptimeslice:duration=10000 -Poutput_file=ts_$(basename $file) $file
done

# After (Standalone)  
for file in inputs/*.root; do
    ./timeslice_merger -n 100 -d 10000 -o ts_$(basename $file) $file
done
```

## Summary

The standalone merger provides the exact same merging logic and configuration options as the JANA version, but with a simpler architecture and no framework dependencies. Choose based on your specific use case and integration requirements.