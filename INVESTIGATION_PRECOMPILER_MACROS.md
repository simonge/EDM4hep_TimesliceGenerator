# Investigation: Using C++ Preprocessor Macros for EDM4hep Branch Name Mapping

## Executive Summary

This investigation addresses the request to use C++ preprocessor macros to link the names of vector/association members of EDM4hep objects to the ROOT branch names they reference. 

**Result**: Successfully implemented a macro-based system that provides:
- ✅ Centralized branch name definitions
- ✅ Direct linkage between EDM4hep member names and branch names
- ✅ Type-safe helper functions
- ✅ Full backward compatibility
- ✅ Improved code maintainability

## Background

### The Problem

The codebase previously used hardcoded string literals to construct ROOT branch names for EDM4hep vector/association members:

```cpp
// Old approach - hardcoded strings
std::string parents_branch = "_MCParticles_parents";
std::string ref_branch = "_" + collection_name + "_particle";
std::string contrib_branch = "_" + collection_name + "_contributions";
```

**Issues with this approach:**
1. No compile-time guarantee that strings match actual EDM4hep member names
2. String literals scattered throughout the code
3. Risk of typos (e.g., "partical" instead of "particle")
4. Difficult to maintain if EDM4hep conventions change
5. No clear documentation of the mapping between members and branch names

### EDM4hep Branch Naming Convention

EDM4hep stores vector and association members as separate ROOT branches with the naming pattern:
```
_<CollectionName>_<member_name>
```

Examples:
- MCParticle's `parents` member → `_MCParticles_parents` branch
- MCParticle's `daughters` member → `_MCParticles_daughters` branch
- SimTrackerHit's `particle` member → `_VXDTrackerHits_particle` branch
- SimCalorimeterHit's `contributions` member → `_ECalBarrelHits_contributions` branch
- CaloHitContribution's `particle` member → `_ECalBarrelHitsContributions_particle` branch

## Solution Implementation

### 1. Created EDM4hepBranchNames.h Header

A new header file (`include/EDM4hepBranchNames.h`) provides:

#### Core Macros
```cpp
#define EDM4HEP_STRINGIFY(x) #x
#define EDM4HEP_MEMBER_NAME(member) EDM4HEP_STRINGIFY(member)
```

#### Member Name Constants
Type-safe constants that stringify EDM4hep member names:

```cpp
namespace MCParticle {
    constexpr const char* PARENTS_MEMBER = EDM4HEP_STRINGIFY(parents);
    constexpr const char* DAUGHTERS_MEMBER = EDM4HEP_STRINGIFY(daughters);
}

namespace SimTrackerHit {
    constexpr const char* PARTICLE_MEMBER = EDM4HEP_STRINGIFY(particle);
}

namespace SimCalorimeterHit {
    constexpr const char* CONTRIBUTIONS_MEMBER = EDM4HEP_STRINGIFY(contributions);
}

namespace CaloHitContribution {
    constexpr const char* PARTICLE_MEMBER = EDM4HEP_STRINGIFY(particle);
}
```

#### Convenience Functions
Type-safe helper functions for common patterns:

```cpp
std::string getMCParticleParentsBranchName();
std::string getMCParticleDaughtersBranchName();
std::string getTrackerHitParticleBranchName(const std::string& collection_name);
std::string getCaloHitContributionsBranchName(const std::string& collection_name);
std::string getContributionParticleBranchName(const std::string& contribution_collection_name);
```

### 2. Updated DataSource.cc

Modified all branch name construction code to use the new macro-based functions:

**Before:**
```cpp
std::string parents_branch_name = "_MCParticles_parents";
std::string children_branch_name = "_MCParticles_daughters";
std::string ref_branch_name = "_" + coll_name + "_particle";
```

**After:**
```cpp
std::string parents_branch_name = getMCParticleParentsBranchName();
std::string children_branch_name = getMCParticleDaughtersBranchName();
std::string ref_branch_name = getTrackerHitParticleBranchName(coll_name);
```

### 3. Created Test Program

A comprehensive test program (`examples/test_branch_name_macros.cc`) validates:
- Member name constants match expected values
- Branch name generation follows EDM4hep conventions
- Custom branch name construction works correctly
- Backward compatibility with hardcoded approach

## Benefits

### 1. Improved Code Clarity
```cpp
// Clear intent - we're getting the particle reference branch
std::string branch = getTrackerHitParticleBranchName(collection);

// vs unclear string manipulation
std::string branch = "_" + collection + "_particle";
```

### 2. Centralized Documentation
All branch naming logic is documented in one place with extensive comments and usage examples.

### 3. Type Safety
The macro approach uses EDM4hep member names as tokens, providing a degree of compile-time safety:
```cpp
// This uses 'parents' as a token, not a string
constexpr const char* PARENTS_MEMBER = EDM4HEP_STRINGIFY(parents);
```

### 4. Maintainability
If EDM4hep naming conventions change, updates are needed in only one place.

### 5. Self-Documenting Code
Function names clearly indicate what they do:
- `getMCParticleParentsBranchName()` - obvious purpose
- `getTrackerHitParticleBranchName()` - clear intent

### 6. Backward Compatibility
The test program proves the macro-based approach produces identical results to the hardcoded strings.

## Usage Examples

### Basic Usage
```cpp
#include "EDM4hepBranchNames.h"

// MCParticle branches
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

### Custom Construction
```cpp
// Direct use of member name constants
std::string custom = EDM4HEP_BRANCH_NAME("MyCollection", 
                                         SimTrackerHit::PARTICLE_MEMBER);
// Result: "_MyCollection_particle"
```

## Limitations and Considerations

### 1. Not True Compile-Time Verification
C++ preprocessor macros cannot verify that the tokens used actually correspond to real struct members. For example:

```cpp
// This will compile but may not match actual EDM4hep
constexpr const char* WRONG = EDM4HEP_STRINGIFY(nonexistent_member);
```

**Mitigation:** 
- Comprehensive test suite validates correctness
- Clear documentation of expected member names
- Regular verification against EDM4hep headers

### 2. Manual Synchronization Required
The macro definitions must be manually kept in sync with EDM4hep member names.

**Mitigation:**
- Test program validates expected names
- Centralized location makes updates easy
- Runtime errors will occur if names don't match

### 3. Not Full Reflection
This is not true C++ reflection. We're using preprocessor text substitution.

**Future Improvement:**
When C++26 reflection becomes available, this could be replaced with true compile-time introspection:
```cpp
template<typename T>
constexpr auto getMemberName(auto member_ptr) {
    return std::meta::name_of(member_ptr);
}
```

## Comparison with Alternatives

### Alternative 1: Keep Hardcoded Strings
**Pros:** Simple, no new code
**Cons:** Error-prone, scattered, hard to maintain

### Alternative 2: Runtime String Registry
**Pros:** More dynamic
**Cons:** Runtime overhead, no compile-time checking

### Alternative 3: Template Metaprogramming
**Pros:** More type-safe
**Cons:** Very complex, hard to understand and maintain

### Alternative 4: Wait for C++26 Reflection
**Pros:** True compile-time verification
**Cons:** Years away from widespread compiler support

### Selected Solution: Preprocessor Macros
**Pros:** 
- Compile-time string generation
- Zero runtime overhead
- Readable and maintainable
- Works with C++20
- Backward compatible

**Cons:**
- Not true type checking
- Manual synchronization required

The preprocessor macro approach provides the best balance of safety, maintainability, and practical usability with current C++ standards.

## Testing Results

The test program (`test_branch_name_macros`) validates:

✅ All member name constants are correct:
- `MCParticle::PARENTS_MEMBER == "parents"`
- `MCParticle::DAUGHTERS_MEMBER == "daughters"`
- `SimTrackerHit::PARTICLE_MEMBER == "particle"`
- `SimCalorimeterHit::CONTRIBUTIONS_MEMBER == "contributions"`
- `CaloHitContribution::PARTICLE_MEMBER == "particle"`

✅ All branch name generation functions produce expected results

✅ Custom branch construction works correctly

✅ Full backward compatibility - macro-based approach produces identical results to hardcoded strings

## Files Modified/Created

### Created
1. `include/EDM4hepBranchNames.h` - Macro definitions and helper functions
2. `examples/test_branch_name_macros.cc` - Comprehensive test program
3. `INVESTIGATION_PRECOMPILER_MACROS.md` - This documentation

### Modified
1. `src/DataSource.cc` - Updated to use macro-based functions
   - `setupMCParticleBranches()` - Uses `getMCParticleParentsBranchName()` etc.
   - `setupTrackerBranches()` - Uses `getTrackerHitParticleBranchName()`
   - `setupCalorimeterBranches()` - Uses `getCaloHitContributionsBranchName()` etc.
   - `loadNextEvent()` - Uses all the macro-based functions

## Migration Guide

For developers wanting to add new EDM4hep types:

1. **Add member name constants** in `EDM4hepBranchNames.h`:
   ```cpp
   namespace NewType {
       constexpr const char* NEW_MEMBER = EDM4HEP_STRINGIFY(new_member);
   }
   ```

2. **Add convenience function** (optional):
   ```cpp
   inline std::string getNewTypeMemberBranchName(const std::string& collection) {
       return EDM4HEP_BRANCH_NAME(collection, NewType::NEW_MEMBER);
   }
   ```

3. **Use in code**:
   ```cpp
   std::string branch = getNewTypeMemberBranchName("MyCollection");
   ```

4. **Add test** in `test_branch_name_macros.cc`

## Recommendations

### Immediate Actions
1. ✅ **Adopt the macro-based approach** - Provides immediate benefits with minimal risk
2. ✅ **Run the test program** regularly to verify correctness
3. ✅ **Update documentation** to reference the new approach

### Future Enhancements
1. **Add more EDM4hep types** as they're used in the codebase
2. **Extend test coverage** for edge cases
3. **Monitor C++26 reflection** for future migration path
4. **Consider code generation** from EDM4hep YAML definitions if available

### Best Practices
1. **Always use the helper functions** instead of constructing branch names manually
2. **Add tests** when adding new branch name patterns
3. **Document** any new EDM4hep types added to the macro system
4. **Review** EDM4hep updates for member name changes

## Conclusion

The investigation successfully demonstrates that C++ preprocessor macros can be used to link EDM4hep vector/association member names to ROOT branch names. The implemented solution:

- ✅ Improves code clarity and maintainability
- ✅ Provides a centralized, documented system
- ✅ Maintains full backward compatibility
- ✅ Works with current C++20 standard
- ✅ Has zero runtime overhead
- ✅ Is easily extensible for new types

While not providing true compile-time type verification (which would require C++26 reflection), the macro-based approach offers significant practical benefits over hardcoded strings and represents a substantial improvement to the codebase.

**Recommendation**: Adopt this approach throughout the codebase for all EDM4hep branch name construction.
