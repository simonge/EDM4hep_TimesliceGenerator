#pragma once

#include <edm4hep/MCParticleData.h>
#include <edm4hep/SimTrackerHitData.h>
#include <edm4hep/SimCalorimeterHitData.h>
#include <edm4hep/CaloHitContributionData.h>
#include <edm4hep/EventHeaderData.h>
#include <podio/ObjectID.h>
#include <type_traits>
#include <concepts>
#include <string>
#include <map>
#include <any>
#include <variant>
#include <functional>

/**
 * C++20 concepts-based collection processing with automatic field detection.
 * Uses compile-time type introspection to determine which fields need updating.
 * All branches treated uniformly - no hardcoded special cases.
 */

// Concept to check if a type has a 'time' member
template<typename T>
concept HasTime = requires(T t) {
    { t.time } -> std::convertible_to<float>;
};

// Concept to check if a type has a 'generatorStatus' member
template<typename T>
concept HasGeneratorStatus = requires(T t) {
    { t.generatorStatus } -> std::convertible_to<int>;
};

// Concept to check if a type has parents_begin/end
template<typename T>
concept HasParents = requires(T t) {
    { t.parents_begin } -> std::convertible_to<unsigned int>;
    { t.parents_end } -> std::convertible_to<unsigned int>;
};

// Concept to check if a type has daughters_begin/end
template<typename T>
concept HasDaughters = requires(T t) {
    { t.daughters_begin } -> std::convertible_to<unsigned int>;
    { t.daughters_end } -> std::convertible_to<unsigned int>;
};

// Concept to check if a type has contributions_begin/end
template<typename T>
concept HasContributions = requires(T t) {
    { t.contributions_begin } -> std::convertible_to<unsigned int>;
    { t.contributions_end } -> std::convertible_to<unsigned int>;
};

/**
 * Apply offsets to a collection based on its type traits.
 * The offsets_map contains branch names and their current sizes.
 * Field names map to branch names (e.g., "parents" -> "_MCParticles_parents").
 */
template<typename T>
void applyOffsets(std::vector<T>& collection,
                 float time_offset,
                 int gen_status_offset,
                 const std::map<std::string, size_t>& offsets_map,
                 bool already_merged) {
    
    for (auto& item : collection) {
        // Apply time offset if type has time member and not already merged
        if constexpr (HasTime<T>) {
            if (!already_merged) {
                item.time += time_offset;
            }
        }
        
        // Apply generator status offset if type has generatorStatus and not already merged
        if constexpr (HasGeneratorStatus<T>) {
            if (!already_merged) {
                item.generatorStatus += gen_status_offset;
            }
        }
        
        // Apply parent index offsets if type has parents
        // The offset comes from the _parents branch, not the main collection
        if constexpr (HasParents<T>) {
            auto it = offsets_map.find("parents");
            if (it != offsets_map.end()) {
                item.parents_begin += it->second;
                item.parents_end += it->second;
            }
        }
        
        // Apply daughter index offsets if type has daughters
        // The offset comes from the _daughters branch, not the main collection
        if constexpr (HasDaughters<T>) {
            auto it = offsets_map.find("daughters");
            if (it != offsets_map.end()) {
                item.daughters_begin += it->second;
                item.daughters_end += it->second;
            }
        }
        
        // Apply contribution index offsets if type has contributions
        if constexpr (HasContributions<T>) {
            auto it = offsets_map.find("contributions");
            if (it != offsets_map.end()) {
                item.contributions_begin += it->second;
                item.contributions_end += it->second;
            }
        }
    }
}

/**
 * Specialization for ObjectID references - apply index offset
 */
inline void applyOffsets(std::vector<podio::ObjectID>& refs,
                        float /*time_offset*/,
                        int /*gen_status_offset*/,
                        const std::map<std::string, size_t>& offsets_map,
                        bool /*already_merged*/) {
    // For ObjectID vectors, look for a "target" entry in the map
    auto it = offsets_map.find("target");
    if (it != offsets_map.end()) {
        for (auto& ref : refs) {
            ref.index += it->second;
        }
    }
}

/**
 * Type alias for all possible collection types
 */
using CollectionVariant = std::variant<
    std::vector<edm4hep::MCParticleData>*,
    std::vector<edm4hep::SimTrackerHitData>*,
    std::vector<edm4hep::SimCalorimeterHitData>*,
    std::vector<edm4hep::CaloHitContributionData>*,
    std::vector<edm4hep::EventHeaderData>*,
    std::vector<podio::ObjectID>*,
    std::vector<std::string>*,
    std::vector<std::vector<int>>*,
    std::vector<std::vector<float>>*,
    std::vector<std::vector<double>>*,
    std::vector<std::vector<std::string>>*
>;

/**
 * Generic processing function that works with std::any.
 * Automatically detects type and applies appropriate offsets.
 */
inline void processCollectionGeneric(std::any& collection_data,
                                    const std::string& collection_type,
                                    float time_offset,
                                    int gen_status_offset,
                                    const std::map<std::string, size_t>& offsets_map,
                                    bool already_merged) {
    
    if (collection_type == "MCParticles") {
        auto* coll = std::any_cast<std::vector<edm4hep::MCParticleData>>(&collection_data);
        if (coll) applyOffsets(*coll, time_offset, gen_status_offset, offsets_map, already_merged);
    }
    else if (collection_type == "SimTrackerHit") {
        auto* coll = std::any_cast<std::vector<edm4hep::SimTrackerHitData>>(&collection_data);
        if (coll) applyOffsets(*coll, time_offset, gen_status_offset, offsets_map, already_merged);
    }
    else if (collection_type == "SimCalorimeterHit") {
        auto* coll = std::any_cast<std::vector<edm4hep::SimCalorimeterHitData>>(&collection_data);
        if (coll) applyOffsets(*coll, time_offset, gen_status_offset, offsets_map, already_merged);
    }
    else if (collection_type == "CaloHitContribution") {
        auto* coll = std::any_cast<std::vector<edm4hep::CaloHitContributionData>>(&collection_data);
        if (coll) applyOffsets(*coll, time_offset, gen_status_offset, offsets_map, already_merged);
    }
    else if (collection_type == "ObjectID") {
        auto* coll = std::any_cast<std::vector<podio::ObjectID>>(&collection_data);
        if (coll) applyOffsets(*coll, time_offset, gen_status_offset, offsets_map, already_merged);
    }
    // EventHeaders and GP collections don't need offset processing
}
