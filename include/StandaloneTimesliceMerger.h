#pragma once

#include "StandaloneMergerConfig.h"
#include <edm4hep/EventHeaderCollection.h>
#include <edm4hep/MCParticleCollection.h>
#include <edm4hep/SimTrackerHitCollection.h>
#include <edm4hep/SimCalorimeterHitCollection.h>
#include <edm4hep/CaloHitContributionCollection.h>
#include <podio/ROOTReader.h>
#include <podio/ROOTWriter.h>
#include <podio/Frame.h>
#include <random>
#include <unordered_map>
#include <vector>
#include <string>

struct SourceReader {
    podio::ROOTReader reader;
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
    void mergeCollections(const std::unique_ptr<podio::Frame>& frame, 
                         const SourceConfig& sourceConfig,
                         edm4hep::MCParticleCollection& out_particles,
                         edm4hep::EventHeaderCollection& out_sub_event_headers,
                         std::unordered_map<std::string, edm4hep::SimTrackerHitCollection*>& out_tracker_hits,
                         std::unordered_map<std::string, edm4hep::SimCalorimeterHitCollection*>& out_calo_hits,
                         std::unordered_map<std::string, edm4hep::CaloHitContributionCollection*>& out_calo_contributions);
                         
    float generateTimeOffset(SourceConfig sourceConfig, float distance);
    std::vector<std::string> getCollectionNames(const SourceReader& reader, const std::string& type);
};