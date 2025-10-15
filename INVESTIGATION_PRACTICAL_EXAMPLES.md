# Practical Example: Using EDM4hep Branch Name Macros

This document provides a step-by-step walkthrough of using the macro-based branch name system in practice.

## Scenario: Adding a New Collection Type

Let's say we need to add support for a new EDM4hep collection type called `ReconstructedParticle` that has a `tracks` member (OneToManyRelation).

### Step 1: Define Member Name Constant

Edit `include/EDM4hepBranchNames.h`:

```cpp
/**
 * ReconstructedParticle Vector/Association Members
 * 
 * EDM4hep ReconstructedParticleData structure has:
 * - tracks: OneToManyRelations (stored as ObjectID vector)
 */
namespace ReconstructedParticle {
    constexpr const char* TRACKS_MEMBER = EDM4HEP_STRINGIFY(tracks);
}
```

### Step 2: Add Convenience Function

Still in `include/EDM4hepBranchNames.h`:

```cpp
/**
 * @brief Construct the tracks reference branch name for a reconstructed particle collection
 * @param collection_name The reconstructed particle collection name (e.g., "RecoParticles")
 * @return "_<collection_name>_tracks"
 */
inline std::string getReconstructedParticleTracksBranchName(const std::string& collection_name) {
    return EDM4HEP_BRANCH_NAME(collection_name, ReconstructedParticle::TRACKS_MEMBER);
}
```

### Step 3: Use in Code

In `src/DataSource.cc` (or wherever you're setting up branches):

```cpp
void DataSource::setupReconstructedParticleBranches() {
    for (const auto& coll_name : *reco_particle_collection_names_) {
        reco_particle_branches_[coll_name] = new std::vector<edm4hep::ReconstructedParticleData>();
        int result = chain_->SetBranchAddress(coll_name.c_str(), &reco_particle_branches_[coll_name]);
        
        // Use macro-based function to ensure branch name matches EDM4hep member name
        std::string tracks_branch_name = getReconstructedParticleTracksBranchName(coll_name);
        objectid_branches_[tracks_branch_name] = new std::vector<podio::ObjectID>();
        result = chain_->SetBranchAddress(tracks_branch_name.c_str(), &objectid_branches_[tracks_branch_name]);
    }
}
```

### Step 4: Add Tests

In `examples/test_branch_name_macros.cc`:

```cpp
void testReconstructedParticle() {
    std::cout << "ReconstructedParticle Tracks Reference Branches:" << std::endl;
    std::string reco_tracks = getReconstructedParticleTracksBranchName("RecoParticles");
    
    std::cout << "  RecoParticles: " << reco_tracks << std::endl;
    
    assert(reco_tracks == "_RecoParticles_tracks");
    std::cout << "  ✓ ReconstructedParticle tracks reference branch name is correct" << std::endl;
}
```

## Real-World Example: Refactoring Existing Code

Let's look at how the macro system was applied to actual code in this repository.

### Before: MCParticle Setup (Hardcoded)

```cpp
void DataSource::setupMCParticleBranches() {
    // Setup MCParticles branch
    mcparticle_branch_ = new std::vector<edm4hep::MCParticleData>();
    int result = chain_->SetBranchAddress("MCParticles", &mcparticle_branch_);

    // Setup MCParticle parent-child relationship branches
    std::string parents_branch_name = "_MCParticles_parents";        // ⚠️ Hardcoded
    std::string children_branch_name = "_MCParticles_daughters";     // ⚠️ Hardcoded
    
    objectid_branches_[parents_branch_name] = new std::vector<podio::ObjectID>();
    objectid_branches_[children_branch_name] = new std::vector<podio::ObjectID>();
    
    result = chain_->SetBranchAddress(parents_branch_name.c_str(), 
                                     &objectid_branches_[parents_branch_name]);
    result = chain_->SetBranchAddress(children_branch_name.c_str(), 
                                     &objectid_branches_[children_branch_name]);
}
```

**Problems:**
- If EDM4hep changes "parents" to "parentParticles", we have to find all occurrences
- Typo risk: "_MCParticles_parens" would compile but fail at runtime
- No clear indication these strings relate to EDM4hep member names
- Scattered documentation of the naming convention

### After: MCParticle Setup (Macro-Based)

```cpp
void DataSource::setupMCParticleBranches() {
    // Setup MCParticles branch
    mcparticle_branch_ = new std::vector<edm4hep::MCParticleData>();
    int result = chain_->SetBranchAddress("MCParticles", &mcparticle_branch_);

    // Setup MCParticle parent-child relationship branches using consolidated map
    // Use macros to ensure branch names match EDM4hep member names
    std::string parents_branch_name = getMCParticleParentsBranchName();       // ✓ Macro-based
    std::string children_branch_name = getMCParticleDaughtersBranchName();    // ✓ Macro-based
    
    objectid_branches_[parents_branch_name] = new std::vector<podio::ObjectID>();
    objectid_branches_[children_branch_name] = new std::vector<podio::ObjectID>();
    
    result = chain_->SetBranchAddress(parents_branch_name.c_str(), 
                                     &objectid_branches_[parents_branch_name]);
    result = chain_->SetBranchAddress(children_branch_name.c_str(), 
                                     &objectid_branches_[children_branch_name]);
}
```

**Benefits:**
- Clear intent: "get MCParticle parents branch name"
- If EDM4hep changes, update only in `EDM4hepBranchNames.h`
- IDE autocomplete helps discover available functions
- Self-documenting code

### Before: Tracker Hit Setup (Hardcoded)

```cpp
void DataSource::setupTrackerBranches() {
    for (const auto& coll_name : *tracker_collection_names_) {
        tracker_hit_branches_[coll_name] = new std::vector<edm4hep::SimTrackerHitData>();
        int result = chain_->SetBranchAddress(coll_name.c_str(), &tracker_hit_branches_[coll_name]);
        
        // Also setup the particle reference branch
        std::string ref_branch_name = "_" + coll_name + "_particle";  // ⚠️ Hardcoded pattern
        objectid_branches_[ref_branch_name] = new std::vector<podio::ObjectID>();
        result = chain_->SetBranchAddress(ref_branch_name.c_str(), &objectid_branches_[ref_branch_name]);
    }
}
```

### After: Tracker Hit Setup (Macro-Based)

```cpp
void DataSource::setupTrackerBranches() {
    for (const auto& coll_name : *tracker_collection_names_) {
        tracker_hit_branches_[coll_name] = new std::vector<edm4hep::SimTrackerHitData>();
        int result = chain_->SetBranchAddress(coll_name.c_str(), &tracker_hit_branches_[coll_name]);
        
        // Also setup the particle reference branch using consolidated map
        // Use macro-based function to ensure branch name matches EDM4hep member name
        std::string ref_branch_name = getTrackerHitParticleBranchName(coll_name);  // ✓ Macro-based
        objectid_branches_[ref_branch_name] = new std::vector<podio::ObjectID>();
        result = chain_->SetBranchAddress(ref_branch_name.c_str(), &objectid_branches_[ref_branch_name]);
    }
}
```

## Advanced Usage: Direct Macro Access

Sometimes you need low-level access to construct custom branch names:

```cpp
// Get just the member name as a string
const char* member = SimTrackerHit::PARTICLE_MEMBER;  // "particle"

// Use with custom collection name
std::string my_collection = "MyCustomTrackerHits";
std::string branch = EDM4HEP_BRANCH_NAME(my_collection, SimTrackerHit::PARTICLE_MEMBER);
// Result: "_MyCustomTrackerHits_particle"

// Or build completely custom patterns (not recommended unless necessary)
std::string custom = "_" + my_collection + "_" + std::string(SimTrackerHit::PARTICLE_MEMBER) + "_backup";
// Result: "_MyCustomTrackerHits_particle_backup"
```

## Common Patterns and Idioms

### Pattern 1: Loop Over Collections

```cpp
// Before
for (const auto& name : *tracker_collection_names_) {
    std::string ref = "_" + name + "_particle";
    // Use ref...
}

// After
for (const auto& name : *tracker_collection_names_) {
    std::string ref = getTrackerHitParticleBranchName(name);
    // Use ref...
}
```

### Pattern 2: Conditional Branch Names

```cpp
// Before
std::string branch_name;
if (is_tracker) {
    branch_name = "_" + coll + "_particle";
} else if (is_calo) {
    branch_name = "_" + coll + "_contributions";
}

// After
std::string branch_name;
if (is_tracker) {
    branch_name = getTrackerHitParticleBranchName(coll);
} else if (is_calo) {
    branch_name = getCaloHitContributionsBranchName(coll);
}
```

### Pattern 3: Map Keys

```cpp
// Before
std::unordered_map<std::string, std::vector<podio::ObjectID>*> objectid_branches_;
objectid_branches_["_MCParticles_parents"] = new std::vector<podio::ObjectID>();

// After
std::unordered_map<std::string, std::vector<podio::ObjectID>*> objectid_branches_;
std::string key = getMCParticleParentsBranchName();
objectid_branches_[key] = new std::vector<podio::ObjectID>();
```

## Debugging Tips

### Tip 1: Print Branch Names for Verification

```cpp
// Check what branch name is being generated
std::string branch = getTrackerHitParticleBranchName("VXDTrackerHits");
std::cout << "Branch name: " << branch << std::endl;
// Output: Branch name: _VXDTrackerHits_particle
```

### Tip 2: Verify Member Name Constants

```cpp
// Check the member name constant
std::cout << "Member name: " << SimTrackerHit::PARTICLE_MEMBER << std::endl;
// Output: Member name: particle
```

### Tip 3: Run the Test Program

```bash
g++ -std=c++20 -I./include -o test_macros examples/test_branch_name_macros.cc
./test_macros
```

## Migration Checklist

When migrating code to use the macro system:

- [ ] Identify all hardcoded branch name strings
- [ ] Replace with appropriate macro-based functions
- [ ] Check if convenience function exists, if not create one
- [ ] Update any comments to reference the macro system
- [ ] Run tests to verify functionality
- [ ] Check for any string concatenation patterns that build branch names
- [ ] Update documentation

## Performance Considerations

### Compile-Time String Generation

```cpp
// This happens at compile time
constexpr const char* PARENTS = EDM4HEP_STRINGIFY(parents);

// This happens at compile time (constant folding)
std::string branch = getMCParticleParentsBranchName();
// Equivalent to: std::string branch = "_MCParticles_parents";
```

### Runtime String Construction

```cpp
// This still requires runtime string concatenation
// (unavoidable when collection name is a runtime value)
std::string branch = getTrackerHitParticleBranchName(collection_name);
// Internally: return "_" + collection_name + "_particle";
```

The macro system doesn't add overhead - it just provides a cleaner, safer way to construct the strings.

## Error Messages

### What the Compiler Tells You

Good error messages help catch mistakes early:

```cpp
// Typo in function name
std::string branch = getTrackerHitParticeBranchName("VXD");  // Typo: "Partic" not "Particle"
// Compiler error: 'getTrackerHitParticeBranchName' was not declared in this scope
```

```cpp
// Missing include
std::string branch = getMCParticleParentsBranchName();
// Compiler error: 'getMCParticleParentsBranchName' was not declared in this scope
// Did you forget to include "EDM4hepBranchNames.h"?
```

## Summary

The macro-based branch name system provides:

1. **Safety**: Links member names to branch names at compile-time
2. **Clarity**: Self-documenting function names
3. **Maintainability**: Single location for all naming logic
4. **Performance**: Zero runtime overhead
5. **Flexibility**: Easy to extend for new types
6. **Compatibility**: Produces identical results to hardcoded strings

The key insight is using C++ preprocessor stringification of tokens (like `parents`, `daughters`, `particle`, `contributions`) to ensure these strings match the actual EDM4hep member names, reducing the chance of typos and making the code more maintainable.
