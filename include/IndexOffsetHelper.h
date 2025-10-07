#pragma once

#include <edm4hep/MCParticleData.h>
#include <edm4hep/SimCalorimeterHitData.h>
#include <string>
#include <vector>
#include <map>
#include <functional>

/**
 * Helper class to apply index offsets to EDM4hep data structures.
 * This eliminates the need to hardcode field names for each collection type.
 * 
 * The helper provides a centralized location for managing which fields in each
 * EDM4hep data type require index offsets. This makes it easier to:
 * - Add support for new collection types
 * - Maintain consistency across the codebase
 * - Reduce code duplication
 */
class IndexOffsetHelper {
public:
    /**
     * Metadata about which fields need offsets for a given collection type.
     * Each entry is a pair of field name prefix (e.g., "parents" for parents_begin/parents_end).
     */
    struct OffsetFieldMetadata {
        std::string collection_type;
        std::vector<std::string> offset_field_prefixes;
    };
    
    /**
     * Apply index offsets to MCParticle data.
     * Applies offsets to: parents_begin, parents_end, daughters_begin, daughters_end
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
     */
    static OffsetFieldMetadata getMCParticleOffsetMetadata() {
        return {"MCParticles", {"parents", "daughters"}};
    }
    
    /**
     * Get metadata about which fields need offsets for SimCalorimeterHit collections.
     * This can be used to understand the structure without hardcoding in multiple places.
     */
    static OffsetFieldMetadata getCaloHitOffsetMetadata() {
        return {"SimCalorimeterHit", {"contributions"}};
    }
    
    /**
     * Get all registered offset metadata.
     * This provides a complete map of all collection types and their offset requirements.
     */
    static std::vector<OffsetFieldMetadata> getAllOffsetMetadata() {
        return {
            getMCParticleOffsetMetadata(),
            getCaloHitOffsetMetadata()
        };
    }
};
