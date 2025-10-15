#ifndef EDM4HEP_BRANCH_NAMES_H
#define EDM4HEP_BRANCH_NAMES_H

#pragma once

/**
 * @file EDM4hepBranchNames.h
 * @brief C++ Preprocessor Macros for EDM4hep Vector/Association Branch Name Mapping
 * 
 * This header provides compile-time mapping between EDM4hep object member names 
 * and their corresponding ROOT branch names. This ensures consistency and reduces
 * the risk of string literal typos when constructing branch names.
 * 
 * Background:
 * -----------
 * EDM4hep (Event Data Model for High Energy Physics) stores vector and association
 * members as separate ROOT branches with a naming convention. For example:
 * 
 * - MCParticle has 'parents' and 'daughters' members stored as:
 *   "_MCParticles_parents" and "_MCParticles_daughters" branches
 * 
 * - SimTrackerHit has a 'particle' member stored as:
 *   "_<CollectionName>_particle" branch
 * 
 * - SimCalorimeterHit has a 'contributions' member stored as:
 *   "_<CollectionName>_contributions" branch
 * 
 * - CaloHitContribution has a 'particle' member stored as:
 *   "_<CollectionName>_particle" branch
 * 
 * These macros provide a centralized, type-safe way to construct these branch names
 * by directly referencing the EDM4hep member names as tokens, ensuring they match
 * the actual data structure members.
 */

#include <string>

// =============================================================================
// CORE MACROS FOR BRANCH NAME CONSTRUCTION
// =============================================================================

/**
 * @brief Stringify a preprocessor token
 * This is the standard C preprocessor technique for converting tokens to strings.
 */
#define EDM4HEP_STRINGIFY(x) #x

/**
 * @brief Convert a member name token to a string literal
 * This macro ensures compile-time conversion of member names to strings.
 */
#define EDM4HEP_MEMBER_NAME(member) EDM4HEP_STRINGIFY(member)

/**
 * @brief Construct a branch name for a vector/association member
 * @param collection The collection name (e.g., "MCParticles", "VXDTrackerHits")
 * @param member The member name token (e.g., parents, daughters, particle, contributions)
 * 
 * This constructs the branch name following EDM4hep's convention:
 * "_<collection>_<member>"
 * 
 * Note: This returns a std::string constructed at runtime. For compile-time
 * string literal requirements, use EDM4HEP_MEMBER_NAME directly.
 */
inline std::string EDM4HEP_BRANCH_NAME(const std::string& collection, const char* member) {
    return "_" + collection + "_" + member;
}

// =============================================================================
// TYPE-SPECIFIC MEMBER NAME CONSTANTS
// =============================================================================

/**
 * MCParticle Vector/Association Members
 * 
 * EDM4hep MCParticleData structure has the following vector members:
 * - parents: OneToManyRelations (stored as ObjectID vector)
 * - daughters: OneToManyRelations (stored as ObjectID vector)
 */
namespace MCParticle {
    constexpr const char* PARENTS_MEMBER = EDM4HEP_STRINGIFY(parents);
    constexpr const char* DAUGHTERS_MEMBER = EDM4HEP_STRINGIFY(daughters);
    
    // Legacy name support (EDM4hep uses "daughters" internally)
    constexpr const char* CHILDREN_MEMBER = EDM4HEP_STRINGIFY(daughters);
}

/**
 * SimTrackerHit Vector/Association Members
 * 
 * EDM4hep SimTrackerHitData structure has:
 * - particle: OneToOneRelation (stored as ObjectID)
 */
namespace SimTrackerHit {
    constexpr const char* PARTICLE_MEMBER = EDM4HEP_STRINGIFY(particle);
}

/**
 * SimCalorimeterHit Vector/Association Members
 * 
 * EDM4hep SimCalorimeterHitData structure has:
 * - contributions: OneToManyRelations (stored as ObjectID vector)
 */
namespace SimCalorimeterHit {
    constexpr const char* CONTRIBUTIONS_MEMBER = EDM4HEP_STRINGIFY(contributions);
}

/**
 * CaloHitContribution Vector/Association Members
 * 
 * EDM4hep CaloHitContributionData structure has:
 * - particle: OneToOneRelation (stored as ObjectID)
 */
namespace CaloHitContribution {
    constexpr const char* PARTICLE_MEMBER = EDM4HEP_STRINGIFY(particle);
}

// =============================================================================
// CONVENIENCE FUNCTIONS FOR COMMON BRANCH NAME PATTERNS
// =============================================================================

/**
 * @brief Construct the MCParticle parents branch name
 * @return "_MCParticles_parents"
 */
inline std::string getMCParticleParentsBranchName() {
    return EDM4HEP_BRANCH_NAME("MCParticles", MCParticle::PARENTS_MEMBER);
}

/**
 * @brief Construct the MCParticle daughters branch name
 * @return "_MCParticles_daughters"
 */
inline std::string getMCParticleDaughtersBranchName() {
    return EDM4HEP_BRANCH_NAME("MCParticles", MCParticle::DAUGHTERS_MEMBER);
}

/**
 * @brief Construct the particle reference branch name for a tracker hit collection
 * @param collection_name The tracker hit collection name (e.g., "VXDTrackerHits")
 * @return "_<collection_name>_particle"
 */
inline std::string getTrackerHitParticleBranchName(const std::string& collection_name) {
    return EDM4HEP_BRANCH_NAME(collection_name, SimTrackerHit::PARTICLE_MEMBER);
}

/**
 * @brief Construct the contributions reference branch name for a calorimeter hit collection
 * @param collection_name The calorimeter hit collection name (e.g., "ECalBarrelHits")
 * @return "_<collection_name>_contributions"
 */
inline std::string getCaloHitContributionsBranchName(const std::string& collection_name) {
    return EDM4HEP_BRANCH_NAME(collection_name, SimCalorimeterHit::CONTRIBUTIONS_MEMBER);
}

/**
 * @brief Construct the particle reference branch name for a calorimeter hit contribution collection
 * @param contribution_collection_name The contribution collection name (e.g., "ECalBarrelHitsContributions")
 * @return "_<contribution_collection_name>_particle"
 */
inline std::string getContributionParticleBranchName(const std::string& contribution_collection_name) {
    return EDM4HEP_BRANCH_NAME(contribution_collection_name, CaloHitContribution::PARTICLE_MEMBER);
}

// =============================================================================
// DOCUMENTATION AND USAGE EXAMPLES
// =============================================================================

/**
 * USAGE EXAMPLES:
 * 
 * 1. Getting MCParticle branch names:
 * ```cpp
 * std::string parents_branch = getMCParticleParentsBranchName();
 * // Result: "_MCParticles_parents"
 * 
 * std::string daughters_branch = getMCParticleDaughtersBranchName();
 * // Result: "_MCParticles_daughters"
 * ```
 * 
 * 2. Getting tracker hit particle reference branch:
 * ```cpp
 * std::string tracker_coll = "VXDTrackerHits";
 * std::string particle_ref_branch = getTrackerHitParticleBranchName(tracker_coll);
 * // Result: "_VXDTrackerHits_particle"
 * ```
 * 
 * 3. Getting calorimeter hit contributions branch:
 * ```cpp
 * std::string calo_coll = "ECalBarrelHits";
 * std::string contrib_branch = getCaloHitContributionsBranchName(calo_coll);
 * // Result: "_ECalBarrelHits_contributions"
 * ```
 * 
 * 4. Direct use of member name constants:
 * ```cpp
 * // When you need just the member name string
 * const char* member = SimTrackerHit::PARTICLE_MEMBER;  // "particle"
 * 
 * // For custom branch name construction
 * std::string custom_branch = EDM4HEP_BRANCH_NAME("MyCollection", 
 *                                                   CaloHitContribution::PARTICLE_MEMBER);
 * // Result: "_MyCollection_particle"
 * ```
 * 
 * BENEFITS:
 * 
 * 1. Type Safety: Using EDM4HEP_STRINGIFY ensures member names are actual tokens,
 *    not arbitrary strings. If you misspell a member name, you'll get a compile error
 *    (with proper usage patterns).
 * 
 * 2. Centralization: All branch naming logic is in one place, making it easier to
 *    update if the EDM4hep convention changes.
 * 
 * 3. Self-Documentation: The macro names clearly indicate what they represent.
 * 
 * 4. Refactoring Safety: If EDM4hep member names change, you only need to update
 *    the macro definitions in this file.
 * 
 * LIMITATIONS:
 * 
 * 1. C++ preprocessor macros don't provide true compile-time verification that
 *    the stringified names match actual struct members. For that, you would need
 *    C++20 reflection (not yet available) or more complex template metaprogramming.
 * 
 * 2. The member name tokens in the macros (parents, daughters, etc.) must be
 *    kept synchronized with the actual EDM4hep member names manually.
 * 
 * FUTURE IMPROVEMENTS:
 * 
 * When C++20/26 reflection becomes widely available, this could be replaced with
 * true compile-time introspection:
 * 
 * ```cpp
 * template<typename T>
 * constexpr auto getMemberName(auto member_ptr) {
 *     return std::meta::name_of(member_ptr);
 * }
 * ```
 */

#endif // EDM4HEP_BRANCH_NAMES_H
