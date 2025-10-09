# Index Offset Refactoring Summary

## Problem Statement

The original code had hardcoded index offset logic scattered throughout the `DataSource` class:

```cpp
// In processMCParticles()
particle.parents_begin   += particle_index_offset;
particle.parents_end     += particle_index_offset;
particle.daughters_begin += particle_index_offset;
particle.daughters_end   += particle_index_offset;

// In processCaloHits()
hit.contributions_begin += contribution_index_offset;
hit.contributions_end += contribution_index_offset;
```

This hardcoding made it difficult to:
- Add support for new collection types
- Understand which fields needed offsets
- Maintain consistency across the codebase

## Solution

Created a new `IndexOffsetHelper` class that:

1. **Centralizes** all index offset logic in one place
2. **Provides metadata** about which fields need offsets for each collection type
3. **Enables automatic inference** of offset requirements from ROOT branch structure
4. **Makes it easy to extend** support for new EDM4hep collection types

## Changes Made

### 1. New IndexOffsetHelper Class (`include/IndexOffsetHelper.h`)

- `applyMCParticleOffsets()` - Applies offsets to MCParticle parent/daughter indices
- `applyCaloHitOffsets()` - Applies offsets to calorimeter hit contribution indices
- `getMCParticleOffsetMetadata()` - Returns metadata about MCParticle offset requirements
- `getCaloHitOffsetMetadata()` - Returns metadata about CaloHit offset requirements
- `inferOffsetFieldsFromBranches()` - Automatically determines offset fields from branch names
- `createMetadataFromBranches()` - Creates complete metadata from branch structure

### 2. Updated DataSource (`src/DataSource.cc`, `include/DataSource.h`)

**Before:**
```cpp
for (auto& particle : particles) {
    // ... time and status updates ...
    particle.parents_begin   += particle_index_offset;
    particle.parents_end     += particle_index_offset;
    particle.daughters_begin += particle_index_offset;
    particle.daughters_end   += particle_index_offset;
}
```

**After:**
```cpp
for (auto& particle : particles) {
    // ... time and status updates ...
}
IndexOffsetHelper::applyMCParticleOffsets(particles, particle_index_offset);
```

Similar changes were made for `processCaloHits()`.

### 3. Examples and Documentation

- `examples/index_offset_example.cc` - Complete example demonstrating all features
- `examples/README.md` - Comprehensive documentation on usage and extensibility

## Benefits

1. **Reduced Code Duplication**: Index offset logic is written once and reused
2. **Better Maintainability**: Changes only need to be made in one place
3. **Automatic Inference**: Can determine requirements from file structure
4. **Easy Extension**: Clear pattern for adding new collection types
5. **Self-Documenting**: Metadata API provides structured information

## Usage Example

```cpp
// Get metadata about offset requirements
auto metadata = IndexOffsetHelper::getMCParticleOffsetMetadata();
// Returns: {"MCParticles", {"parents", "daughters"}, "MC truth particles..."}

// Apply offsets
IndexOffsetHelper::applyMCParticleOffsets(particles, offset);

// Infer requirements from branch structure
std::vector<std::string> branches = {"_MCParticles_parents", "_MCParticles_daughters"};
auto fields = IndexOffsetHelper::inferOffsetFieldsFromBranches("MCParticles", branches);
// Returns: ["parents", "daughters"]
```

## Adding Support for New Collection Types

To add support for a new EDM4hep collection type:

1. Add an apply function in `IndexOffsetHelper`:
```cpp
static void applyMyCollectionOffsets(std::vector<edm4hep::MyData>& data, size_t offset) {
    for (auto& item : data) {
        item.field_begin += offset;
        item.field_end += offset;
    }
}
```

2. Add metadata function:
```cpp
static OffsetFieldMetadata getMyCollectionOffsetMetadata() {
    return {"MyCollection", {"field"}, "Description"};
}
```

3. Use the helper in DataSource:
```cpp
IndexOffsetHelper::applyMyCollectionOffsets(my_collection, offset);
```

## Impact

- **Production code changes**: Minimal (only 15 lines modified in DataSource.cc)
- **New code**: IndexOffsetHelper class, examples, and documentation
- **Backward compatibility**: Fully maintained - behavior is identical
- **Future extensibility**: Significantly improved

## Files Changed

- `include/IndexOffsetHelper.h` - New helper class (181 lines)
- `include/DataSource.h` - Added include (1 line)
- `src/DataSource.cc` - Replaced hardcoded offsets (15 lines modified)
- `examples/index_offset_example.cc` - Example program (103 lines)
- `examples/README.md` - Documentation (112 lines)

Total: 403 additions, 9 deletions in production code
