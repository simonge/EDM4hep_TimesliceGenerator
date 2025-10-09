#pragma once

#include "CollectionMetadata.h"
#include "IndexOffsetHelper.h"
#include <edm4hep/MCParticleData.h>
#include <edm4hep/SimTrackerHitData.h>
#include <edm4hep/SimCalorimeterHitData.h>
#include <edm4hep/CaloHitContributionData.h>
#include <podio/ObjectID.h>
#include <vector>
#include <string>
#include <map>

/**
 * Completely generic collection processor using type-erased metadata.
 * No type-specific methods - everything driven by CollectionMetadata registry.
 * 
 * All offsets (time, generator status, index) are treated uniformly through
 * the metadata system.
 */
class CollectionProcessor {
public:
    /**
     * Initialize the metadata registry with all known collection types.
     * This is called once at startup.
     */
    static void initializeRegistry();
    
    /**
     * Process a collection using its metadata.
     * This is the ONLY processing method - completely generic.
     * 
     * @param collection_data Pointer to the collection (type-erased as void*)
     * @param collection_name Name of the collection (e.g., "MCParticles")
     * @param offsets Map of field names to offset values
     *                e.g., {"time": time_offset, "generatorStatus": gen_status_offset,
     *                       "parents": particle_index_offset, "daughters": particle_index_offset}
     * @param already_merged Whether the data is already merged (skip some updates)
     */
    static void processCollection(
        void* collection_data,
        const std::string& collection_name,
        const std::map<std::string, float>& float_offsets,
        const std::map<std::string, int>& int_offsets,
        const std::map<std::string, size_t>& size_t_offsets,
        bool already_merged);
    
    /**
     * Process ObjectID references (generic for all ObjectID vectors)
     */
    static void processObjectIDReferences(
        std::vector<podio::ObjectID>& refs,
        size_t index_offset) {
        for (auto& ref : refs) {
            ref.index += index_offset;
        }
    }
};
