# Architectural Refactoring Summary: Unified Collection Processing

## Overview

This document describes the major architectural refactoring that moved all collection processing logic from DataSource to StandaloneTimesliceMerger, creating a clear separation of concerns.

## Problem

Previously, DataSource had numerous `process*()` methods that applied various offsets:
- `processMCParticles()` - applied time, generator status, and index offsets
- `processTrackerHits()` - applied time offsets
- `processCaloHits()` - applied index offsets
- `processCaloContributions()` - applied time offsets
- `processObjectID()` - applied index offsets
- Several GP processing methods

This mixed data loading with data processing, making the architecture unclear and difficult to maintain.

## Solution

### 1. DataSource: Pure Data Provider

DataSource now only handles data loading and provides raw access to collections:

**Removed:**
- All `process*()` methods (processMCParticles, processTrackerHits, etc.)
- Internal state tracking (current_time_offset_, current_particle_index_offset_)

**Added:**
- Simple getter methods for raw collection access:
  - `getMCParticles()`
  - `getTrackerHits(collection_name)`
  - `getCaloHits(collection_name)`
  - `getCaloContributions(collection_name)`
  - `getObjectIDCollection(collection_name)`
  - GP branch getters
  
- Public helper methods:
  - `generateTimeOffset()` - calculate time offset
  - `calculateBeamDistance()` - calculate beam distance
  
- Metadata access:
  - `getOneToManyRelations()` - expose discovered relations map

**Retained:**
- `loadEvent()` - still loads event data from ROOT files
- OneToMany relations discovery at initialization
- All branch setup logic

### 2. CollectionProcessor: Offset Application Helper

New static helper class that centralizes all offset application logic:

```cpp
class CollectionProcessor {
public:
    // Time offset application
    static void applyTimeOffset(std::vector<edm4hep::MCParticleData>&, float);
    static void applyTimeOffset(std::vector<edm4hep::SimTrackerHitData>&, float);
    static void applyTimeOffset(std::vector<edm4hep::CaloHitContributionData>&, float);
    
    // Generator status offset
    static void applyGeneratorStatusOffset(std::vector<edm4hep::MCParticleData>&, int);
    
    // Index offset for references
    static void applyIndexOffset(std::vector<podio::ObjectID>&, size_t);
    
    // Unified processing methods
    static void processMCParticles(...);
    static void processTrackerHits(...);
    static void processCaloHits(...);
    static void processCaloContributions(...);
};
```

**Key Features:**
- All methods are static (stateless)
- Uses IndexOffsetHelper for OneToMany relation offsets
- Accepts OneToMany field names as parameters
- No hardcoded collection-specific logic

### 3. StandaloneTimesliceMerger: Processing Coordinator

StandaloneTimesliceMerger now orchestrates all processing:

**Processing Flow:**
```cpp
for each data source:
    for each event:
        1. Load event: data_source->loadEvent()
        
        2. Calculate offsets:
           - time_offset = calculate based on beam distance
           - particle_index_offset = current merged size
           - Get OneToMany relations from data source
        
        3. Get raw data: 
           - particles = data_source->getMCParticles()
           - hits = data_source->getTrackerHits(name)
           - etc.
        
        4. Process with CollectionProcessor:
           - CollectionProcessor::processMCParticles(
                 particles, time_offset, status_offset,
                 index_offset, index_fields, already_merged)
           - Similar for other collection types
        
        5. Move to merged collections
```

**Benefits:**
- Single place for all processing logic
- Clear data flow: load → process → merge
- Uses OneToMany relations map throughout
- No collection type is special

## Code Changes

### DataSource.h
- Removed: `process*()` method declarations (~10 methods)
- Removed: `current_time_offset_`, `current_particle_index_offset_`, `getCurrentTimeOffset()`
- Added: Getter methods for raw collection access (~10 methods)
- Added: `getOneToManyRelations() const`
- Made public: `generateTimeOffset()`, `calculateBeamDistance()`

### DataSource.cc
- Removed: All `process*()` method implementations (~150 lines)
- No changes to: `loadEvent()`, branch setup, OneToMany discovery

### CollectionProcessor.h (New File)
- Static methods for time offset application
- Static methods for index offset application
- Static methods for generator status offset application
- Unified processing methods that combine multiple offsets
- Uses OneToMany relations via IndexOffsetHelper

### StandaloneTimesliceMerger.cc
- Updated: `createMergedTimeslice()` processing loop
- Now: Calculates time offsets locally
- Now: Gets OneToMany relations from DataSource
- Now: Uses CollectionProcessor for all offset applications
- Removed: Calls to `process*()` methods
- Added: Calls to collection getters + CollectionProcessor

## Benefits

### Clear Separation of Concerns
- **DataSource**: "I provide data"
- **CollectionProcessor**: "I apply offsets"
- **StandaloneTimesliceMerger**: "I coordinate the merging process"

### Single Source of Truth
- All processing logic in CollectionProcessor
- All OneToMany metadata in IndexOffsetHelper
- No duplication

### Easier to Maintain
- Want to change how time offsets work? → Update CollectionProcessor
- Want to add a new collection type? → Add getter to DataSource, processing to CollectionProcessor
- Want to change merging strategy? → Update StandaloneTimesliceMerger

### Fully Generic
- OneToMany relations discovered at runtime
- Passed to processing methods as parameters
- No hardcoded collection-specific code paths
- Extensible to any EDM4hep schema

### Testable
- CollectionProcessor methods are static and pure
- Easy to unit test in isolation
- DataSource can be mocked
- Clear interfaces

## Migration Path

For new collection types:

1. **Add getter to DataSource:**
   ```cpp
   std::vector<NewType>& getNewCollection(const std::string& name) {
       return *new_collection_branches_[name];
   }
   ```

2. **Add processing to CollectionProcessor (if needed):**
   ```cpp
   static void processNewCollection(
       std::vector<NewType>& data,
       float time_offset,
       size_t index_offset,
       const std::vector<std::string>& index_fields)
   {
       // Apply offsets as needed
   }
   ```

3. **Use in StandaloneTimesliceMerger:**
   ```cpp
   auto& data = data_source->getNewCollection(name);
   CollectionProcessor::processNewCollection(
       data, time_offset, index_offset, 
       one_to_many_relations[name]);
   merged_collections_.new_data[name].insert(...);
   ```

## Architecture Diagram

```
┌─────────────────────────────────────────────────────────────────┐
│                    ROOT Input Files                             │
└────────────────────────────────┬────────────────────────────────┘
                                 │
                                 ▼
┌─────────────────────────────────────────────────────────────────┐
│                       DataSource                                │
│  ┌──────────────────────────────────────────────────────────┐  │
│  │  loadEvent()                                             │  │
│  │  getMCParticles() / getTrackerHits() / getCaloHits()    │  │
│  │  getOneToManyRelations()                                 │  │
│  │  generateTimeOffset() / calculateBeamDistance()          │  │
│  └──────────────────────────────────────────────────────────┘  │
└────────────────────────────────┬────────────────────────────────┘
                                 │ Raw Data + Metadata
                                 ▼
┌─────────────────────────────────────────────────────────────────┐
│               StandaloneTimesliceMerger                         │
│  ┌──────────────────────────────────────────────────────────┐  │
│  │  1. Get raw data from DataSource                         │  │
│  │  2. Get OneToMany relations                              │  │
│  │  3. Calculate offsets                                    │  │
│  │  4. Call CollectionProcessor                             │  │
│  │  5. Merge results                                        │  │
│  └──────────────────────────────────────────────────────────┘  │
└────────────────────────────────┬────────────────────────────────┘
                                 │
                                 ▼
┌─────────────────────────────────────────────────────────────────┐
│                  CollectionProcessor                            │
│  ┌──────────────────────────────────────────────────────────┐  │
│  │  applyTimeOffset()                                       │  │
│  │  applyIndexOffset()                                      │  │
│  │  applyGeneratorStatusOffset()                            │  │
│  │                                                          │  │
│  │  Uses: IndexOffsetHelper (pointer-to-member)            │  │
│  │        OneToMany relations map                           │  │
│  └──────────────────────────────────────────────────────────┘  │
└─────────────────────────────────────────────────────────────────┘
```

## Commits

- **7d170f5**: Refactor DataSource to provide only raw collection access, remove all process methods
- **496359f**: Move all collection processing logic to StandaloneTimesliceMerger using CollectionProcessor

## See Also

- `RUNTIME_DISCOVERY.md` - Runtime discovery of OneToMany relations
- `GENERIC_OFFSET_APPLICATION.md` - Generic offset application with pointer-to-member
- `REFACTORING_INDEX_OFFSETS.md` - Original index offset refactoring
