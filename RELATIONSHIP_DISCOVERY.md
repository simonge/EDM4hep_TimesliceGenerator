# Automatic Relationship Discovery

## Overview

This document explains the automatic branch relationship discovery system that replaces the previous hardcoded approach for handling EDM4hep data relationships.

## Problem Statement

The original implementation used hardcoded patterns to identify relationship branches:
- MCParticle relationships: `_MCParticles_parents`, `_MCParticles_daughters`
- Tracker hit particle references: `_<CollectionName>_particle`
- Calorimeter hit contributions: `_<CollectionName>_contributions`
- Contribution particle references: `_<CollectionName>Contributions_particle`

This hardcoding made the code:
- Brittle and difficult to maintain
- Unable to handle new collection types without code changes
- Prone to errors if naming conventions changed
- Not aligned with podio's design philosophy

## Solution: BranchRelationshipMapper

The `BranchRelationshipMapper` class automatically discovers collection relationships by analyzing the ROOT TTree branch structure.

### How It Works

1. **Branch Analysis**: The mapper scans all branches in the input ROOT file
2. **Collection Identification**: It identifies object data branches (e.g., `MCParticles`, `VertexBarrelHits`)
3. **Relationship Discovery**: It finds relationship branches (those starting with `_` and containing `ObjectID`)
4. **Mapping Creation**: It creates a mapping between collections and their relationships

### Branch Naming Convention

The system relies on podio's naming convention:
- **Object branches**: Collection name (e.g., `MCParticles`, `VertexBarrelHits`)
- **Relationship branches**: `_<CollectionName>_<relationName>` (e.g., `_MCParticles_parents`)
- **Data types**: Contain `edm4hep::*Data` for objects, `ObjectID` for relationships

### Key Classes and Structures

#### RelationshipInfo
```cpp
struct RelationshipInfo {
    std::string relation_branch_name;  // Full branch name
    std::string relation_name;         // Extracted relation name
    std::string target_collection;     // Referenced collection
    bool is_one_to_many;              // Relationship cardinality
};
```

#### CollectionRelationships
```cpp
struct CollectionRelationships {
    std::string collection_name;
    std::string data_type;
    std::vector<RelationshipInfo> relationships;
};
```

#### BranchRelationshipMapper
Main class that performs discovery and provides query methods:
- `discoverRelationships()`: Scans and maps all relationships
- `getCollectionRelationships()`: Gets relationships for a specific collection
- `getCollectionsByType()`: Finds collections matching a pattern
- `hasRelationships()`: Checks if a collection has relationships

## Integration

### DataSource
The `DataSource` class now:
1. Receives a pointer to the shared `BranchRelationshipMapper`
2. Uses it during branch setup to dynamically discover relationship branches
3. Falls back to hardcoded patterns if mapper is not available (backward compatibility)

### StandaloneTimesliceMerger
The merger:
1. Creates a `BranchRelationshipMapper` during initialization
2. Uses it to discover all collections and relationships
3. Passes it to all DataSource instances
4. Uses dynamic relationship queries instead of hardcoded names

### Example Usage

```cpp
// Create and initialize the mapper
auto mapper = std::make_unique<BranchRelationshipMapper>();
mapper->discoverRelationships(chain.get());

// Query discovered relationships
auto tracker_collections = mapper->getCollectionsByType("SimTrackerHit");
if (mapper->hasRelationships("MCParticles")) {
    auto rel_branches = mapper->getRelationshipBranches("MCParticles");
    // Process each relationship branch dynamically
}
```

## Benefits

1. **Automatic Discovery**: No manual coding of relationship patterns needed
2. **Flexibility**: Handles any EDM4hep collection type automatically
3. **Maintainability**: Changes to podio naming conventions handled automatically
4. **Type Safety**: Uses actual branch types from ROOT metadata
5. **Backward Compatibility**: Falls back to hardcoded patterns if needed

## Migration Path

The implementation includes fallback mechanisms:
```cpp
if (relationship_mapper_->hasRelationships(name)) {
    // Use discovered relationships
    auto rel_branches = relationship_mapper_->getRelationshipBranches(name);
    // ...
} else {
    // Fallback to hardcoded pattern
    std::string ref_branch_name = "_" + name + "_particle";
    // ...
}
```

This allows:
- Gradual migration of code
- Testing with and without the mapper
- Support for files that might not follow standard conventions

## Future Enhancements

Possible improvements:
1. **Podio Metadata Parsing**: Read relationship information directly from podio metadata tree
2. **Caching**: Cache discovered relationships to avoid repeated scans
3. **Validation**: Add checks to ensure relationship integrity
4. **Custom Mappings**: Allow user-specified relationship overrides
5. **Relationship Types**: Distinguish between OneToOne, OneToMany, and ManyToMany relationships

## Testing

To verify the system:
1. Run with existing input files to ensure all relationships are discovered
2. Check the debug output from `printDiscoveredRelationships()`
3. Verify output files contain the same structure as before
4. Test with podio's reader to ensure compatibility

## References

- EDM4hep documentation: https://github.com/key4hep/EDM4hep
- Podio framework: https://github.com/AIDASoft/podio
- ROOT I/O: https://root.cern/manual/io/
