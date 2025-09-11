#pragma once

#include "StandaloneMergerConfig.h"
#include "MutableRootReader.h"
#include <edm4hep/EventHeaderCollection.h>
#include <edm4hep/MCParticleCollection.h>
#include <edm4hep/SimTrackerHitCollection.h>
#include <edm4hep/SimCalorimeterHitCollection.h>
#include <edm4hep/CaloHitContributionCollection.h>
#include <podio/ROOTWriter.h>
#include <random>
#include <unordered_map>
#include <vector>
#include <string>
#include <memory>

struct SourceReader {
    std::shared_ptr<MutableRootReader> mutable_reader;
    size_t total_entries{0};
    size_t current_entry_index{0};
    size_t entries_needed{1};
    std::vector<std::string> collection_names_to_read;
    std::vector<std::string> collection_types_to_read;
    const SourceConfig* config;
};

class StandaloneTimesliceMerger {
public:
    StandaloneTimesliceMerger(const MergerConfig& config);
    
    void run();

private:
    MergerConfig m_config;
    
    // Random number generator members
    std::random_device rd;
    std::mt19937 gen;

    // Collections to merge for all instances
    std::vector<std::string> collections_to_merge = {"MCParticles", "EventHeader"};

    // Collection types to merge for hits
    std::vector<std::string> hit_collection_types = {"edm4hep::SimTrackerHit", "edm4hep::SimCalorimeterHit", "edm4hep::CaloHitContribution"};

    // Collections to merge for particles
    std::vector<std::string> particle_collection_types = {"ReconstructedParticles"};
    // Also need to handle verteces which aren't necessarily trivially named.

    // State variables
    size_t events_generated;

    std::vector<SourceReader> initializeInputFiles();
    bool updateInputNEvents(std::vector<SourceReader>& inputs);
    void createMergedTimeslice(std::vector<SourceReader>& inputs, std::unique_ptr<podio::ROOTWriter>& writer);

    // Helper methods for timeslice merging logic
    void mergeCollections(const std::unique_ptr<MutableRootReader::MutableFrame>& frame, 
                         const SourceConfig& sourceConfig,
                         edm4hep::MCParticleCollection& out_particles,
                         edm4hep::EventHeaderCollection& out_sub_event_headers,
                         std::unordered_map<std::string, edm4hep::SimTrackerHitCollection*>& out_tracker_hits,
                         std::unordered_map<std::string, edm4hep::SimCalorimeterHitCollection*>& out_calo_hits,
                         std::unordered_map<std::string, edm4hep::CaloHitContributionCollection*>& out_calo_contributions);
    
    /**
     * @brief More efficient collection merging that preserves associations better
     * 
     * This method attempts to minimize the need for manual association reattachment
     * by applying time offsets directly to mutable collections where possible.
     */
    void mergeCollectionsEfficient(std::unique_ptr<MutableRootReader::MutableFrame>& frame, 
                                 const SourceConfig& sourceConfig,
                                 std::unique_ptr<MutableRootReader::MutableFrame>& output_frame,
                                 bool is_first_frame);
    
    /**
     * @brief Apply time offset directly to collections in a frame
     */
    void applyTimeOffsetToFrame(MutableRootReader::MutableFrame& frame, 
                               float time_offset, 
                               const SourceConfig& sourceConfig);
    
    /**
     * @brief Transfer collection between frames while preserving associations
     */
    void transferCollectionBetweenFrames(MutableRootReader::MutableFrame& source_frame,
                                       MutableRootReader::MutableFrame& dest_frame,
                                       const std::string& collection_name);
    
    /**
     * @brief Zip collections together while preserving associations
     */
    void zipCollectionsWithAssociations(MutableRootReader::MutableFrame& source_frame,
                                       MutableRootReader::MutableFrame& dest_frame,
                                       const SourceConfig& sourceConfig);
                         
    float generateTimeOffset(SourceConfig sourceConfig, float distance);
    std::vector<std::string> getCollectionNames(const SourceReader& reader, const std::string& type);
};