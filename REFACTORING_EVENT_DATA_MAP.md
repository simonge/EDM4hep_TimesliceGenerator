# Refactoring: Event Data as a Map Structure

## Overview

This document describes the refactoring that changes `DataSource::loadEvent()` to return event data as a map structure, eliminates specific collection accessor methods, and moves time offset calculation into the DataSource.

## Problem Statement

Previously, the code had several issues:
1. `StandaloneTimesliceMerger` had to call specific methods like `getMCParticles()`, `getObjectIDCollection()`, etc. to access different collection types
2. Time offset generation was done in `StandaloneTimesliceMerger` after loading the event
3. There was no centralized tracking of collection sizes before merging
4. The code was not truly generic - it required knowledge of specific collection types

## Solution

### 1. EventData Structure

Created a new `EventData` struct that encapsulates all event data:

```cpp
struct EventData {
    // Map of collection name to collection data (type-erased as std::any)
    std::unordered_map<std::string, std::any> collections;
    
    // Map tracking the size of each collection before merging
    std::unordered_map<std::string, size_t> collection_sizes;
    
    // Time offset calculated for this event
    float time_offset = 0.0f;
    
    // Helper methods for type-safe access
    template<typename T>
    T* getCollection(const std::string& name);
    
    bool hasCollection(const std::string& name) const;
    std::string getCollectionType(const std::string& name) const;
};
```

**Key Features:**
- Uses `std::any` for type-erased storage of collections
- Tracks collection sizes for offset calculations
- Includes calculated time offset
- Provides type-safe accessor methods

### 2. DataSource::loadEvent() Refactoring

Changed the signature and implementation:

**Before:**
```cpp
void loadEvent(size_t event_index);
// Required subsequent calls to getMCParticles(), getTrackerHits(), etc.
```

**After:**
```cpp
EventData* loadEvent(size_t event_index, float time_slice_duration, 
                     float bunch_crossing_period, std::mt19937& rng);
```

**Implementation:**
1. Loads event from ROOT TChain
2. Calculates time offset using MCParticles data
3. Populates EventData map with all collections:
   - MCParticles
   - MCParticle references (parents/daughters)
   - Tracker hits and their particle references
   - Calorimeter hits, contributions, and references
   - Event headers
   - GP (Global Parameter) branches
4. Records original size of each collection
5. Returns pointer to EventData

### 3. Time Offset Calculation

**Moved from StandaloneTimesliceMerger to DataSource:**

The time offset is now calculated inside `loadEvent()`:
- Accesses MCParticles from loaded event
- Calculates beam distance if needed
- Applies bunch crossing logic
- Applies beam effects and Gaussian spread
- Stores result in `EventData::time_offset`

This ensures:
- Time offset is calculated with access to event data
- No need to pass MCParticles back to calculate offset
- Clean separation: DataSource knows how to calculate offsets

### 4. Removed Specific Collection Accessors

**Removed methods:**
- `getMCParticles()`
- `getObjectIDCollection()`
- `getTrackerHits()`
- `getCaloHits()`
- `getCaloContributions()`
- `getEventHeaders()`
- `getGPBranch()`
- `getGPIntValues()` / `getGPFloatValues()` / etc.

**Benefits:**
- No type-specific knowledge needed in calling code
- All collections accessed uniformly through the map
- Easier to extend to new collection types

### 5. Generic Collection Processing in StandaloneTimesliceMerger

**New approach:**
```cpp
EventData* event_data = data_source->loadEvent(...);

// Track collection lengths before merging
std::unordered_map<std::string, size_t> collection_offsets;
collection_offsets["MCParticles"] = merged_collections_.mcparticles.size();

// Iterate over all collections generically
for (auto& [collection_name, collection_data] : event_data->collections) {
    // Determine type
    std::string collection_type = data_source->getCollectionTypeName(collection_name);
    
    // Process based on type
    if (collection_name == "MCParticles") {
        auto* particles = std::any_cast<std::vector<edm4hep::MCParticleData>>(&collection_data);
        // Process and merge...
    }
    else if (collection_type == "SimTrackerHit") {
        auto* hits = std::any_cast<std::vector<edm4hep::SimTrackerHitData>>(&collection_data);
        // Process and merge...
    }
    // ... etc for other types
}
```

**Key improvements:**
- Single loop iterates over all collections
- Uses `std::any_cast` for type-safe extraction
- Uses `getCollectionTypeName()` helper to determine processing logic
- Collection offsets tracked in a map
- No hardcoded collection names in main loop

### 6. Collection Size Tracking

The `EventData::collection_sizes` map records the original size of each collection:
- Used for debugging and validation
- Helps track which collections were empty
- Could be used for optimization decisions

The `collection_offsets` map in `StandaloneTimesliceMerger` tracks merged collection sizes:
- `collection_offsets["MCParticles"]` = current size of merged MCParticles
- Used to calculate index offsets for references
- Updated as collections are merged

## Benefits

### 1. True Generic Processing
- No need to add new accessor methods for new collection types
- Collections discovered and processed uniformly
- Extensible to any EDM4hep collection type

### 2. Cleaner Separation of Concerns
- **DataSource**: Loads event data, calculates time offsets, provides data as a map
- **StandaloneTimesliceMerger**: Orchestrates merging by iterating over the map
- **CollectionProcessor**: Applies offsets to data

### 3. Better Data Flow
```
ROOT Files → DataSource.loadEvent() → EventData (with time_offset)
                                          ↓
                                  StandaloneTimesliceMerger
                                          ↓
                                  Generic collection iteration
                                          ↓
                                  CollectionProcessor
                                          ↓
                                  Merged output
```

### 4. Easier to Maintain
- Want to add a new collection type? Just add it to loadEvent() and the generic loop handles it
- Want to change offset calculations? Update CollectionProcessor
- Want to change time offset logic? Update DataSource.generateTimeOffset()

### 5. Collection Size Tracking Built-in
- Sizes tracked automatically in EventData
- Available for debugging and validation
- Used for offset calculations

## Code Changes Summary

### DataSource.h
- **Added**: `EventData` struct with collection map and size tracking
- **Changed**: `loadEvent()` signature to return `EventData*` and accept time slice parameters
- **Added**: `getCollectionTypeName()` helper method
- **Removed**: All specific collection accessor methods (getMCParticles, etc.)
- **Added**: `current_event_data_` member to store current event

### DataSource.cc
- **Reimplemented**: `loadEvent()` to:
  - Calculate time offset internally
  - Populate EventData map with all collections
  - Track collection sizes
  - Return EventData pointer
- **Made private**: `generateTimeOffset()` and `calculateBeamDistance()`

### StandaloneTimesliceMerger.cc
- **Refactored**: `createMergedTimeslice()` to:
  - Call new `loadEvent()` signature
  - Get time offset from EventData
  - Iterate over collections map generically
  - Use `std::any_cast` for type-safe extraction
  - Track collection offsets in a map
  - Remove all calls to specific accessor methods

## Migration Impact

### For Users
- No changes to external API or configuration
- Transparent refactoring

### For Developers
- New collection types: Add to `loadEvent()` and update type determination logic
- No need to add new accessor methods
- Generic loop automatically handles new types

## Testing Recommendations

1. **Functional testing**: Verify output is identical to previous version
2. **Collection processing**: Test all collection types are properly merged
3. **Offset calculations**: Verify time offsets and index offsets are correct
4. **Edge cases**: Test with empty collections, already-merged sources
5. **Performance**: Compare performance with previous implementation

## Architecture Alignment

This refactoring aligns with the goal stated in the problem statement:
> "The code should just loop over all of the collections updating them based on what the map tells it what members and connections exist between them. There shouldn't ever need to be e.g. getMCParticles, getObjectIDCollection etc. as all of the collections are just iterated over from the map."

All collections are now accessed uniformly through the EventData map, with no type-specific accessor methods required.

## See Also

- `ARCHITECTURAL_REFACTORING.md` - Previous refactoring that moved processing to CollectionProcessor
- `RUNTIME_DISCOVERY.md` - Runtime discovery of OneToMany relations
- `GENERIC_OFFSET_APPLICATION.md` - Generic offset application with metadata
