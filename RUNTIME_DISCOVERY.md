# Runtime Discovery of OneToMany Relations

## Overview

The IndexOffsetHelper now supports **runtime discovery** of OneToMany relations from the ROOT file structure, eliminating the need for compile-time knowledge of field names like "parents", "daughters", or "contributions".

## How It Works

### Discovery Process

The system automatically discovers which fields need index offsets by:

1. **Opening the ROOT file** and accessing the "events" tree
2. **Scanning all branches** for ObjectID vector branches
3. **Parsing branch names** to extract collection and field information
4. **Building a map** of collection → field names

### Branch Name Pattern

ObjectID branches follow the pattern: `_<CollectionName>_<fieldname>`

Examples:
- `_MCParticles_parents` → Collection: "MCParticles", Field: "parents"
- `_MCParticles_daughters` → Collection: "MCParticles", Field: "daughters"  
- `_ECalBarrelCollection_contributions` → Collection: "ECalBarrelCollection", Field: "contributions"

## Usage

### Automatic Discovery in DataSource

The `DataSource` class automatically discovers relations during initialization:

```cpp
void DataSource::initialize(...) {
    // Create TChain and add files
    // ...
    
    // Automatic discovery during branch setup
    setupBranches();  // Calls discoverOneToManyRelations()
}

void DataSource::discoverOneToManyRelations() {
    // Extract relations from first input file
    one_to_many_relations_ = IndexOffsetHelper::extractAllOneToManyRelations(
        config_->input_files[0]
    );
    
    // Prints discovered relations:
    // Collection 'MCParticles' has OneToMany fields: parents, daughters
    // Collection 'ECalBarrelCollection' has OneToMany fields: contributions
}
```

### Applying Offsets with Discovered Relations

When processing collections, the discovered field names are used dynamically:

```cpp
void DataSource::processMCParticles(...) {
    // ... time offset processing ...
    
    // Apply index offsets using discovered field names
    auto it = one_to_many_relations_.find("MCParticles");
    if (it != one_to_many_relations_.end() && !it->second.empty()) {
        // Use runtime-discovered field names
        IndexOffsetHelper::applyMCParticleOffsets(particles, offset, it->second);
    } else {
        // Fallback to default if discovery failed
        IndexOffsetHelper::applyMCParticleOffsets(particles, offset);
    }
}
```

### Direct API Usage

You can also use the discovery API directly:

```cpp
// Extract all OneToMany relations from a file
auto relations = IndexOffsetHelper::extractAllOneToManyRelations("input.root");
// Returns: map<string, vector<string>>
//   {"MCParticles": ["parents", "daughters"],
//    "ECalBarrelCollection": ["contributions"], ...}

// Or for a specific collection
auto fields = IndexOffsetHelper::extractOneToManyFieldsFromFile("input.root", "MCParticles");
// Returns: ["parents", "daughters"]
```

## Benefits

✅ **No compile-time knowledge required** - Field names discovered at runtime  
✅ **Adapts to data model changes** - Works with any EDM4hep schema  
✅ **No hardcoded field names** - Structure comes from file itself  
✅ **Future-proof** - Automatically supports new collection types  
✅ **Backward compatible** - Falls back to defaults if discovery fails  

## Implementation Details

### IndexOffsetHelper API

The helper provides two main discovery functions:

```cpp
// Extract OneToMany fields for a specific collection
static std::vector<std::string> extractOneToManyFieldsFromFile(
    const std::string& file_path,
    const std::string& collection_name);

// Extract all OneToMany relations in the file
static std::map<std::string, std::vector<std::string>> extractAllOneToManyRelations(
    const std::string& file_path);
```

### Offset Application with Dynamic Fields

The apply functions now accept optional field name parameters:

```cpp
// Apply with dynamically discovered field names
static void applyMCParticleOffsets(
    std::vector<edm4hep::MCParticleData>& particles, 
    size_t offset,
    const std::vector<std::string>& field_names);

// Backward-compatible version (uses default field names)
static void applyMCParticleOffsets(
    std::vector<edm4hep::MCParticleData>& particles, 
    size_t offset);
```

### Adding Support for New Fields

To support a new OneToMany field in MCParticleData:

```cpp
static void applyMCParticleOffsets(..., const std::vector<std::string>& field_names) {
    for (auto& particle : particles) {
        for (const auto& field_name : field_names) {
            if (field_name == "parents") {
                particle.parents_begin += offset;
                particle.parents_end += offset;
            } else if (field_name == "daughters") {
                particle.daughters_begin += offset;
                particle.daughters_end += offset;
            } else if (field_name == "your_new_field") {
                // Add your new field handling here
                particle.your_new_field_begin += offset;
                particle.your_new_field_end += offset;
            }
        }
    }
}
```

The discovery process will automatically find the new field if it exists in the file!

## Example Output

When a DataSource initializes, you'll see output like:

```
=== Setting up branches for source 0 ===
Discovering OneToMany relations from file structure...
  Collection 'MCParticles' has OneToMany fields: parents, daughters
  Collection 'ECalBarrelCollection' has OneToMany fields: contributions
  Collection 'HCalBarrelCollection' has OneToMany fields: contributions
  Collection 'VertexBarrelCollection' has OneToMany fields: particle
=== Branch setup complete ===
```

## Migration from Hardcoded Approach

### Before (Hardcoded)
```cpp
// Hardcoded field names
particle.parents_begin += offset;
particle.parents_end += offset;
particle.daughters_begin += offset;
particle.daughters_end += offset;
```

### After (Runtime Discovery)
```cpp
// Field names discovered at runtime
auto fields = one_to_many_relations_["MCParticles"];  // ["parents", "daughters"]
IndexOffsetHelper::applyMCParticleOffsets(particles, offset, fields);
```

## See Also

- `include/IndexOffsetHelper.h` - Full API documentation
- `src/DataSource.cc` - Production usage examples
- `examples/README.md` - General IndexOffsetHelper usage
