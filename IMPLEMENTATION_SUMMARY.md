# Implementation Summary: Automatic Index Offsetting

## What Was Accomplished

### Phase 1: Infrastructure Creation ✅
Created the `BranchRelationshipMapper` class to automatically discover branch relationships from ROOT tree metadata:

**New Files:**
- `include/BranchRelationshipMapper.h` - Header defining mapper class and data structures
- `src/BranchRelationshipMapper.cc` - Implementation of discovery and query methods

**Key Features:**
- Automatic discovery of EDM4hep collection types
- Identification of relationship branches (ObjectID references)
- Parsing of branch naming conventions
- Query interface for accessing discovered relationships
- Support for OneToOne and OneToMany relationships

### Phase 2: DataSource Refactoring ✅
Updated `DataSource` to use dynamic relationship discovery instead of hardcoded patterns:

**Changes:**
- Added `relationship_mapper_` member to store reference to mapper
- Updated `initialize()` to accept mapper pointer
- Refactored `setupMCParticleBranches()` to use discovered relationships
- Refactored `setupTrackerBranches()` to use discovered relationships  
- Refactored `setupCalorimeterBranches()` to use discovered relationships
- Added fallback to hardcoded patterns for backward compatibility

**Benefits:**
- Automatically handles any collection type
- No need to update code when adding new detectors
- More maintainable and flexible

### Phase 3: StandaloneTimesliceMerger Updates ✅
Updated the merger to orchestrate automatic discovery:

**Changes:**
- Added `relationship_mapper_` member for managing discoveries
- Updated `initializeDataSources()` to create and use mapper
- Refactored `createMergedTimeslice()` to use dynamic relationship queries
- Updated `setupOutputTree()` to create branches based on discovered relationships
- Replaced all hardcoded branch name constructions with mapper queries

**Benefits:**
- Consistent handling across all collection types
- Better separation of concerns
- Easier to debug and understand data flow

### Documentation ✅
Created comprehensive documentation:
- `RELATIONSHIP_DISCOVERY.md` - Detailed explanation of the new system
- This summary file - Overview of implementation

## What Remains (Testing & Validation)

### Phase 4: Testing
The code needs to be tested with real data files:

1. **Build the project** with dependencies (ROOT, PODIO, EDM4HEP)
2. **Test with sample files** from the configs directory
3. **Verify discovery output** - Check that all relationships are found
4. **Validate index offsetting** - Ensure references are correctly updated
5. **Test output compatibility** - Confirm files can be read by podio readers

### Required Testing Steps

```bash
# 1. Build the project (requires ROOT, PODIO, EDM4HEP environment)
./build.sh

# 2. Test with a simple configuration
./install/bin/timeslice_merger --config configs/config.yml

# 3. Check the output for discovered relationships
# Look for "=== Discovering Branch Relationships ===" output

# 4. Validate output file with podio test
./install/bin/test_podio_format merged_timeslices.root
```

### Validation Checklist

- [ ] All collection types are discovered correctly
- [ ] All relationship branches are identified
- [ ] MCParticle parent/daughter relationships work
- [ ] Tracker hit particle references are correct
- [ ] Calorimeter hit contributions are properly linked
- [ ] Contribution particle references are accurate
- [ ] Output files have correct structure
- [ ] Output files can be read by podio::ROOTReader
- [ ] Index offsetting produces valid results
- [ ] No segmentation faults or memory errors

## Key Improvements Over Original

### Before (Hardcoded)
```cpp
// Hardcoded MCParticle relationships
std::string parents_branch_name = "_MCParticles_parents";
std::string children_branch_name = "_MCParticles_daughters";

// Hardcoded tracker references
std::string ref_branch_name = "_" + name + "_particle";

// Hardcoded calo contributions
std::string ref_branch_name = "_" + name + "_contributions";
```

### After (Dynamic Discovery)
```cpp
// Discover all relationships automatically
relationship_mapper_->discoverRelationships(chain);

// Query for specific collection's relationships
if (relationship_mapper_->hasRelationships("MCParticles")) {
    auto rel_branches = relationship_mapper_->getRelationshipBranches("MCParticles");
    for (const auto& branch_name : rel_branches) {
        // Process each discovered relationship
    }
}
```

## Architecture Changes

### Data Flow

**Old:**
```
Input File → Hardcoded Branch Names → DataSource Setup → Processing
```

**New:**
```
Input File → BranchRelationshipMapper → Discovered Relationships → DataSource Setup → Processing
```

### Class Relationships

```
StandaloneTimesliceMerger
    ├── BranchRelationshipMapper (owned)
    │   └── Discovers and stores all relationships
    └── DataSource instances (owned)
        └── Reference to shared BranchRelationshipMapper
```

## Backward Compatibility

The implementation includes fallback mechanisms throughout:
- If mapper is null, use hardcoded patterns
- If relationships not found, fall back to pattern-based names
- All existing functionality preserved

This ensures:
- No breaking changes for existing workflows
- Gradual migration path
- Easy testing and debugging

## Technical Details

### Relationship Discovery Algorithm

1. **Scan all branches** in the input TTree
2. **Identify object branches** by looking for `edm4hep::*Data` types
3. **Identify relationship branches** by:
   - Branch name starts with "_"
   - Data type contains "ObjectID"
   - Name follows pattern `_<Collection>_<relation>`
4. **Parse branch names** to extract collection and relation names
5. **Build mapping** from collections to their relationships
6. **Determine cardinality** (OneToMany vs OneToOne) from data type

### Memory Management

- `BranchRelationshipMapper` is owned by `StandaloneTimesliceMerger`
- DataSource stores a raw pointer (non-owning) to the mapper
- All relationship data is stored in the mapper for centralized access
- No duplication of relationship information

## Next Steps for Future Work

1. **Performance Optimization**
   - Cache discovered relationships
   - Avoid repeated chain creation
   - Optimize branch traversal

2. **Enhanced Discovery**
   - Parse podio_metadata tree directly
   - Support custom collection types
   - Handle non-standard naming

3. **Better Error Handling**
   - Validate relationship integrity
   - Check for missing branches
   - Provide helpful error messages

4. **Testing Infrastructure**
   - Add unit tests for mapper
   - Create integration tests
   - Add CI/CD validation

5. **Documentation**
   - Add inline code comments
   - Create usage examples
   - Update README with new features

## Conclusion

The implementation successfully addresses the original issue by:
- **Removing hardcoded relationships** throughout the codebase
- **Using automatic discovery** based on ROOT metadata
- **Maintaining backward compatibility** with fallback mechanisms
- **Improving maintainability** through better architecture
- **Following podio conventions** for relationship naming

The system is now more flexible, maintainable, and aligned with the podio framework's design philosophy. All that remains is thorough testing with real data files to validate the implementation.
