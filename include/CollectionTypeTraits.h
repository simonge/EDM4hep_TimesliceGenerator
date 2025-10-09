#pragma once

#include <edm4hep/MCParticleData.h>
#include <edm4hep/SimTrackerHitData.h>
#include <edm4hep/SimCalorimeterHitData.h>
#include <edm4hep/CaloHitContributionData.h>
#include <podio/ObjectID.h>
#include <type_traits>
#include <concepts>

/**
 * Simple C++20 type traits for collection processing.
 * Uses concepts and compile-time detection to automatically
 * handle different collection types without manual registration.
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

// Concept to check if a type has begin/end index members (for OneToMany relations)
template<typename T>
concept HasIndexRange = requires(T t) {
    { t.parents_begin } -> std::convertible_to<unsigned int>;
    { t.parents_end } -> std::convertible_to<unsigned int>;
};

// Concept to check if a type has contributions_begin/end
template<typename T>
concept HasContributions = requires(T t) {
    { t.contributions_begin } -> std::convertible_to<unsigned int>;
    { t.contributions_end } -> std::convertible_to<unsigned int>;
};

/**
 * Apply offsets to a collection based on its type traits.
 * Uses if constexpr for compile-time branching.
 */
template<typename T>
void applyOffsets(std::vector<T>& collection,
                 float time_offset,
                 int gen_status_offset,
                 size_t particle_index_offset,
                 size_t contrib_index_offset,
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
        
        // Apply particle index offsets if type has parent/daughter indices
        if constexpr (HasIndexRange<T>) {
            item.parents_begin += particle_index_offset;
            item.parents_end += particle_index_offset;
            item.daughters_begin += particle_index_offset;
            item.daughters_end += particle_index_offset;
        }
        
        // Apply contribution index offsets if type has contributions
        if constexpr (HasContributions<T>) {
            item.contributions_begin += contrib_index_offset;
            item.contributions_end += contrib_index_offset;
        }
    }
}

/**
 * Specialization for ObjectID references - just apply index offset
 */
inline void applyOffsets(std::vector<podio::ObjectID>& refs,
                        float /*time_offset*/,
                        int /*gen_status_offset*/,
                        size_t index_offset,
                        size_t /*contrib_index_offset*/,
                        bool /*already_merged*/) {
    for (auto& ref : refs) {
        ref.index += index_offset;
    }
}

/**
 * Type-safe visitor pattern for std::any collections.
 * Automatically casts and applies offsets based on collection type.
 */
template<typename Func>
void visitCollection(std::any& collection_data,
                    const std::string& collection_type,
                    Func&& func) {
    
    if (collection_type == "MCParticles") {
        auto* coll = std::any_cast<std::vector<edm4hep::MCParticleData>>(&collection_data);
        if (coll) func(*coll);
    }
    else if (collection_type == "SimTrackerHit") {
        auto* coll = std::any_cast<std::vector<edm4hep::SimTrackerHitData>>(&collection_data);
        if (coll) func(*coll);
    }
    else if (collection_type == "SimCalorimeterHit") {
        auto* coll = std::any_cast<std::vector<edm4hep::SimCalorimeterHitData>>(&collection_data);
        if (coll) func(*coll);
    }
    else if (collection_type == "CaloHitContribution") {
        auto* coll = std::any_cast<std::vector<edm4hep::CaloHitContributionData>>(&collection_data);
        if (coll) func(*coll);
    }
    else if (collection_type == "ObjectID") {
        auto* coll = std::any_cast<std::vector<podio::ObjectID>>(&collection_data);
        if (coll) func(*coll);
    }
}
