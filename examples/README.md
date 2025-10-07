# IndexOffsetHelper Examples

This directory contains examples demonstrating how to use the `IndexOffsetHelper` class to automatically infer and apply index offsets to EDM4hep data structures.

## Overview

The `IndexOffsetHelper` was created to solve the problem of hardcoded index offset logic scattered throughout the codebase. Previously, each collection type had specific hardcoded field updates like:

```cpp
// Old hardcoded approach
particle.parents_begin   += particle_index_offset;
particle.parents_end     += particle_index_offset;
particle.daughters_begin += particle_index_offset;
particle.daughters_end   += particle_index_offset;
```

Now, this logic is centralized in the `IndexOffsetHelper`:

```cpp
// New centralized approach
IndexOffsetHelper::applyMCParticleOffsets(particles, particle_index_offset);
```

## Key Features

### 1. Centralized Offset Logic
All index offset operations are defined in one place (`IndexOffsetHelper`), making the code more maintainable.

### 2. Automatic Inference
The helper can automatically determine which fields need offsets by analyzing ROOT branch names:

```cpp
// Infer offset fields from branch structure
std::vector<std::string> all_branches = {
    "_MCParticles_parents",
    "_MCParticles_daughters"
};

auto fields = IndexOffsetHelper::inferOffsetFieldsFromBranches("MCParticles", all_branches);
// Returns: ["parents", "daughters"]
```

### 3. Metadata API
Query information about offset requirements without hardcoding:

```cpp
auto metadata = IndexOffsetHelper::getMCParticleOffsetMetadata();
// Returns: {"MCParticles", {"parents", "daughters"}, "MC truth particles..."}
```

## Running the Example

The example program demonstrates all features of the `IndexOffsetHelper`:

```bash
# Build and run (once build system is set up)
./build/examples/index_offset_example
```

## Adding Support for New Collection Types

To add support for a new EDM4hep collection type:

1. **Add an apply function** in `IndexOffsetHelper.h`:
```cpp
static void applyMyCollectionOffsets(std::vector<edm4hep::MyCollectionData>& data, size_t offset) {
    for (auto& item : data) {
        item.reference_begin += offset;
        item.reference_end += offset;
    }
}
```

2. **Add a metadata function**:
```cpp
static OffsetFieldMetadata getMyCollectionOffsetMetadata() {
    return {
        "MyCollection", 
        {"reference"},
        "Description of my collection"
    };
}
```

3. **Update the metadata registry**:
```cpp
static std::vector<OffsetFieldMetadata> getAllOffsetMetadata() {
    return {
        getMCParticleOffsetMetadata(),
        getCaloHitOffsetMetadata(),
        getMyCollectionOffsetMetadata()  // Add your metadata
    };
}
```

4. **Use the helper in DataSource**:
```cpp
IndexOffsetHelper::applyMyCollectionOffsets(my_collection, offset);
```

## Benefits

- **Reduced Code Duplication**: Index offset logic is written once
- **Easier Maintenance**: Changes to offset logic only need to be made in one place
- **Better Extensibility**: Clear pattern for adding new collection types
- **Self-Documenting**: Metadata API provides clear information about requirements
- **Automatic Inference**: Can determine requirements from file structure

## See Also

- `include/IndexOffsetHelper.h` - Full implementation and API documentation
- `src/DataSource.cc` - Usage examples in production code
