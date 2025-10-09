# Simplified Collection Processing with C++20

## Overview

This refactoring uses C++20 concepts to automatically detect which fields need updating in collection types, treating all branches uniformly without special cases.

## Key Principles

### Uniform Branch Treatment
All branches are treated equally based on their type characteristics:
- If a type has `daughters_begin`, it gets updated with the MCParticles offset
- If a type has `contributions_begin`, it gets updated with the contributions offset
- No special-casing for `_MCParticles_parents` vs `_MCParticles_daughters` - they're just ObjectID collections

### Automatic Field Detection
C++20 concepts detect at compile time which fields a type has:

```cpp
template<typename T>
concept HasDaughters = requires(T t) {
    { t.daughters_begin } -> std::convertible_to<unsigned int>;
    { t.daughters_end } -> std::convertible_to<unsigned int>;
};
```

### Single Processing Path
All collections go through the same processing function:

```cpp
processCollectionGeneric(collection_data, collection_type,
                        time_offset, gen_status_offset,
                        offsets_map, already_merged);
```

The function automatically applies appropriate offsets based on:
1. **Compile-time type detection** (`if constexpr`)
2. **Runtime offsets map** passed by caller

## Implementation

### Type-Based Processing
The `applyOffsets()` function uses `if constexpr` to check each possible field:

```cpp
template<typename T>
void applyOffsets(std::vector<T>& collection, ..., 
                 const std::map<std::string, size_t>& offsets_map, ...) {
    for (auto& item : collection) {
        // Only processes fields that exist in T
        if constexpr (HasTime<T>) {
            if (!already_merged) item.time += time_offset;
        }
        
        if constexpr (HasDaughters<T>) {
            auto it = offsets_map.find("MCParticles");
            if (it != offsets_map.end()) {
                item.daughters_begin += it->second;
                item.daughters_end += it->second;
            }
        }
        // ... other fields ...
    }
}
```

### Offsets Map
The caller builds an offsets map with the relevant collection sizes:

```cpp
std::map<std::string, size_t> offsets_map;
offsets_map["MCParticles"] = merged_collections_.mcparticles.size();
// Add other offsets as needed for this collection type
```

## Benefits

1. **Uniform Treatment**: No special cases for parent/daughter branches
2. **Automatic**: Fields are automatically updated if they exist in the type
3. **Compile-Time**: All type checking done at compile time with `if constexpr`
4. **Extensible**: New field types just need a concept definition
5. **Simple**: Single code path for all collections

## Code Metrics

- `CollectionTypeTraits.h`: 168 lines (increased from 129 for more flexibility)
- Processing logic: Unified single path through `processCollectionGeneric()`
- No special handling needed for reference branches - they work automatically

## Future Improvements

The destination routing (which container to merge into) could potentially be further simplified with a similar map-based approach, but that would require restructuring the `MergedCollections` data structure.
