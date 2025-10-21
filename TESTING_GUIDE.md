# Testing and Validation Guide

This guide provides instructions for testing the automatic relationship discovery implementation.

## Prerequisites

Before testing, ensure you have:
- ROOT 6.x or later
- PODIO library and headers
- EDM4HEP library and headers
- yaml-cpp library
- CMake 3.16 or later
- C++20 compatible compiler (GCC 10+ or Clang 11+)

## Building the Code

### Option 1: Using the build script
```bash
cd /path/to/EDM4hep_TimesliceGenerator
./build.sh
```

### Option 2: Manual build
```bash
mkdir -p build && cd build
cmake .. -DCMAKE_INSTALL_PREFIX=../install
make -j$(nproc)
make install
```

## Testing Phases

### Phase 1: Verify Compilation
First, ensure the code compiles without errors:
```bash
./build.sh
# Should complete without errors
```

Expected output:
- No compilation errors
- All targets built successfully
- Installation completed

### Phase 2: Test Relationship Discovery
Run the merger with debug output to see discovered relationships:

```bash
# Using a configuration file
./install/bin/timeslice_merger --config configs/config.yml

# Or with command line arguments
./install/bin/timeslice_merger -n 10 -d 1000.0 input.root
```

**Look for this output:**
```
=== Discovering Branch Relationships ===
Total branches: XX
  Found collection: MCParticles (type: vector<edm4hep::MCParticleData>)
  Found collection: VertexBarrelHits (type: vector<edm4hep::SimTrackerHitData>)
  ...
  Found relationship: _MCParticles_parents -> MCParticles.parents (one-to-many)
  Found relationship: _MCParticles_daughters -> MCParticles.daughters (one-to-many)
  Found relationship: _VertexBarrelHits_particle -> VertexBarrelHits.particle (one-to-one)
  ...
=== Discovery Complete ===
```

### Phase 3: Validate Output Structure
Check that the output file has the correct structure:

```bash
# List branches in output file
root -l merged_timeslices.root
```

In ROOT prompt:
```cpp
events->Print()  // Should show all branches
events->Scan("MCParticles.PDG")  // Verify data is present
.q
```

### Phase 4: Test with Podio Reader
Verify the output can be read by podio:

```bash
./install/bin/test_podio_format merged_timeslices.root
```

Expected output:
```
=== Podio API Format Test ===
✓ File opened successfully with Podio reader
✓ Found X events
✓ All collections can be properly formed into Podio frames
✓ Relationships between collections are preserved
=== Podio API Test Complete ===
```

## Test Cases

### Test Case 1: Basic MCParticle Relationships
**Purpose:** Verify parent-daughter relationships are correctly maintained

**Input:** Any file with MCParticles
**Expected:** 
- `_MCParticles_parents` branch discovered
- `_MCParticles_daughters` branch discovered
- Parent-child indices correctly offset

**Verification:**
```bash
./install/bin/timeslice_merger -n 5 -s -e 2 input.root -o test_mcparticles.root
./install/bin/test_podio_format test_mcparticles.root
```

### Test Case 2: Tracker Hit References
**Purpose:** Verify tracker hits maintain correct particle references

**Input:** File with tracker hit collections
**Expected:**
- All tracker collections discovered
- Particle reference branches found for each collection
- References correctly offset when merging

**Verification:**
```bash
# Check for tracker-specific output in logs
grep "Found collection.*Tracker" merger_output.log
grep "tracker relationship" merger_output.log
```

### Test Case 3: Calorimeter Hit Contributions
**Purpose:** Verify calorimeter hit to contribution mapping

**Input:** File with calorimeter hits
**Expected:**
- Calo hit collections discovered
- Contribution collections discovered (e.g., `EcalBarrelHitsContributions`)
- Contribution reference branches discovered
- Indices correctly offset

**Verification:**
```bash
# Look for calo-specific discoveries
grep "SimCalorimeterHit" merger_output.log
grep "Contributions" merger_output.log
```

### Test Case 4: Multiple Sources
**Purpose:** Verify relationships work with multiple input sources

**Input:** Config with 2+ sources
**Expected:**
- All relationships discovered from first source
- Applied correctly to all sources
- Indices offset independently per source

**Test command:**
```bash
./install/bin/timeslice_merger --config configs/config.yml
```

### Test Case 5: Already Merged Input
**Purpose:** Verify system works with pre-merged timeslice files

**Input:** Output from previous merger run
**Expected:**
- Relationships discovered from merged file
- Can re-merge without errors
- Relationships preserved through multiple passes

**Test commands:**
```bash
# First merge
./install/bin/timeslice_merger -n 10 input.root -o first_merge.root

# Second merge (using output as input)
./install/bin/timeslice_merger -n 5 first_merge.root -o second_merge.root
```

## Debugging

### Common Issues

#### Issue: "No branches found in chain"
**Cause:** Input file path incorrect or file corrupted
**Solution:** 
```bash
# Verify file exists and is readable
file input.root
root -l input.root -e "events->Print()"
```

#### Issue: "Relationship branch references unknown collection"
**Cause:** Mismatch between object branch and relationship branch names
**Solution:** Check the discovery output for the expected collection name

#### Issue: Segmentation fault during processing
**Cause:** Incorrect index offsetting or missing branch
**Solution:** 
- Enable debug output in code
- Check if all expected branches are present
- Verify data types match expectations

### Debug Build

For detailed debugging, build with debug symbols:
```bash
cd build
cmake .. -DCMAKE_BUILD_TYPE=Debug
make -j$(nproc)
```

Then run with gdb:
```bash
gdb --args ./install/bin/timeslice_merger -n 5 input.root
```

### Enable Additional Logging

Uncomment debug print statements in:
- `BranchRelationshipMapper::discoverRelationships()` 
- `DataSource::setupBranches()`
- `StandaloneTimesliceMerger::createMergedTimeslice()`

## Performance Testing

Compare performance with and without the new system:

```bash
# Time the merger
time ./install/bin/timeslice_merger -n 100 input.root -o output.root

# Monitor memory usage
/usr/bin/time -v ./install/bin/timeslice_merger -n 100 input.root
```

Expected: No significant performance degradation

## Validation Checklist

Use this checklist to validate the implementation:

### Build and Compilation
- [ ] Code compiles without errors
- [ ] No warnings about deprecated features
- [ ] All executables built successfully

### Discovery Process
- [ ] BranchRelationshipMapper successfully scans branches
- [ ] All EDM4hep collections discovered
- [ ] All relationship branches identified
- [ ] Relationship names correctly parsed
- [ ] OneToMany vs OneToOne correctly identified

### Data Processing
- [ ] MCParticle relationships work correctly
- [ ] Tracker hit particle references maintained
- [ ] Calorimeter hit contributions linked properly
- [ ] Contribution particle references correct
- [ ] Index offsetting produces valid results

### Output Validation
- [ ] Output file created successfully
- [ ] All expected branches present
- [ ] Branch structure matches input
- [ ] Data types preserved
- [ ] Can open output with ROOT
- [ ] Can read output with podio::ROOTReader

### Multi-Source Tests
- [ ] Multiple sources handled correctly
- [ ] Independent index offsetting per source
- [ ] SubEventHeaders created properly
- [ ] Cross-source relationships preserved

### Edge Cases
- [ ] Empty collections handled
- [ ] Collections with no relationships handled
- [ ] Already-merged files work as input
- [ ] Large files (>1GB) process correctly
- [ ] Many events (>10k) process correctly

### Backward Compatibility
- [ ] Fallback to hardcoded patterns works
- [ ] Old configuration files still work
- [ ] Output format unchanged
- [ ] Can be read by existing analysis code

## Reporting Issues

If you encounter problems:

1. **Gather Information:**
   - Input file details (size, collections, structure)
   - Command used to run merger
   - Full error message and stack trace
   - Discovery output from logs

2. **Create Minimal Example:**
   - Smallest input file that reproduces issue
   - Simplest command that triggers problem

3. **Report:**
   - Open GitHub issue with details
   - Include system information (OS, compiler, library versions)
   - Attach log files if relevant

## Next Steps After Validation

Once testing is complete:

1. **Document Results:**
   - Note any issues found
   - Record performance metrics
   - Document any limitations discovered

2. **Update Code:**
   - Fix any bugs found during testing
   - Optimize performance bottlenecks
   - Add additional error handling

3. **Enhance Documentation:**
   - Add examples from testing
   - Document any special cases
   - Update user guides

4. **Integration:**
   - Merge to main branch
   - Update CI/CD pipelines
   - Announce changes to users
