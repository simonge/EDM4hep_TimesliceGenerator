#pragma once

#include <edm4hep/MCParticleData.h>
#include <edm4hep/SimCalorimeterHitData.h>
#include <string>
#include <vector>
#include <map>
#include <functional>
#include <algorithm>

/**
 * Helper class to apply index offsets to EDM4hep data structures.
 * This eliminates the need to hardcode field names for each collection type.
 * 
 * The helper provides a centralized location for managing which fields in each
 * EDM4hep data type require index offsets. This makes it easier to:
 * - Add support for new collection types
 * - Maintain consistency across the codebase
 * - Reduce code duplication
 * - Automatically infer offset requirements from branch structure
 * 
 * Usage example:
 *   // Get metadata about which fields need offsets
 *   auto metadata = IndexOffsetHelper::getMCParticleOffsetMetadata();
 *   // Apply offsets using the helper
 *   IndexOffsetHelper::applyMCParticleOffsets(particles, offset);
 * 
 * To add support for a new collection type:
 *   1. Add a static apply function (e.g., applyNewCollectionOffsets)
 *   2. Add a metadata function (e.g., getNewCollectionOffsetMetadata)
 *   3. Update getAllOffsetMetadata() to include the new metadata
 */
class IndexOffsetHelper {
public:
    /**
     * Metadata about which fields need offsets for a given collection type.
     * Each entry is a field name prefix (e.g., "parents" for parents_begin/parents_end).
     */
    struct OffsetFieldMetadata {
        std::string collection_type;
        std::vector<std::string> offset_field_prefixes;
        
        // Human-readable description of what this collection type is
        std::string description;
    };
    
    /**
     * Apply index offsets to MCParticle data.
     * Applies offsets to: parents_begin, parents_end, daughters_begin, daughters_end
     * 
     * These fields are indices into the ObjectID vectors for parent and daughter particles.
     * When merging events, particle indices need to be adjusted to account for particles
     * from previous events.
     * 
     * @param particles Vector of MCParticle data objects
     * @param offset The offset to add to index fields
     */
    static void applyMCParticleOffsets(std::vector<edm4hep::MCParticleData>& particles, size_t offset) {
        for (auto& particle : particles) {
            particle.parents_begin += offset;
            particle.parents_end += offset;
            particle.daughters_begin += offset;
            particle.daughters_end += offset;
        }
    }
    
    /**
     * Apply index offsets to SimCalorimeterHit data.
     * Applies offsets to: contributions_begin, contributions_end
     * 
     * These fields are indices into the CaloHitContribution vector.
     * When merging events, contribution indices need to be adjusted to account for
     * contributions from previous events.
     * 
     * @param hits Vector of SimCalorimeterHit data objects
     * @param offset The offset to add to index fields
     */
    static void applyCaloHitOffsets(std::vector<edm4hep::SimCalorimeterHitData>& hits, size_t offset) {
        for (auto& hit : hits) {
            hit.contributions_begin += offset;
            hit.contributions_end += offset;
        }
    }
    
    /**
     * Get metadata about which fields need offsets for MCParticle collections.
     * This can be used to understand the structure without hardcoding in multiple places.
     * 
     * @return Metadata describing the offset requirements for MCParticle collections
     */
    static OffsetFieldMetadata getMCParticleOffsetMetadata() {
        return {
            "MCParticles", 
            {"parents", "daughters"},
            "MC truth particles with parent-child relationships"
        };
    }
    
    /**
     * Get metadata about which fields need offsets for SimCalorimeterHit collections.
     * This can be used to understand the structure without hardcoding in multiple places.
     * 
     * @return Metadata describing the offset requirements for SimCalorimeterHit collections
     */
    static OffsetFieldMetadata getCaloHitOffsetMetadata() {
        return {
            "SimCalorimeterHit", 
            {"contributions"},
            "Simulated calorimeter hits with energy contributions"
        };
    }
    
    /**
     * Get all registered offset metadata.
     * This provides a complete map of all collection types and their offset requirements.
     * 
     * @return Vector of all offset metadata for all supported collection types
     */
    static std::vector<OffsetFieldMetadata> getAllOffsetMetadata() {
        return {
            getMCParticleOffsetMetadata(),
            getCaloHitOffsetMetadata()
        };
    }
    
    /**
     * Infer which fields need offsets for a collection based on its ObjectID branch names.
     * 
     * Given a collection name and a list of ObjectID branch names, this function
     * extracts the field names that need offsets.
     * 
     * Example: For "MCParticles" with branches ["_MCParticles_parents", "_MCParticles_daughters"]
     *          Returns: ["parents", "daughters"]
     * 
     * This allows the system to automatically determine offset requirements by analyzing
     * the ROOT branch structure rather than hardcoding the information.
     * 
     * @param collection_name The name of the collection (e.g., "MCParticles")
     * @param objectid_branch_names List of ObjectID branch names
     * @return Vector of field name prefixes that need offsets
     */
    static std::vector<std::string> inferOffsetFieldsFromBranches(
        const std::string& collection_name,
        const std::vector<std::string>& objectid_branch_names) 
    {
        std::vector<std::string> field_prefixes;
        std::string prefix = "_" + collection_name + "_";
        
        for (const auto& branch_name : objectid_branch_names) {
            // Check if this branch belongs to the collection
            if (branch_name.find(prefix) == 0) {
                // Extract the field name after the prefix
                std::string field_name = branch_name.substr(prefix.length());
                field_prefixes.push_back(field_name);
            }
        }
        
        return field_prefixes;
    }
    
    /**
     * Create an OffsetFieldMetadata by inferring from ObjectID branches.
     * 
     * This enables automatic discovery of offset requirements from the file structure,
     * reducing the need for hardcoded configuration.
     * 
     * @param collection_name The name of the collection
     * @param objectid_branch_names List of all ObjectID branch names
     * @return Metadata describing the offset requirements for this collection
     */
    static OffsetFieldMetadata createMetadataFromBranches(
        const std::string& collection_name,
        const std::vector<std::string>& objectid_branch_names)
    {
        OffsetFieldMetadata metadata;
        metadata.collection_type = collection_name;
        metadata.offset_field_prefixes = inferOffsetFieldsFromBranches(collection_name, objectid_branch_names);
        metadata.description = "Inferred from branch structure";
        return metadata;
    }
};
