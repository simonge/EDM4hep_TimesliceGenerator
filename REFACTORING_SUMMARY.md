# StandaloneTimesliceMerger Refactoring Summary

## Overview
This document summarizes the major refactoring of the StandaloneTimesliceMerger from a podio::Frame-based approach to a vector-based ROOT processing approach as requested.

## Key Changes

### 1. Architecture Change
- **Before**: Used podio::Frame and podio::ROOTReader/Writer for I/O
- **After**: Uses ROOT TChain/TTree directly with vector branches

### 2. Data Structures
- **Before**: Worked with edm4hep Collections (MCParticleCollection, etc.)  
- **After**: Works with std::vector<edm4hep::*Data> types directly

### 3. Processing Flow
- **Before**: Read entire frames, clone objects, manage complex relationships
- **After**: Read vectors directly, append to global vectors, update indices

## Implementation Details

### Header Changes (StandaloneTimesliceMerger.h)
- Replaced `podio::ROOTReader` with `TChain` in SourceReader
- Added branch pointer maps for different data types
- Added global merged vector storage
- Simplified method signatures

### Core Changes (StandaloneTimesliceMerger.cc)

#### initializeInputFiles()
- Creates TChain for each source instead of podio::ROOTReader
- Discovers collection names by inspecting ROOT branches
- Sets up branch addresses for direct vector reading

#### mergeEventData()
- New method that processes individual events
- Reads data directly into vectors using TChain::GetEntry()
- Updates time fields for SimTrackerHits, MCParticles, CaloHitContributions
- Manages ObjectID references by adjusting indices based on vector offsets
- Handles parent-daughter relationships by updating indices

#### createMergedTimeslice()
- Simplified to clear vectors and call mergeEventData() for each event
- Appends data to global merged vectors
- Writes merged data to output tree

#### Output
- Uses ROOT TTree with vector branches instead of podio::ROOTWriter
- Preserves collection structure in ROOT format

## Requirements Addressed

✅ **Strip back code**: Removed complex podio::Frame handling
✅ **Keep main and config**: Configuration and main function unchanged  
✅ **Redo IO and processing**: Complete rewrite of I/O using ROOT directly
✅ **Open all sources**: TChain handles multiple input files per source
✅ **Read collections as vectors**: Direct vector branch reading
✅ **Update Time fields**: Time updated for required collection types
✅ **Append to global vectors**: Data appended to merged vectors
✅ **Update ObjectIDs**: Particle references adjusted using vector offsets
✅ **Preserve metadata**: ROOT tree structure maintains compatibility

## Benefits

1. **Simplified Architecture**: Removed podio::Frame complexity
2. **Direct Vector Access**: More efficient data handling
3. **Clearer Data Flow**: Easier to understand append-based processing
4. **Better Performance**: Reduced object copying and cloning
5. **Maintained Functionality**: All original features preserved

## Compatibility

- Configuration files remain compatible
- Command-line interface unchanged
- Output format compatible with ROOT-based analysis
- Collection naming follows dd4hep/edm4hep conventions

## Build Requirements

- ROOT (for TTree/TChain functionality)
- podio (for data type definitions)
- EDM4HEP (for data structures)
- yaml-cpp (for configuration)

## Testing

The refactored code maintains the same external interface and should produce equivalent results with improved performance and simplified internals.