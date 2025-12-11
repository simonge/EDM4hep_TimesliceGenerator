# Testing Guide for HepMC3 Timeslice Merger

This document provides instructions for testing the HepMC3 timeslice merger implementation.

## Prerequisites

Before testing, ensure you have:
- HepMC3 library (>= 3.2.0) installed
- ROOT (>= 6.20) with HepMC3 ROOT I/O support
- yaml-cpp library
- CMake (>= 3.16)
- C++17 compatible compiler

## Building with HepMC3 Support

### 1. Clean Build

```bash
cd /path/to/EDM4hep_TimesliceGenerator
rm -rf build install
mkdir build && cd build
```

### 2. Configure with CMake

If HepMC3 is in a standard location:
```bash
cmake .. -DCMAKE_INSTALL_PREFIX=../install
```

If HepMC3 is in a custom location:
```bash
cmake .. -DCMAKE_INSTALL_PREFIX=../install -DHepMC3_DIR=/path/to/hepmc3/share/HepMC3/cmake
```

### 3. Verify HepMC3 Detection

Look for this message in the CMake output:
```
-- HepMC3 found - HepMC3 merger will be built
```

If you see:
```
-- HepMC3 not found - HepMC3 merger will NOT be built
```

Then HepMC3 was not found and you need to specify its location.

### 4. Build

```bash
make -j$(nproc)
make install
```

### 5. Verify Installation

```bash
ls ../install/bin/hepmc3_timeslice_merger
```

If the file exists, the HepMC3 merger was successfully built.

## Test Cases

### Test 1: Basic Functionality Check

Test the help message:
```bash
./install/bin/hepmc3_timeslice_merger --help
```

Expected output: Help message showing all command-line options.

### Test 2: Single Source Merging

Create a minimal test with one signal source:

```bash
./install/bin/hepmc3_timeslice_merger \
    --source:signal:input_files /path/to/signal.hepmc3.tree.root \
    --source:signal:frequency 0 \
    -n 10 \
    -d 2000.0 \
    -o test_single_source.hepmc3.tree.root
```

Expected behavior:
- Creates output file `test_single_source.hepmc3.tree.root`
- Processes 10 timeslices
- Each timeslice contains exactly 1 event
- No errors or crashes

### Test 3: Multiple Source Merging

Test with signal + background:

```bash
./install/bin/hepmc3_timeslice_merger \
    --source:signal:input_files /path/to/signal.hepmc3.tree.root \
    --source:signal:frequency 0 \
    --source:bg:input_files /path/to/background.hepmc3.tree.root \
    --source:bg:frequency 0.00002 \
    --source:bg:status_offset 1000 \
    --source:bg:repeat_on_eof true \
    -n 100 \
    -d 2000.0 \
    -o test_multi_source.hepmc3.tree.root
```

Expected behavior:
- Creates output file with merged events
- Background events distributed according to Poisson with given frequency
- Background particle status codes offset by 1000
- Background file cycles when EOF reached

### Test 4: Static Event Count

Test static number of events per timeslice:

```bash
./install/bin/hepmc3_timeslice_merger \
    --source:signal:input_files /path/to/signal.hepmc3.tree.root \
    --source:signal:static_events true \
    --source:signal:events_per_slice 3 \
    -n 50 \
    -o test_static.hepmc3.tree.root
```

Expected behavior:
- Each timeslice contains exactly 3 events
- Events placed at random times within each timeslice

### Test 5: YAML Configuration

Create a test configuration file `test_config.yml`:

```yaml
output_file: test_yaml.hepmc3.tree.root
max_events: 50
time_slice_duration: 2000.0
bunch_crossing_period: 10.0

sources:
  - name: signal
    input_files: [/path/to/signal.hepmc3.tree.root]
    mean_event_frequency: 0.0
    generator_status_offset: 0
    
  - name: background
    input_files: [/path/to/background.hepmc3.tree.root]
    mean_event_frequency: 0.00002
    generator_status_offset: 1000
    repeat_on_eof: true
```

Run:
```bash
./install/bin/hepmc3_timeslice_merger --config test_config.yml
```

Expected behavior:
- Uses all settings from YAML file
- Produces output file as specified

### Test 6: Bunch Crossing

Test bunch crossing discretization:

```bash
./install/bin/hepmc3_timeslice_merger \
    --source:signal:input_files /path/to/signal.hepmc3.tree.root \
    --source:signal:frequency 0 \
    --source:signal:bunch_crossing true \
    -d 2000.0 \
    -p 10.0 \
    -n 10 \
    -o test_bunch_crossing.hepmc3.tree.root
```

Expected behavior:
- Event times aligned to bunch crossing boundaries (multiples of 10 ns)

### Test 7: Output Format

Test ASCII output:

```bash
./install/bin/hepmc3_timeslice_merger \
    --source:signal:input_files /path/to/signal.hepmc3.tree.root \
    --source:signal:frequency 0 \
    -n 5 \
    -o test_ascii.hepmc3
```

Expected behavior:
- Creates ASCII format HepMC3 file (not ROOT format)
- File is human-readable text

### Test 8: Large Scale Test

Test with many timeslices:

```bash
./install/bin/hepmc3_timeslice_merger \
    --source:signal:input_files /path/to/signal.hepmc3.tree.root \
    --source:signal:frequency 0 \
    -n 10000 \
    -o test_large_scale.hepmc3.tree.root
```

Expected behavior:
- Processes 10000 timeslices without crashing
- Reports progress every 1000 slices
- Reports statistics at end

## Validation

### Validate Output File

Use HepMC3 tools to validate the output:

```bash
# If you have HepMC3 tools installed
hepmc3_reader test_single_source.hepmc3.tree.root
```

Or use ROOT:

```c++
// In ROOT
TFile* f = TFile::Open("test_single_source.hepmc3.tree.root");
TTree* tree = (TTree*)f->Get("hepmc3_tree");
tree->Print();
tree->Show(0);  // Show first event
```

### Check Event Content

Verify that:
1. Events contain particles with correct properties
2. Vertex positions have time offsets applied
3. Particle status codes are correctly offset for different sources
4. Event weights are preserved (if applicable)

### Check Statistics

The merger prints statistics at the end. Verify:
- Number of events placed from each source
- Average events per timeslice matches expectations
- Number of final state particles is reasonable

Example expected output:
```
=== Statistics ===
Source: signal
  Events placed: 100
  Average events/slice: 1.000
  Final state particles: 15234
  Average particles/slice: 152.340

Source: background
  Events placed: 4012
  Average events/slice: 40.120
  Final state particles: 80240
  Average particles/slice: 802.400
```

## Common Issues and Solutions

### Issue: HepMC3 Not Found

**Symptom**: `HepMC3 not found - HepMC3 merger will NOT be built`

**Solution**: 
- Ensure HepMC3 is installed
- Specify HepMC3 location: `-DHepMC3_DIR=/path/to/hepmc3/share/HepMC3/cmake`
- Check that HepMC3CMakeConfig files are present in the cmake directory

### Issue: Compilation Errors

**Symptom**: Errors about missing HepMC3 headers or functions

**Solution**:
- Ensure HepMC3 version is >= 3.2.0
- Check that development headers are installed (not just runtime libraries)

### Issue: Runtime Errors Reading Input

**Symptom**: `Failed to open HepMC3 file`

**Solution**:
- Verify input file exists and path is correct
- Check file format is compatible (HepMC3 format)
- Ensure ROOT is built with HepMC3 support if using .root files

### Issue: Incorrect Event Frequencies

**Symptom**: Too many or too few background events

**Solution**:
- Double-check frequency units (events/ns, not kHz)
- Verify: frequency_events_per_ns = frequency_kHz * 1e-6
- Example: 20 kHz = 0.00002 events/ns

### Issue: Memory Issues

**Symptom**: High memory usage or crashes with weighted sources

**Solution**:
- Weighted sources load all events into memory
- Use frequency-based sources instead if memory is limited
- Split large weighted source files into smaller chunks

## Performance Benchmarks

Typical performance expectations:
- Frequency-based sources: 5000-10000 timeslices/second
- Weighted sources: 1000-5000 timeslices/second (depends on file size)
- Output to ROOT: faster than ASCII
- Multiple sources: approximately linear slowdown with number of sources

## Reporting Issues

When reporting issues, please include:
1. HepMC3 version: `hepmc3-config --version`
2. ROOT version: `root-config --version`
3. CMake configuration output
4. Full error messages
5. Minimal reproducing example
6. Input file characteristics (size, format, number of events)

## Further Testing

For comprehensive testing, consider:
1. Different HepMC3 file formats (ASCII, ROOT, IO_GenEvent)
2. Various particle types and event topologies
3. Extreme frequencies (very high and very low)
4. Edge cases (empty files, single event files)
5. Long-running tests for memory leak detection
6. Comparison with original HEPMC_Merger output for validation
