# Generic Offset Application with Field Accessors

## Overview

The IndexOffsetHelper now uses a **completely generic approach** with pointer-to-member to apply offsets without any hardcoded field name checks. This addresses the requirement to "identify the members automatically and apply the relevant offset by this reference rather than name."

## Key Improvement

**Previous approach (still had hardcoded checks):**
```cpp
for (const auto& field_name : field_names) {
    if (field_name == "parents") {              // ❌ Hardcoded name check
        particle.parents_begin += offset;
        particle.parents_end += offset;
    } else if (field_name == "daughters") {     // ❌ Hardcoded name check
        particle.daughters_begin += offset;
        particle.daughters_end += offset;
    }
}
```

**New approach (no hardcoded checks):**
```cpp
// Generic function using pointer-to-member
applyOffsetsGeneric(particles, offset, field_names, accessors);

// The generic function uses member pointers directly:
item.*(accessor.begin_member) += offset;  // ✅ No name checks!
item.*(accessor.end_member) += offset;    // ✅ Direct member access!
```

## How It Works

### 1. Field Accessor Registry

Each data type has a registry of field accessors that map field names to actual struct member pointers:

```cpp
template<typename T>
struct FieldAccessor {
    unsigned int T::*begin_member;  // Pointer to begin member
    unsigned int T::*end_member;    // Pointer to end member
    std::string field_name;         // Name for matching with discovered fields
};
```

### 2. Registration for Each Type

For MCParticleData:
```cpp
static const std::vector<FieldAccessor<edm4hep::MCParticleData>>& getMCParticleFieldAccessors() {
    static std::vector<FieldAccessor<edm4hep::MCParticleData>> accessors = {
        {&edm4hep::MCParticleData::parents_begin, 
         &edm4hep::MCParticleData::parents_end, 
         "parents"},
        {&edm4hep::MCParticleData::daughters_begin, 
         &edm4hep::MCParticleData::daughters_end, 
         "daughters"}
    };
    return accessors;
}
```

For SimCalorimeterHitData:
```cpp
static const std::vector<FieldAccessor<edm4hep::SimCalorimeterHitData>>& getCaloHitFieldAccessors() {
    static std::vector<FieldAccessor<edm4hep::SimCalorimeterHitData>> accessors = {
        {&edm4hep::SimCalorimeterHitData::contributions_begin, 
         &edm4hep::SimCalorimeterHitData::contributions_end, 
         "contributions"}
    };
    return accessors;
}
```

### 3. Generic Application Function

A single generic function handles all data types:

```cpp
template<typename T>
static void applyOffsetsGeneric(std::vector<T>& data,
                                size_t offset,
                                const std::vector<std::string>& field_names,
                                const std::vector<FieldAccessor<T>>& accessors) {
    for (auto& item : data) {
        for (const auto& field_name : field_names) {
            // Find matching accessor by field name
            for (const auto& accessor : accessors) {
                if (accessor.field_name == field_name) {
                    // Apply offset using pointer-to-member (no hardcoded field names!)
                    item.*(accessor.begin_member) += offset;
                    item.*(accessor.end_member) += offset;
                    break;
                }
            }
        }
    }
}
```

### 4. Type-Specific Wrappers

Type-specific functions just call the generic function with the appropriate accessor registry:

```cpp
static void applyMCParticleOffsets(std::vector<edm4hep::MCParticleData>& particles, 
                                   size_t offset,
                                   const std::vector<std::string>& field_names) {
    applyOffsetsGeneric(particles, offset, field_names, getMCParticleFieldAccessors());
}

static void applyCaloHitOffsets(std::vector<edm4hep::SimCalorimeterHitData>& hits, 
                                size_t offset,
                                const std::vector<std::string>& field_names) {
    applyOffsetsGeneric(hits, offset, field_names, getCaloHitFieldAccessors());
}
```

## Benefits

### 1. Single Generic Function

One `applyOffsetsGeneric()` function handles all collection types - no need for separate logic per type.

### 2. No Hardcoded Field Name Checks

The code uses pointer-to-member to access fields directly by reference, not by checking string names in if-else chains.

### 3. Minimal Registration Required

To add support for a new field, just add one line to the accessor registry:
```cpp
{&YourType::new_field_begin, &YourType::new_field_end, "new_field"}
```

### 4. Type-Safe

Uses C++ type system with pointer-to-member - compile-time errors if members don't exist.

## Adding a New Collection Type

To add support for a new EDM4hep collection type:

### Step 1: Create Field Accessor Registry

```cpp
static const std::vector<FieldAccessor<edm4hep::YourNewData>>& getYourNewDataFieldAccessors() {
    static std::vector<FieldAccessor<edm4hep::YourNewData>> accessors = {
        {&edm4hep::YourNewData::field1_begin, &edm4hep::YourNewData::field1_end, "field1"},
        {&edm4hep::YourNewData::field2_begin, &edm4hep::YourNewData::field2_end, "field2"}
    };
    return accessors;
}
```

### Step 2: Create Wrapper Function

```cpp
static void applyYourNewDataOffsets(std::vector<edm4hep::YourNewData>& data, 
                                    size_t offset,
                                    const std::vector<std::string>& field_names) {
    applyOffsetsGeneric(data, offset, field_names, getYourNewDataFieldAccessors());
}
```

That's it! No if-else chains, no hardcoded field name checks.

## Complete Flow

```
Runtime Discovery
       ↓
File Structure → Extract Field Names → ["parents", "daughters"]
       ↓
DataSource Processing
       ↓
applyMCParticleOffsets(particles, offset, ["parents", "daughters"])
       ↓
applyOffsetsGeneric(particles, offset, field_names, MCParticle_accessors)
       ↓
For each field_name in ["parents", "daughters"]:
  Find matching accessor in registry
  Use pointer-to-member to access fields directly
       ↓
item.*(accessor.begin_member) += offset;  // Direct member access!
item.*(accessor.end_member) += offset;
```

## Comparison with Previous Approaches

### Original (Hardcoded Everything)
```cpp
particle.parents_begin += offset;      // ❌ Hardcoded field
particle.parents_end += offset;        // ❌ Hardcoded field
particle.daughters_begin += offset;    // ❌ Hardcoded field
particle.daughters_end += offset;      // ❌ Hardcoded field
```

### First Refactoring (Hardcoded Field Checks)
```cpp
if (field_name == "parents") {         // ❌ Hardcoded string check
    particle.parents_begin += offset;
    particle.parents_end += offset;
}
```

### Current (Generic with Member Pointers)
```cpp
item.*(accessor.begin_member) += offset;  // ✅ No hardcoding!
item.*(accessor.end_member) += offset;    // ✅ Direct member access!
```

## Technical Details

### Pointer-to-Member Syntax

```cpp
unsigned int T::*begin_member;           // Declare pointer-to-member
&T::field_name                           // Take address of member
item.*(pointer_to_member)                // Access member via pointer
```

### Why This Works

- **Type-safe**: Compiler validates member existence
- **Efficient**: No string comparisons at runtime
- **Generic**: Single function handles all types
- **Maintainable**: Add new fields by registration only

## See Also

- `include/IndexOffsetHelper.h` - Complete implementation
- `RUNTIME_DISCOVERY.md` - Runtime discovery documentation
- `REFACTORING_INDEX_OFFSETS.md` - Original refactoring details
