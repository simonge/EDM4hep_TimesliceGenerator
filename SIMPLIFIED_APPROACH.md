# Simplified Collection Processing with C++20

## Overview

This refactoring uses C++20 concepts to automatically detect which fields need updating in collection types. All branches are treated uniformly with correct offset handling and a registry system eliminates hardcoded checks.

## Key Improvements

### 1. Correct Offset Handling

Field offsets now correctly map to their corresponding branch sizes:
- `daughters_begin` → size of `_MCParticles_daughters` branch (not MCParticles collection)
- `parents_begin` → size of `_MCParticles_parents` branch
- `contributions_begin` → size of contributions branch

**Before (incorrect)**:
```cpp
offsets_map["MCParticles"] = merged_collections_.mcparticles.size();
// daughters_begin incorrectly offset by MCParticles size
```

**After (correct)**:
```cpp
field_offsets["daughters"] = merged_collections_.mcparticle_children_refs.size();
field_offsets["parents"] = merged_collections_.mcparticle_parents_refs.size();
// daughters_begin correctly offset by daughters branch size
```

### 2. Collection Registry System

Eliminates hardcoded checks like `if (collection_name == "GPIntValues")`:

```cpp
// Register collections with their types and merge functions
CollectionRegistry::registerDescriptor("GPIntValues", {
    .type_name = "GPIntValues",
    .merge_function = [](std::any& data, MergedCollections& m, const std::string&) {
        auto* coll = std::any_cast<std::vector<std::vector<int>>>(&data);
        if (coll) m.gp_int_values.insert(...);
    },
    .get_size_function = [](const MergedCollections& m, const std::string&) {
        return m.gp_int_values.size();
    }
});

// Usage - no hardcoded checks needed
const auto* descriptor = CollectionRegistry::getDescriptor(collection_name);
if (descriptor) {
    descriptor->merge_function(collection_data, merged_collections_, collection_name);
}
```

### 3. Automatic Field Detection

C++20 concepts detect fields at compile time:

```cpp
template<typename T>
concept HasDaughters = requires(T t) {
    { t.daughters_begin } -> std::convertible_to<unsigned int>;
    { t.daughters_end } -> std::convertible_to<unsigned int>;
};

template<typename T>
void applyOffsets(..., const std::map<std::string, size_t>& field_offsets, ...) {
    for (auto& item : collection) {
        if constexpr (HasDaughters<T>) {
            // Automatically looks up "daughters" in field_offsets map
            auto it = field_offsets.find("daughters");
            if (it != field_offsets.end()) {
                item.daughters_begin += it->second;
                item.daughters_end += it->second;
            }
        }
    }
}
```

## Architecture

### Field Offset Map
```cpp
std::map<std::string, size_t> field_offsets;
field_offsets["parents"] = merged_collections_.mcparticle_parents_refs.size();
field_offsets["daughters"] = merged_collections_.mcparticle_children_refs.size();
field_offsets["contributions"] = merged_collections_.calo_contributions[name].size();
field_offsets["target"] = merged_collections_.mcparticles.size();  // For ObjectID refs
```

### Collection Registry
- Maps collection names to types and merge functions
- Handles: MCParticles, _MCParticles_parents, _MCParticles_daughters, GPIntValues, GPFloatValues, GPDoubleValues, GPStringValues
- Dynamic collections (tracker hits, calo hits) handled separately

### Type Traits (C++20 Concepts)
- `HasTime` - detects time field
- `HasGeneratorStatus` - detects generatorStatus field  
- `HasParents` - detects parents_begin/end fields
- `HasDaughters` - detects daughters_begin/end fields
- `HasContributions` - detects contributions_begin/end fields

## Benefits

1. **Correct Semantics**: Field offsets match their actual reference targets
2. **No Hardcoded Checks**: Registry system handles known collections
3. **Automatic**: C++20 concepts detect fields at compile time
4. **Extensible**: New collections added by registering descriptors
5. **Type Safe**: Compile-time type checking with concepts
6. **Simple**: Clear mapping between fields and their offsets

## Code Structure

- `CollectionTypeTraits.h` (180 lines) - C++20 concepts and offset application
- `CollectionRegistry.h/cc` - Registry system for collection handling
- `StandaloneTimesliceMerger.cc` - Simplified processing with field_offsets map
- All collection-specific logic centralized in registry
