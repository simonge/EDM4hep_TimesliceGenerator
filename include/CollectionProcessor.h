#pragma once

#include "IndexOffsetHelper.h"
#include "StandaloneMergerConfig.h"
#include <edm4hep/MCParticleData.h>
#include <edm4hep/SimTrackerHitData.h>
#include <edm4hep/SimCalorimeterHitData.h>
#include <edm4hep/CaloHitContributionData.h>
#include <podio/ObjectID.h>
#include <vector>
#include <string>
#include <map>
#include <random>

/**
 * CollectionProcessor handles all offset applications for collections.
 * It uses the OneToMany relations map to apply offsets generically.
 * 
 * This centralizes all processing logic that was previously scattered
 * across DataSource process methods.
 */
class CollectionProcessor {
public:
    /**
     * Metadata for processing a collection - what offsets to apply
     */
    struct ProcessingInfo {
        bool apply_time_offset = false;
        bool apply_generator_status_offset = false;
        bool apply_index_offsets = false;
        std::vector<std::string> index_offset_fields;  // From OneToMany relations
    };
    
    /**
     * Apply time offset to MCParticles
     */
    static void applyTimeOffset(std::vector<edm4hep::MCParticleData>& particles, float time_offset) {
        for (auto& particle : particles) {
            particle.time += time_offset;
        }
    }
    
    /**
     * Apply time offset to tracker hits
     */
    static void applyTimeOffset(std::vector<edm4hep::SimTrackerHitData>& hits, float time_offset) {
        for (auto& hit : hits) {
            hit.time += time_offset;
        }
    }
    
    /**
     * Apply time offset to calo contributions
     */
    static void applyTimeOffset(std::vector<edm4hep::CaloHitContributionData>& contribs, float time_offset) {
        for (auto& contrib : contribs) {
            contrib.time += time_offset;
        }
    }
    
    /**
     * Apply generator status offset to MCParticles
     */
    static void applyGeneratorStatusOffset(std::vector<edm4hep::MCParticleData>& particles, int status_offset) {
        for (auto& particle : particles) {
            particle.generatorStatus += status_offset;
        }
    }
    
    /**
     * Apply index offset to ObjectID references
     */
    static void applyIndexOffset(std::vector<podio::ObjectID>& refs, size_t index_offset) {
        for (auto& ref : refs) {
            ref.index += index_offset;
        }
    }
    
    /**
     * Process MCParticles with all applicable offsets
     */
    static void processMCParticles(
        std::vector<edm4hep::MCParticleData>& particles,
        float time_offset,
        int generator_status_offset,
        size_t index_offset,
        const std::vector<std::string>& index_offset_fields,
        bool already_merged)
    {
        if (!already_merged) {
            applyTimeOffset(particles, time_offset);
            applyGeneratorStatusOffset(particles, generator_status_offset);
        }
        
        // Apply index offsets using the generic helper
        if (!index_offset_fields.empty()) {
            IndexOffsetHelper::applyMCParticleOffsets(particles, index_offset, index_offset_fields);
        }
    }
    
    /**
     * Process tracker hits with time offset
     */
    static void processTrackerHits(
        std::vector<edm4hep::SimTrackerHitData>& hits,
        float time_offset,
        bool already_merged)
    {
        if (!already_merged) {
            applyTimeOffset(hits, time_offset);
        }
    }
    
    /**
     * Process calo hits with index offsets
     */
    static void processCaloHits(
        std::vector<edm4hep::SimCalorimeterHitData>& hits,
        size_t index_offset,
        const std::vector<std::string>& index_offset_fields)
    {
        if (!index_offset_fields.empty()) {
            IndexOffsetHelper::applyCaloHitOffsets(hits, index_offset, index_offset_fields);
        }
    }
    
    /**
     * Process calo contributions with time offset
     */
    static void processCaloContributions(
        std::vector<edm4hep::CaloHitContributionData>& contribs,
        float time_offset,
        bool already_merged)
    {
        if (!already_merged) {
            applyTimeOffset(contribs, time_offset);
        }
    }
};
