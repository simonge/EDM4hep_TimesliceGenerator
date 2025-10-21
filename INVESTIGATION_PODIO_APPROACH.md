# Investigation: Using Podio's RelationNameMapping for Branch Name Resolution

## Overview

This investigation explores using podio's built-in `RelationNameMapping` system to dynamically obtain the names of vector and association members of EDM4hep objects, rather than using hardcoded strings or C++ preprocessor macros.

## Background: Podio's DatamodelRegistry

Podio provides a global registry system (`DatamodelRegistry`) that stores metadata about datamodels, including the names of relations and vector members for each datatype. This is populated at code generation time from YAML datamodel definitions.

### Key Components

#### 1. RelationNameMapping Type

```cpp
using RelationNameMapping = std::vector<
    std::tuple<
        std::string_view,                    // Datatype name (e.g., "MCParticle")
        std::vector<std::string_view>,       // Relation names (OneToMany before OneToOne)
        std::vector<std::string_view>        // VectorMember names
    >
>;
```

This type alias defines the structure for storing all relations and vector members for all datatypes in an EDM.

#### 2. RelationNames Structure

```cpp
struct RelationNames {
    const std::vector<std::string_view>& relations;       // Names of relations
    const std::vector<std::string_view>& vectorMembers;   // Names of vector members
};
```

This structure provides access to the relation and vector member names for a specific datatype.

#### 3. DatamodelRegistry API

```cpp
class DatamodelRegistry {
public:
    static const DatamodelRegistry& instance();
    
    // Register a datamodel with its relation name mapping
    size_t registerDatamodel(
        std::string name,
        std::string_view definition,
        const podio::RelationNameMapping& relationNames
    );
    
    // Get relation and vector member names for a datatype
    RelationNames getRelationNames(std::string_view typeName) const;
};
```

## How It Works

### At Code Generation Time

When podio generates code from a YAML datamodel definition, it creates:

1. **Static vectors** containing the relation and vector member names for each datatype
2. **Registration code** that calls `registerDatamodel()` to populate the registry

### At Runtime

The application can query the registry to get relation/vector member names:

```cpp
// Get the registry instance
const auto& registry = podio::DatamodelRegistry::instance();

// Get relation names for MCParticle
auto relationNames = registry.getRelationNames("MCParticle");

// Access the names
for (const auto& relName : relationNames.relations) {
    std::cout << "Relation: " << relName << std::endl;
    // Output: "parents", "daughters", etc.
}
```

## Application to Branch Name Construction

### Current Problem

The codebase currently uses hardcoded strings to construct ROOT branch names:

```cpp
// Hardcoded approach
std::string parents_branch = "_MCParticles_parents";
std::string tracker_ref = "_" + coll_name + "_particle";
std::string calo_contrib = "_" + coll_name + "_contributions";
```

### Podio-Based Solution

Instead of hardcoding, we can query podio's registry:

```cpp
// Get relation names from podio's registry
const auto& registry = podio::DatamodelRegistry::instance();
auto mcParticleRelations = registry.getRelationNames("MCParticle");

// Construct branch names using the relation names from podio
for (const auto& relName : mcParticleRelations.relations) {
    std::string branchName = "_MCParticles_" + std::string(relName);
    // Result: "_MCParticles_parents", "_MCParticles_daughters", etc.
}
```

### For Collection-Specific Relations

```cpp
// For tracker hits with "particle" relation
auto trackerHitRelations = registry.getRelationNames("SimTrackerHit");
for (const auto& relName : trackerHitRelations.relations) {
    std::string branchName = "_" + collectionName + "_" + std::string(relName);
    // Result: "_VXDTrackerHits_particle", etc.
}

// For calorimeter hits with "contributions" relation
auto caloHitRelations = registry.getRelationNames("SimCalorimeterHit");
for (const auto& relName : caloHitRelations.relations) {
    std::string branchName = "_" + collectionName + "_" + std::string(relName);
    // Result: "_ECalBarrelHits_contributions", etc.
}
```

## Advantages of the Podio Approach

### 1. **True Data Model Driven**
- Information comes directly from the EDM4hep YAML definition
- No manual synchronization needed
- Guaranteed to match the actual datamodel

### 2. **No Code Generation Required**
- No need to maintain separate macro definitions
- Automatically stays in sync with EDM4hep updates
- Works with any EDM4hep version that uses podio

### 3. **Runtime Flexibility**
- Can adapt to different EDM4hep versions at runtime
- No recompilation needed if datamodel changes (only relinking)
- Can handle multiple datamodel versions simultaneously

### 4. **Standard Podio Pattern**
- Uses the same mechanism other podio-based tools use
- Follows established patterns in the podio ecosystem
- Better integration with podio tooling

### 5. **Clean API**
- Simple, straightforward interface
- Type-safe (returns `string_view`)
- No macro magic or complex template metaprogramming

## Implementation Considerations

### Availability of RelationNameMapping

The `RelationNameMapping` must be registered before we can query it. This happens when:

1. EDM4hep shared libraries are loaded
2. The registration code (generated by podio) executes
3. The registry is populated with metadata

For our timeslice merger application:

```cpp
// Ensure EDM4hep libraries are loaded (usually automatic via TChain)
// Then query the registry
const auto& registry = podio::DatamodelRegistry::instance();
```

### Branch Name Convention

Podio's registry provides member names (e.g., "parents", "daughters", "particle", "contributions"), but doesn't prescribe the branch naming convention. The convention of prepending "_" and the collection name is still our responsibility:

```cpp
// Convention: _<CollectionName>_<memberName>
std::string branchName = "_" + collectionName + "_" + std::string(memberName);
```

### Helper Functions

We can create helper functions that encapsulate the podio registry queries:

```cpp
#include <podio/DatamodelRegistry.h>

class EDM4hepBranchNames {
public:
    // Get MCParticle relation branch names
    static std::vector<std::string> getMCParticleRelationBranches() {
        const auto& registry = podio::DatamodelRegistry::instance();
        auto relations = registry.getRelationNames("MCParticle");
        
        std::vector<std::string> branches;
        for (const auto& relName : relations.relations) {
            branches.push_back("_MCParticles_" + std::string(relName));
        }
        return branches;
    }
    
    // Get relation branch names for a specific collection and type
    static std::vector<std::string> getRelationBranches(
        const std::string& collectionName,
        const std::string& typeName
    ) {
        const auto& registry = podio::DatamodelRegistry::instance();
        auto relations = registry.getRelationNames(typeName);
        
        std::vector<std::string> branches;
        for (const auto& relName : relations.relations) {
            branches.push_back("_" + collectionName + "_" + std::string(relName));
        }
        return branches;
    }
};
```

## Comparison: Macro vs Podio Approach

| Aspect | Macro Approach | Podio Approach |
|--------|---------------|----------------|
| **Source of Truth** | Manually defined macros | EDM4hep YAML definition |
| **Synchronization** | Manual | Automatic |
| **Runtime Flexibility** | None (compile-time) | Full (runtime queries) |
| **Complexity** | Low (simple macros) | Low (simple API calls) |
| **Integration** | Standalone | Part of podio ecosystem |
| **Maintenance** | Manual updates needed | Automatic with EDM4hep |
| **Type Safety** | String literals | `string_view` from registry |
| **Compile-time** | Yes | No (but minimal runtime cost) |
| **EDM4hep Updates** | Requires code changes | Works automatically |

## Example: Refactored DataSource.cc

### Before (Hardcoded)
```cpp
void DataSource::setupMCParticleBranches() {
    mcparticle_branch_ = new std::vector<edm4hep::MCParticleData>();
    chain_->SetBranchAddress("MCParticles", &mcparticle_branch_);

    std::string parents_branch = "_MCParticles_parents";
    std::string daughters_branch = "_MCParticles_daughters";
    
    objectid_branches_[parents_branch] = new std::vector<podio::ObjectID>();
    objectid_branches_[daughters_branch] = new std::vector<podio::ObjectID>();
    
    chain_->SetBranchAddress(parents_branch.c_str(), &objectid_branches_[parents_branch]);
    chain_->SetBranchAddress(daughters_branch.c_str(), &objectid_branches_[daughters_branch]);
}
```

### After (Podio-Based)
```cpp
void DataSource::setupMCParticleBranches() {
    mcparticle_branch_ = new std::vector<edm4hep::MCParticleData>();
    chain_->SetBranchAddress("MCParticles", &mcparticle_branch_);

    // Get relation names from podio's datamodel registry
    const auto& registry = podio::DatamodelRegistry::instance();
    auto mcParticleInfo = registry.getRelationNames("MCParticle");
    
    // Setup branches for each relation dynamically
    for (const auto& relName : mcParticleInfo.relations) {
        std::string branch_name = "_MCParticles_" + std::string(relName);
        objectid_branches_[branch_name] = new std::vector<podio::ObjectID>();
        chain_->SetBranchAddress(branch_name.c_str(), &objectid_branches_[branch_name]);
    }
}
```

### For Collection-Specific Relations
```cpp
void DataSource::setupTrackerBranches() {
    const auto& registry = podio::DatamodelRegistry::instance();
    auto trackerHitInfo = registry.getRelationNames("SimTrackerHit");
    
    for (const auto& coll_name : *tracker_collection_names_) {
        tracker_hit_branches_[coll_name] = new std::vector<edm4hep::SimTrackerHitData>();
        chain_->SetBranchAddress(coll_name.c_str(), &tracker_hit_branches_[coll_name]);
        
        // Setup relation branches dynamically
        for (const auto& relName : trackerHitInfo.relations) {
            std::string ref_branch_name = "_" + coll_name + "_" + std::string(relName);
            objectid_branches_[ref_branch_name] = new std::vector<podio::ObjectID>();
            chain_->SetBranchAddress(ref_branch_name.c_str(), &objectid_branches_[ref_branch_name]);
        }
    }
}
```

## Potential Issues and Solutions

### Issue 1: Registry Not Populated

**Problem**: If EDM4hep libraries aren't loaded, the registry won't have the metadata.

**Solution**: Ensure libraries are loaded before querying:
```cpp
// ROOT's TChain automatically loads necessary libraries when opening files
// But we can also explicitly check:
auto info = registry.getRelationNames("MCParticle");
if (info.relations.empty() && info.vectorMembers.empty()) {
    std::cerr << "Warning: MCParticle not found in registry. "
              << "EDM4hep libraries may not be loaded." << std::endl;
}
```

### Issue 2: Type Name Matching

**Problem**: Registry expects exact type names (e.g., "MCParticle" not "MCParticleData").

**Solution**: Use consistent naming or strip suffixes:
```cpp
// Registry automatically handles "Collection" suffix
// "MCParticleCollection" -> looks up "MCParticle"
auto info = registry.getRelationNames("MCParticleCollection");

// For Data types, we need to strip "Data" suffix ourselves if needed
std::string typeName = "MCParticleData";
if (typeName.ends_with("Data")) {
    typeName = typeName.substr(0, typeName.length() - 4);
}
auto info = registry.getRelationNames(typeName);
```

### Issue 3: Branch Ordering

**Problem**: We might rely on specific ordering of relations.

**Solution**: Registry provides relations in a specific order (OneToMany before OneToOne), which matches the YAML definition order. For precise control:
```cpp
auto relations = registry.getRelationNames("MCParticle");
// relations.relations[0] will be the first OneToManyRelation
// This is typically "parents" for MCParticle, "daughters" is second
```

## Testing Strategy

1. **Unit Tests**: Verify registry queries return expected relation names
2. **Integration Tests**: Ensure branch setup works with real ROOT files
3. **Regression Tests**: Compare results with hardcoded approach
4. **Version Tests**: Test with different EDM4hep versions

## Recommendation

**✅ ADOPT THE PODIO APPROACH**

The podio `RelationNameMapping` approach is superior to the macro-based approach because:

1. **Truly data-model driven**: Information comes directly from EDM4hep's definition
2. **Zero maintenance**: Automatically stays in sync with EDM4hep updates
3. **Standard pattern**: Uses the same mechanism other podio tools use
4. **Simple and clean**: Straightforward API without macro complexity
5. **Runtime flexibility**: Can handle different EDM4hep versions

While the macro approach works, the podio approach is cleaner, more maintainable, and better integrated with the podio ecosystem. It's the "right way" to solve this problem in a podio-based application.

## Next Steps

1. Implement helper functions that wrap podio registry queries
2. Refactor DataSource.cc to use the podio-based approach
3. Add tests to verify correct branch name resolution
4. Document the usage pattern for future developers
5. Consider contributing improvements back to podio if gaps are found

## References

- Podio DatamodelRegistry: `/tmp/podio/include/podio/DatamodelRegistry.h`
- Podio Documentation: https://github.com/AIDASoft/podio
- EDM4hep Datamodel: https://github.com/key4hep/EDM4hep
