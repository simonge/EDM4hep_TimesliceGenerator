# Code Simplification Approach

## Overview
The current code in the `main` branch uses type-specific processing for each collection type (MCParticles, TrackerHits, CaloHits, etc.). This document outlines a more generic, data-driven approach.

## Current Approach (Hardcoded)

### Problems:
1. **Type-specific methods**: Separate `processMCParticles()`, `processTrackerHits()`, `processCaloHits()`, etc.
2. **Hardcoded branch names**: E.g., `"_MCParticles_parents"`, `"_MCParticles_daughters"`
3. **Repetitive code**: Similar logic repeated for each collection type
4. **Not extensible**: Adding new collection types requires code changes

### Example from current code:
```cpp
// Hardcoded for MCParticles
std::vector<edm4hep::MCParticleData>& processMCParticles(...);
// Hardcoded for TrackerHits  
std::vector<edm4hep::SimTrackerHitData>& processTrackerHits(...);
// Hardcoded for CaloHits
std::vector<edm4hep::SimCalorimeterHitData>& processCaloHits(...);
```

## Simplified Approach (Generic)

### Key Principles:
1. **Discover branches from data**: Loop over all branches in first entry
2. **Build metadata map**: Branch name → type information
3. **Introspect types**: Identify vector members and ObjectID relations automatically
4. **Generic processing**: Single loop that works for all types

### Implementation Steps:

#### 1. Branch Discovery
```cpp
struct BranchMetadata {
    std::string name;
    std::string type_name;
    TClass* type_class;
    bool is_vector;
    bool is_objectid_collection;
    std::vector<std::string> related_branches; // e.g., "_BranchName_particle"
};

std::map<std::string, BranchMetadata> discoverBranches(TTree* tree) {
    std::map<std::string, BranchMetadata> branches;
    
    TObjArray* branch_list = tree->GetListOfBranches();
    for (int i = 0; i < branch_list->GetEntries(); ++i) {
        TBranch* branch = (TBranch*)branch_list->At(i);
        BranchMetadata meta;
        meta.name = branch->GetName();
        
        TClass* expectedClass = nullptr;
        EDataType expectedType;
        branch->GetExpectedType(expectedClass, expectedType);
        
        if (expectedClass) {
            meta.type_class = expectedClass;
            meta.type_name = expectedClass->GetName();
            meta.is_vector = meta.type_name.find("vector<") == 0;
            meta.is_objectid_collection = meta.type_name.find("vector<podio::ObjectID>") == 0;
        }
        
        branches[meta.name] = meta;
    }
    
    return branches;
}
```

#### 2. Identify Relationships
```cpp
void identifyRelationships(std::map<std::string, BranchMetadata>& branches) {
    for (auto& [name, meta] : branches) {
        // Skip reference branches (start with "_")
        if (name[0] == '_') continue;
        
        // Find related reference branches
        // Convention: "_BranchName_relationName" is a reference from BranchName
        for (auto& [ref_name, ref_meta] : branches) {
            if (ref_name[0] != '_') continue;
            if (ref_name.find("_" + name + "_") == 0 || 
                ref_name == "_" + name + "_particle" ||
                ref_name == "_" + name + "_parents" ||
                ref_name == "_" + name + "_daughters") {
                meta.related_branches.push_back(ref_name);
            }
        }
    }
}
```

#### 3. Generic Processing
```cpp
void processGenericBranch(const BranchMetadata& meta, 
                          void* branch_data,
                          size_t index_offset,
                          float time_offset,
                          bool apply_time_offset,
                          MergedCollections& output) {
    
    // Generic processing based on type
    if (meta.is_objectid_collection) {
        // Update ObjectID indices
        auto* refs = static_cast<std::vector<podio::ObjectID>*>(branch_data);
        for (auto& ref : *refs) {
            ref.index += index_offset;
        }
    }
    else if (meta.type_name.find("edm4hep::") != std::string::npos) {
        // Generic EDM4hep data processing
        // Use TClass to access 'time' member if it exists
        if (apply_time_offset && meta.type_class) {
            TDataMember* time_member = meta.type_class->GetDataMember("time");
            if (time_member) {
                // Apply time offset using ROOT's introspection
                // (This would need proper implementation using TClass methods)
            }
        }
    }
}
```

#### 4. Simplified Main Loop
```cpp
void createMergedTimeslice(...) {
    // Clear output collections
    merged_collections_.clear();
    
    // For each source
    for (auto& source : sources) {
        // For each event from this source
        for (size_t i = 0; i < source->getEntriesNeeded(); ++i) {
            source->loadEvent(source->getCurrentEntryIndex());
            
            // Process ALL branches generically
            for (auto& [branch_name, meta] : branch_metadata_) {
                if (branch_name[0] == '_') continue; // Skip reference branches
                
                void* branch_data = source->getBranchData(branch_name);
                
                // Generic processing
                processGenericBranch(meta, branch_data, 
                                   particle_offset, time_offset,
                                   should_apply_time, merged_collections_);
                
                // Process related reference branches
                for (const auto& ref_branch : meta.related_branches) {
                    void* ref_data = source->getBranchData(ref_branch);
                    processGenericBranch(branch_metadata_[ref_branch], 
                                       ref_data, particle_offset, 0.0, 
                                       false, merged_collections_);
                }
            }
            
            source->setCurrentEntryIndex(source->getCurrentEntryIndex() + 1);
        }
    }
}
```

## Benefits of Generic Approach

1. **Extensibility**: New collection types work automatically
2. **Maintainability**: Single code path for all types
3. **Correctness**: Consistent handling reduces bugs
4. **Clarity**: Intent is clearer with less code

## Trade-offs

1. **Complexity**: Generic code can be harder to understand initially
2. **Performance**: Runtime type checks vs compile-time type safety
3. **Debugging**: Type-specific code can be easier to debug

## Recommendation

For this codebase, a hybrid approach works best:
- Use generic branch discovery
- Use generic ObjectID processing
- Keep type-specific handling for special cases (e.g., MCParticles with time offset calculation)
- Minimize code duplication through templating or macros

## Next Steps

To implement this simplification:
1. Create `BranchMetadata` structure
2. Implement `discoverBranches()` function
3. Refactor `DataSource` to use generic branch access
4. Simplify `StandaloneTimesliceMerger::createMergedTimeslice()`
5. Test with existing data files
