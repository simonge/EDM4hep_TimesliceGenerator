# Simplified Architecture Diagram

## Data Flow Overview

```
┌─────────────────────────────────────────────────────────────────┐
│                        USER INPUT                                │
│  Config File (YAML) + Command Line Arguments                    │
└────────────────┬────────────────────────────────────────────────┘
                 │
                 ▼
┌─────────────────────────────────────────────────────────────────┐
│            timeslice_merger_main.cc                             │
│  • Parse config and CLI                                         │
│  • Create MergerConfig                                          │
└────────────────┬────────────────────────────────────────────────┘
                 │
                 ▼
┌─────────────────────────────────────────────────────────────────┐
│         StandaloneTimesliceMerger (Main Orchestrator)           │
│                                                                  │
│  ┌──────────────────────────────────────────────────────────┐  │
│  │  1. initializeDataSources()                              │  │
│  │     • Create DataSource for each input                   │  │
│  │     • Discover branch names & types from first source    │  │
│  │       - discoverCollectionNames("SimTrackerHit")         │  │
│  │       - discoverCollectionNames("SimCalorimeterHit")     │  │
│  │       - discoverGPBranches()                             │  │
│  │     • Uses ROOT's TBranch::GetExpectedType() directly    │  │
│  │     • Simple pattern matching on type strings            │  │
│  └──────────────────────────────────────────────────────────┘  │
│                                                                  │
│  ┌──────────────────────────────────────────────────────────┐  │
│  │  2. setupOutputTree()                                    │  │
│  │     • Create output branches for all discovered types    │  │
│  │     • MCParticles, EventHeader, SubEventHeaders          │  │
│  │     • TrackerHits[name] for each discovered tracker      │  │
│  │     • CaloHits[name] for each discovered calo            │  │
│  │     • GP branches for parameters                         │  │
│  └──────────────────────────────────────────────────────────┘  │
│                                                                  │
│  ┌──────────────────────────────────────────────────────────┐  │
│  │  3. Main Processing Loop                                 │  │
│  │                                                           │  │
│  │     for (each timeslice) {                               │  │
│  │       updateInputNEvents()  // Poisson or static         │  │
│  │       createMergedTimeslice() {                          │  │
│  │         for (each data source) {                         │  │
│  │           for (each event needed) {                      │  │
│  │             • loadEvent() → EventData map                │  │
│  │             • Track offsets (MCParticles, refs, etc)     │  │
│  │             for (each collection in event) {             │  │
│  │               ┌─────────────────────────────────┐        │  │
│  │               │ processCollectionGeneric()      │        │  │
│  │               │  • C++20 concepts detect fields │        │  │
│  │               │  • Apply time offsets           │        │  │
│  │               │  • Apply index offsets          │        │  │
│  │               │  • Apply gen status offsets     │        │  │
│  │               └─────────────────────────────────┘        │  │
│  │               ┌─────────────────────────────────┐        │  │
│  │               │ Direct Type Matching Merge      │        │  │
│  │               │  if (name == "MCParticles")     │        │  │
│  │               │    merged.mcparticles.insert()  │        │  │
│  │               │  else if (type == "TrackerHit") │        │  │
│  │               │    merged.tracker[name].insert()│        │  │
│  │               │  // ... etc for all types       │        │  │
│  │               └─────────────────────────────────┘        │  │
│  │             }                                             │  │
│  │           }                                               │  │
│  │         }                                                 │  │
│  │         writeTimesliceToTree()                           │  │
│  │       }                                                   │  │
│  │     }                                                     │  │
│  └──────────────────────────────────────────────────────────┘  │
└────────────────┬────────────────────────────────────────────────┘
                 │
                 ▼
┌─────────────────────────────────────────────────────────────────┐
│                      OUTPUT FILE                                 │
│  ROOT file with merged timeslices                               │
└─────────────────────────────────────────────────────────────────┘
```

## Component Details

### DataSource (Input Management)
```
┌─────────────────────────────────────┐
│           DataSource                │
│                                     │
│  • TChain for multiple input files │
│  • Branch pointers for each type   │
│    - MCParticles                   │
│    - TrackerHits[name]             │
│    - CaloHits[name]                │
│    - ObjectID refs                 │
│    - GP branches                   │
│  • loadEvent() returns EventData   │
│    (map of collection name → data) │
│  • Calculates time offsets         │
└─────────────────────────────────────┘
```

### CollectionTypeTraits (C++20 Concepts)
```
┌──────────────────────────────────────────┐
│      CollectionTypeTraits.h              │
│                                          │
│  Compile-time field detection:          │
│                                          │
│  concept HasTime<T>                      │
│    → Detects .time field                │
│                                          │
│  concept HasGeneratorStatus<T>          │
│    → Detects .generatorStatus field     │
│                                          │
│  concept HasDaughters<T>                │
│    → Detects .daughters_begin/end       │
│                                          │
│  concept HasParents<T>                  │
│    → Detects .parents_begin/end         │
│                                          │
│  concept HasContributions<T>            │
│    → Detects .contributions_begin/end   │
│                                          │
│  processCollectionGeneric()             │
│    if constexpr (HasTime<T>)            │
│      item.time += offset;               │
│    if constexpr (HasDaughters<T>)       │
│      item.daughters_begin += offset;    │
│    // ... etc                           │
└──────────────────────────────────────────┘
```

### MergedCollections (Output Storage)
```
┌─────────────────────────────────────────┐
│        MergedCollections                │
│                                         │
│  Fixed collections:                     │
│    • mcparticles                        │
│    • event_headers                      │
│    • sub_event_headers                  │
│    • mcparticle_parents_refs            │
│    • mcparticle_children_refs           │
│                                         │
│  Dynamic collections (maps):            │
│    • tracker_hits[collection_name]      │
│    • calo_hits[collection_name]         │
│    • calo_contributions[collection]     │
│    • tracker_hit_particle_refs[name]    │
│    • calo_contrib_particle_refs[name]   │
│    • calo_hit_contributions_refs[name]  │
│                                         │
│  GP (Global Parameters):                │
│    • gp_key_branches[name]              │
│    • gp_int_values                      │
│    • gp_float_values                    │
│    • gp_double_values                   │
│    • gp_string_values                   │
└─────────────────────────────────────────┘
```

## Type Matching Logic (Simplified)

```cpp
// Direct if/else chain - all in one place
for (auto& [collection_name, collection_data] : event_data->collections) {
    
    // 1. Process fields (C++20 concepts detect which fields exist)
    processCollectionGeneric(collection_data, collection_type, 
                           time_offset, gen_status_offset, 
                           field_offsets, already_merged);
    
    // 2. Merge based on name/type
    if (collection_name == "MCParticles") {
        auto* coll = std::any_cast<vector<MCParticleData>>(&data);
        merged.mcparticles.insert(merged.mcparticles.end(), 
                                 coll->begin(), coll->end());
    }
    else if (collection_name == "_MCParticles_parents") {
        auto* coll = std::any_cast<vector<ObjectID>>(&data);
        merged.mcparticle_parents_refs.insert(...);
    }
    else if (collection_name == "_MCParticles_daughters") {
        auto* coll = std::any_cast<vector<ObjectID>>(&data);
        merged.mcparticle_children_refs.insert(...);
    }
    else if (collection_type == "SimTrackerHit") {
        auto* coll = std::any_cast<vector<SimTrackerHitData>>(&data);
        merged.tracker_hits[collection_name].insert(...);
    }
    else if (collection_type == "SimCalorimeterHit") {
        auto* coll = std::any_cast<vector<SimCalorimeterHitData>>(&data);
        merged.calo_hits[collection_name].insert(...);
    }
    // ... etc for all types
}
```

## Key Simplifications

### ✅ Direct ROOT API Usage
- No registry lookup for branch types
- Use `TBranch::GetExpectedType()` directly
- Simple string matching on type names

### ✅ C++20 Concepts for Field Detection
- Compile-time detection of field names
- No runtime metadata needed
- Automatic offset application

### ✅ Direct Type Matching
- Single if/else chain in one place
- No function pointer indirection
- Clear control flow

### ✅ Standard C++ Patterns
- `std::vector::insert()` for merging
- `std::any_cast` for type recovery
- Maps for dynamic collections

## Files and Line Counts

```
Core Logic:        777 lines (Merger.h/cc)
Input Management:  550 lines (DataSource.h/cc)
Type Traits:       180 lines (CollectionTypeTraits.h)
Configuration:      42 lines (MergerConfig.h)
CLI Application:   421 lines (main.cc)
Tests:             472 lines (3 test files)
Supporting:        465 lines (IndexOffsetHelper.h)
─────────────────────────────
Total:           2,907 lines

Registry code:       0 lines (removed 930 lines!)
```

## Benefits of This Architecture

1. **Simple** - Direct control flow, no indirection
2. **Clear** - All merge logic in one place
3. **Maintainable** - Easy to trace and debug
4. **Extensible** - Add types with one if/else case
5. **Standard** - Uses ROOT API and C++20 directly
6. **Efficient** - No registry lookup overhead
7. **Type-safe** - C++20 concepts ensure correctness
