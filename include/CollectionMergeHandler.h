#pragma once

#include "CollectionMetadata.h"
#include "CollectionProcessor.h"
#include <edm4hep/MCParticleData.h>
#include <edm4hep/SimTrackerHitData.h>
#include <edm4hep/SimCalorimeterHitData.h>
#include <edm4hep/CaloHitContributionData.h>
#include <podio/ObjectID.h>
#include <string>
#include <functional>
#include <map>
#include <any>

// Forward declaration
struct MergedCollections;

/**
 * Handler for merging a specific collection type.
 * Contains all the logic for:
 * - Casting from std::any
 * - Building offset maps
 * - Processing the collection
 * - Merging into destination container
 */
struct CollectionMergeHandler {
    std::string collection_type;  // e.g., "MCParticles", "SimTrackerHit", "ObjectID"
    
    // Function to build offset maps based on context
    // Parameters: time_offset, gen_status_offset, collection_offsets, one_to_many_relations, collection_name
    std::function<void(
        float time_offset,
        int gen_status_offset,
        const std::map<std::string, size_t>& collection_offsets,
        const std::map<std::string, std::vector<std::string>>& one_to_many_relations,
        const std::string& collection_name,
        std::map<std::string, float>& float_offsets,
        std::map<std::string, int>& int_offsets,
        std::map<std::string, size_t>& size_t_offsets
    )> build_offset_maps;
    
    // Function to process and merge the collection
    // Parameters: collection_data (std::any), collection_name, should_process, offset maps, merged_collections
    std::function<void(
        std::any& collection_data,
        const std::string& collection_name,
        bool should_process,
        const std::map<std::string, float>& float_offsets,
        const std::map<std::string, int>& int_offsets,
        const std::map<std::string, size_t>& size_t_offsets,
        bool already_merged,
        MergedCollections& merged_collections
    )> process_and_merge;
};

/**
 * Registry for collection merge handlers.
 * Maps collection type names to their merge handlers.
 */
class CollectionMergeRegistry {
public:
    /**
     * Initialize the registry with all known collection types.
     * Called once at startup.
     */
    static void initializeRegistry();
    
    /**
     * Get handler for a collection type.
     * Returns nullptr if not found.
     */
    static const CollectionMergeHandler* getHandler(const std::string& collection_type) {
        auto& registry = getRegistry();
        auto it = registry.find(collection_type);
        if (it != registry.end()) {
            return &it->second;
        }
        return nullptr;
    }
    
    /**
     * Register a handler for a collection type.
     */
    static void registerHandler(const std::string& collection_type, CollectionMergeHandler&& handler) {
        getRegistry()[collection_type] = std::move(handler);
    }
    
private:
    static std::map<std::string, CollectionMergeHandler>& getRegistry() {
        static std::map<std::string, CollectionMergeHandler> registry;
        return registry;
    }
};
