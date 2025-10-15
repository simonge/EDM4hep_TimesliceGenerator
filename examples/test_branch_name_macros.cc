/**
 * @file test_branch_name_macros.cc
 * @brief Demonstration and test program for EDM4hep branch name macros
 * 
 * This program demonstrates the use of C++ preprocessor macros to link
 * EDM4hep vector/association member names to their corresponding ROOT
 * branch names. It validates that the macro-based approach produces the
 * same results as the previous hardcoded string literals.
 */

#include "EDM4hepBranchNames.h"
#include <iostream>
#include <cassert>
#include <string>

/**
 * Test that macro-generated names match expected EDM4hep conventions
 */
void testBranchNameGeneration() {
    std::cout << "=== Testing EDM4hep Branch Name Macro Generation ===" << std::endl;
    std::cout << std::endl;
    
    // Test MCParticle branch names
    std::cout << "MCParticle Branches:" << std::endl;
    std::string parents = getMCParticleParentsBranchName();
    std::string daughters = getMCParticleDaughtersBranchName();
    
    std::cout << "  Parents:   " << parents << std::endl;
    std::cout << "  Daughters: " << daughters << std::endl;
    
    // Verify they match the expected pattern
    assert(parents == "_MCParticles_parents");
    assert(daughters == "_MCParticles_daughters");
    std::cout << "  ✓ MCParticle branch names are correct" << std::endl;
    std::cout << std::endl;
    
    // Test SimTrackerHit particle reference branch names
    std::cout << "SimTrackerHit Particle Reference Branches:" << std::endl;
    std::string vxd_particle = getTrackerHitParticleBranchName("VXDTrackerHits");
    std::string sit_particle = getTrackerHitParticleBranchName("SITTrackerHits");
    
    std::cout << "  VXDTrackerHits: " << vxd_particle << std::endl;
    std::cout << "  SITTrackerHits: " << sit_particle << std::endl;
    
    assert(vxd_particle == "_VXDTrackerHits_particle");
    assert(sit_particle == "_SITTrackerHits_particle");
    std::cout << "  ✓ TrackerHit particle reference branch names are correct" << std::endl;
    std::cout << std::endl;
    
    // Test SimCalorimeterHit contributions reference branch names
    std::cout << "SimCalorimeterHit Contributions Reference Branches:" << std::endl;
    std::string ecal_contrib = getCaloHitContributionsBranchName("ECalBarrelHits");
    std::string hcal_contrib = getCaloHitContributionsBranchName("HCalBarrelHits");
    
    std::cout << "  ECalBarrelHits: " << ecal_contrib << std::endl;
    std::cout << "  HCalBarrelHits: " << hcal_contrib << std::endl;
    
    assert(ecal_contrib == "_ECalBarrelHits_contributions");
    assert(hcal_contrib == "_HCalBarrelHits_contributions");
    std::cout << "  ✓ CaloHit contributions reference branch names are correct" << std::endl;
    std::cout << std::endl;
    
    // Test CaloHitContribution particle reference branch names
    std::cout << "CaloHitContribution Particle Reference Branches:" << std::endl;
    std::string ecal_contrib_particle = getContributionParticleBranchName("ECalBarrelHitsContributions");
    std::string hcal_contrib_particle = getContributionParticleBranchName("HCalBarrelHitsContributions");
    
    std::cout << "  ECalBarrelHitsContributions: " << ecal_contrib_particle << std::endl;
    std::cout << "  HCalBarrelHitsContributions: " << hcal_contrib_particle << std::endl;
    
    assert(ecal_contrib_particle == "_ECalBarrelHitsContributions_particle");
    assert(hcal_contrib_particle == "_HCalBarrelHitsContributions_particle");
    std::cout << "  ✓ Contribution particle reference branch names are correct" << std::endl;
    std::cout << std::endl;
}

/**
 * Test the member name constants directly
 */
void testMemberNameConstants() {
    std::cout << "=== Testing EDM4hep Member Name Constants ===" << std::endl;
    std::cout << std::endl;
    
    std::cout << "Member Name Strings:" << std::endl;
    std::cout << "  MCParticle::PARENTS_MEMBER = \"" << MCParticle::PARENTS_MEMBER << "\"" << std::endl;
    std::cout << "  MCParticle::DAUGHTERS_MEMBER = \"" << MCParticle::DAUGHTERS_MEMBER << "\"" << std::endl;
    std::cout << "  SimTrackerHit::PARTICLE_MEMBER = \"" << SimTrackerHit::PARTICLE_MEMBER << "\"" << std::endl;
    std::cout << "  SimCalorimeterHit::CONTRIBUTIONS_MEMBER = \"" << SimCalorimeterHit::CONTRIBUTIONS_MEMBER << "\"" << std::endl;
    std::cout << "  CaloHitContribution::PARTICLE_MEMBER = \"" << CaloHitContribution::PARTICLE_MEMBER << "\"" << std::endl;
    std::cout << std::endl;
    
    // Verify the member names match expected EDM4hep member names
    assert(std::string(MCParticle::PARENTS_MEMBER) == "parents");
    assert(std::string(MCParticle::DAUGHTERS_MEMBER) == "daughters");
    assert(std::string(SimTrackerHit::PARTICLE_MEMBER) == "particle");
    assert(std::string(SimCalorimeterHit::CONTRIBUTIONS_MEMBER) == "contributions");
    assert(std::string(CaloHitContribution::PARTICLE_MEMBER) == "particle");
    
    std::cout << "  ✓ All member name constants match EDM4hep data structure" << std::endl;
    std::cout << std::endl;
}

/**
 * Test custom branch name construction
 */
void testCustomBranchConstruction() {
    std::cout << "=== Testing Custom Branch Name Construction ===" << std::endl;
    std::cout << std::endl;
    
    // Use the low-level EDM4HEP_BRANCH_NAME function directly
    std::string custom1 = EDM4HEP_BRANCH_NAME("MyTrackerCollection", SimTrackerHit::PARTICLE_MEMBER);
    std::string custom2 = EDM4HEP_BRANCH_NAME("MyCaloCollection", SimCalorimeterHit::CONTRIBUTIONS_MEMBER);
    
    std::cout << "Custom Branch Names:" << std::endl;
    std::cout << "  MyTrackerCollection + particle: " << custom1 << std::endl;
    std::cout << "  MyCaloCollection + contributions: " << custom2 << std::endl;
    
    assert(custom1 == "_MyTrackerCollection_particle");
    assert(custom2 == "_MyCaloCollection_contributions");
    
    std::cout << "  ✓ Custom branch name construction works correctly" << std::endl;
    std::cout << std::endl;
}

/**
 * Demonstrate backward compatibility
 */
void demonstrateBackwardCompatibility() {
    std::cout << "=== Backward Compatibility Check ===" << std::endl;
    std::cout << std::endl;
    
    std::cout << "Comparing macro-based vs. hardcoded approach:" << std::endl;
    std::cout << std::endl;
    
    // MCParticles
    std::string macro_parents = getMCParticleParentsBranchName();
    std::string hardcoded_parents = "_MCParticles_parents";
    std::cout << "  Parents - Macro: \"" << macro_parents << "\" vs Hardcoded: \"" << hardcoded_parents << "\"" << std::endl;
    assert(macro_parents == hardcoded_parents);
    
    std::string macro_daughters = getMCParticleDaughtersBranchName();
    std::string hardcoded_daughters = "_MCParticles_daughters";
    std::cout << "  Daughters - Macro: \"" << macro_daughters << "\" vs Hardcoded: \"" << hardcoded_daughters << "\"" << std::endl;
    assert(macro_daughters == hardcoded_daughters);
    
    // TrackerHit
    std::string coll = "VXDTrackerHits";
    std::string macro_tracker = getTrackerHitParticleBranchName(coll);
    std::string hardcoded_tracker = "_" + coll + "_particle";
    std::cout << "  TrackerHit Particle - Macro: \"" << macro_tracker << "\" vs Hardcoded: \"" << hardcoded_tracker << "\"" << std::endl;
    assert(macro_tracker == hardcoded_tracker);
    
    // CaloHit
    std::string calo_coll = "ECalBarrelHits";
    std::string macro_calo = getCaloHitContributionsBranchName(calo_coll);
    std::string hardcoded_calo = "_" + calo_coll + "_contributions";
    std::cout << "  CaloHit Contributions - Macro: \"" << macro_calo << "\" vs Hardcoded: \"" << hardcoded_calo << "\"" << std::endl;
    assert(macro_calo == hardcoded_calo);
    
    // CaloHitContribution
    std::string contrib_coll = "ECalBarrelHitsContributions";
    std::string macro_contrib = getContributionParticleBranchName(contrib_coll);
    std::string hardcoded_contrib = "_" + contrib_coll + "_particle";
    std::cout << "  Contribution Particle - Macro: \"" << macro_contrib << "\" vs Hardcoded: \"" << hardcoded_contrib << "\"" << std::endl;
    assert(macro_contrib == hardcoded_contrib);
    
    std::cout << std::endl;
    std::cout << "  ✓ Macro-based approach produces identical results to hardcoded strings" << std::endl;
    std::cout << "  ✓ Code is fully backward compatible" << std::endl;
    std::cout << std::endl;
}

int main() {
    std::cout << std::endl;
    std::cout << "╔════════════════════════════════════════════════════════════════╗" << std::endl;
    std::cout << "║  EDM4hep Branch Name Macro Testing                            ║" << std::endl;
    std::cout << "╚════════════════════════════════════════════════════════════════╝" << std::endl;
    std::cout << std::endl;
    
    try {
        testMemberNameConstants();
        testBranchNameGeneration();
        testCustomBranchConstruction();
        demonstrateBackwardCompatibility();
        
        std::cout << "╔════════════════════════════════════════════════════════════════╗" << std::endl;
        std::cout << "║  ✓ All Tests Passed Successfully                              ║" << std::endl;
        std::cout << "╚════════════════════════════════════════════════════════════════╝" << std::endl;
        std::cout << std::endl;
        
        std::cout << "Summary:" << std::endl;
        std::cout << "--------" << std::endl;
        std::cout << "The macro-based approach successfully provides:" << std::endl;
        std::cout << "  • Centralized branch name definitions" << std::endl;
        std::cout << "  • Direct linkage to EDM4hep member names via tokens" << std::endl;
        std::cout << "  • Type-safe construction functions" << std::endl;
        std::cout << "  • Full backward compatibility with existing code" << std::endl;
        std::cout << "  • Self-documenting code through named constants" << std::endl;
        std::cout << std::endl;
        
        return 0;
    }
    catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
}
