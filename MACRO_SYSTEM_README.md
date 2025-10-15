# EDM4hep Branch Name Macro System - Quick Reference

## What is This?

A C++ preprocessor macro system that links EDM4hep vector/association member names to ROOT branch names, replacing hardcoded strings with type-safe, self-documenting functions.

## Quick Start

### Include the Header
```cpp
#include "EDM4hepBranchNames.h"
```

### Use the Functions

```cpp
// MCParticle relationships
std::string parents = getMCParticleParentsBranchName();
// Result: "_MCParticles_parents"

std::string daughters = getMCParticleDaughtersBranchName();
// Result: "_MCParticles_daughters"

// Tracker hit particle reference
std::string tracker_ref = getTrackerHitParticleBranchName("VXDTrackerHits");
// Result: "_VXDTrackerHits_particle"

// Calorimeter hit contributions
std::string calo_contrib = getCaloHitContributionsBranchName("ECalBarrelHits");
// Result: "_ECalBarrelHits_contributions"

// Contribution particle reference
std::string contrib_particle = getContributionParticleBranchName("ECalBarrelHitsContributions");
// Result: "_ECalBarrelHitsContributions_particle"
```

## Why Use This?

### Before (Hardcoded)
```cpp
std::string parents = "_MCParticles_parents";          // ⚠️ Easy to typo
std::string ref = "_" + collection + "_particle";      // ⚠️ Scattered
```

### After (Macro-Based)
```cpp
std::string parents = getMCParticleParentsBranchName();      // ✓ Clear
std::string ref = getTrackerHitParticleBranchName(collection); // ✓ Safe
```

## Available Functions

| Function | Usage | Result |
|----------|-------|--------|
| `getMCParticleParentsBranchName()` | MCParticle parents | `"_MCParticles_parents"` |
| `getMCParticleDaughtersBranchName()` | MCParticle daughters | `"_MCParticles_daughters"` |
| `getTrackerHitParticleBranchName(coll)` | TrackerHit particle ref | `"_<coll>_particle"` |
| `getCaloHitContributionsBranchName(coll)` | CaloHit contributions | `"_<coll>_contributions"` |
| `getContributionParticleBranchName(coll)` | Contribution particle | `"_<coll>_particle"` |

## Member Name Constants

Access the EDM4hep member names directly:

```cpp
MCParticle::PARENTS_MEMBER               // "parents"
MCParticle::DAUGHTERS_MEMBER             // "daughters"
SimTrackerHit::PARTICLE_MEMBER           // "particle"
SimCalorimeterHit::CONTRIBUTIONS_MEMBER  // "contributions"
CaloHitContribution::PARTICLE_MEMBER     // "particle"
```

## Testing

Run the test program to verify everything works:

```bash
g++ -std=c++20 -I./include -o test_macros examples/test_branch_name_macros.cc
./test_macros
```

## Documentation

- **`INVESTIGATION_PRECOMPILER_MACROS.md`** - Complete technical documentation
- **`INVESTIGATION_VISUAL_GUIDE.md`** - Visual diagrams and flowcharts
- **`INVESTIGATION_PRACTICAL_EXAMPLES.md`** - Hands-on examples and tutorials
- **`include/EDM4hepBranchNames.h`** - Inline API documentation

## Key Benefits

✅ **Type-safe** - Functions instead of string literals  
✅ **Self-documenting** - Clear function names  
✅ **Centralized** - One place to maintain  
✅ **Compatible** - Identical to hardcoded strings  
✅ **Zero overhead** - Compile-time generation  

## Common Patterns

### Loop Over Collections
```cpp
for (const auto& name : *tracker_collection_names_) {
    std::string ref = getTrackerHitParticleBranchName(name);
    // Use ref...
}
```

### Map Keys
```cpp
std::string key = getMCParticleParentsBranchName();
objectid_branches_[key] = new std::vector<podio::ObjectID>();
```

### Conditional Logic
```cpp
std::string branch;
if (is_tracker) {
    branch = getTrackerHitParticleBranchName(coll);
} else if (is_calo) {
    branch = getCaloHitContributionsBranchName(coll);
}
```

## Adding New Types

1. Add member constant in `EDM4hepBranchNames.h`
2. Add convenience function (optional)
3. Add test in `test_branch_name_macros.cc`
4. Use in your code

## Need Help?

See the detailed documentation files for:
- Architecture details
- Before/after comparisons
- Migration guides
- Troubleshooting tips
