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
 */
class IndexOffsetHelper {
public:
    /**
     * Apply index offsets to a vector of data objects.
     * The offset is applied to all fields that represent index ranges (_begin and _end fields).
     * 
     * @tparam T The data type (e.g., edm4hep::MCParticleData)
     * @param data Vector of data objects to update
     * @param offset The offset to add to index fields
     * @param field_names List of field name pairs (begin_field, end_field) that need offsets
     */
    template<typename T>
    static void applyIndexOffsets(std::vector<T>& data, size_t offset, 
                                   const std::vector<std::pair<std::string, std::string>>& field_names);
    
    /**
     * Get the list of index offset field pairs for MCParticleData.
     * Returns pairs of (begin_field, end_field) names.
     */
    static std::vector<std::pair<std::string, std::string>> getMCParticleOffsetFields() {
        return {
            {"parents", "parents"},      // parents_begin, parents_end
            {"daughters", "daughters"}   // daughters_begin, daughters_end
        };
    }
    
    /**
     * Get the list of index offset field pairs for SimCalorimeterHitData.
     * Returns pairs of (begin_field, end_field) names.
     */
    static std::vector<std::pair<std::string, std::string>> getCaloHitOffsetFields() {
        return {
            {"contributions", "contributions"}  // contributions_begin, contributions_end
        };
    }
    
    /**
     * Apply index offsets to MCParticle data.
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
     */
    static void applyCaloHitOffsets(std::vector<edm4hep::SimCalorimeterHitData>& hits, size_t offset) {
        for (auto& hit : hits) {
            hit.contributions_begin += offset;
            hit.contributions_end += offset;
        }
    }
};
