/**
 * Example demonstrating how to use IndexOffsetHelper to automatically
 * infer offset requirements from branch structure.
 * 
 * This example shows how the system can determine which fields need
 * offsets without hardcoding, by analyzing the ObjectID branch names.
 */

#include "IndexOffsetHelper.h"
#include <iostream>
#include <vector>
#include <string>

int main() {
    std::cout << "=== IndexOffsetHelper Example ===" << std::endl;
    std::cout << std::endl;
    
    // Example 1: Get predefined metadata
    std::cout << "1. Predefined Metadata for MCParticles:" << std::endl;
    auto mcparticle_metadata = IndexOffsetHelper::getMCParticleOffsetMetadata();
    std::cout << "   Collection: " << mcparticle_metadata.collection_type << std::endl;
    std::cout << "   Description: " << mcparticle_metadata.description << std::endl;
    std::cout << "   Offset fields:" << std::endl;
    for (const auto& field : mcparticle_metadata.offset_field_prefixes) {
        std::cout << "     - " << field << "_begin, " << field << "_end" << std::endl;
    }
    std::cout << std::endl;
    
    // Example 2: Get predefined metadata for CaloHits
    std::cout << "2. Predefined Metadata for SimCalorimeterHit:" << std::endl;
    auto calohit_metadata = IndexOffsetHelper::getCaloHitOffsetMetadata();
    std::cout << "   Collection: " << calohit_metadata.collection_type << std::endl;
    std::cout << "   Description: " << calohit_metadata.description << std::endl;
    std::cout << "   Offset fields:" << std::endl;
    for (const auto& field : calohit_metadata.offset_field_prefixes) {
        std::cout << "     - " << field << "_begin, " << field << "_end" << std::endl;
    }
    std::cout << std::endl;
    
    // Example 3: Infer offset fields from branch structure
    std::cout << "3. Inferring offset fields from branch names:" << std::endl;
    
    // Simulate a list of ObjectID branch names from a ROOT file
    std::vector<std::string> all_branches = {
        "_MCParticles_parents",
        "_MCParticles_daughters",
        "_VertexBarrelCollection_particle",
        "_ECalBarrelCollection_contributions",
        "_HCalBarrelCollection_contributions"
    };
    
    std::cout << "   Available ObjectID branches:" << std::endl;
    for (const auto& branch : all_branches) {
        std::cout << "     - " << branch << std::endl;
    }
    std::cout << std::endl;
    
    // Infer MCParticles offset fields
    std::cout << "   Inferred offset fields for MCParticles:" << std::endl;
    auto inferred_fields = IndexOffsetHelper::inferOffsetFieldsFromBranches("MCParticles", all_branches);
    for (const auto& field : inferred_fields) {
        std::cout << "     - " << field << "_begin, " << field << "_end" << std::endl;
    }
    std::cout << std::endl;
    
    // Infer ECalBarrelCollection offset fields
    std::cout << "   Inferred offset fields for ECalBarrelCollection:" << std::endl;
    auto ecal_fields = IndexOffsetHelper::inferOffsetFieldsFromBranches("ECalBarrelCollection", all_branches);
    for (const auto& field : ecal_fields) {
        std::cout << "     - " << field << "_begin, " << field << "_end" << std::endl;
    }
    std::cout << std::endl;
    
    // Example 4: Create metadata from branches
    std::cout << "4. Creating complete metadata from branch inference:" << std::endl;
    auto inferred_metadata = IndexOffsetHelper::createMetadataFromBranches("MCParticles", all_branches);
    std::cout << "   Collection: " << inferred_metadata.collection_type << std::endl;
    std::cout << "   Description: " << inferred_metadata.description << std::endl;
    std::cout << "   Offset fields: ";
    for (const auto& field : inferred_metadata.offset_field_prefixes) {
        std::cout << field << " ";
    }
    std::cout << std::endl;
    std::cout << std::endl;
    
    // Example 5: Show all registered metadata
    std::cout << "5. All registered offset metadata:" << std::endl;
    auto all_metadata = IndexOffsetHelper::getAllOffsetMetadata();
    for (const auto& metadata : all_metadata) {
        std::cout << "   " << metadata.collection_type << ":" << std::endl;
        std::cout << "     Description: " << metadata.description << std::endl;
        std::cout << "     Fields: ";
        for (const auto& field : metadata.offset_field_prefixes) {
            std::cout << field << " ";
        }
        std::cout << std::endl;
    }
    
    std::cout << std::endl;
    std::cout << "=== Example Complete ===" << std::endl;
    
    return 0;
}
