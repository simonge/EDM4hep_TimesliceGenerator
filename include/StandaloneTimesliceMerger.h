#pragma once

#include "StandaloneMergerConfig.h"
#include <edm4hep/MCParticleData.h>
#include <edm4hep/SimTrackerHitData.h>
#include <edm4hep/SimCalorimeterHitData.h>
#include <edm4hep/CaloHitContributionData.h>
#include <edm4hep/EventHeaderData.h>
#include <podio/ROOTWriter.h>
#include <podio/ObjectID.h>
#include <ROOT/RVec.hxx>
#include <TFile.h>
#include <TTree.h>
#include <TChain.h>
#include <random>
#include <unordered_map>
#include <vector>
#include <string>

struct SourceReader {
    std::unique_ptr<TChain> chain;
    size_t total_entries{0};
    size_t current_entry_index{0};
    size_t entries_needed{1};
    std::vector<std::string> collection_names_to_read;
    const SourceConfig* config;
    
    // Branch pointers for reading data as vectors
    std::unordered_map<std::string, std::vector<edm4hep::MCParticleData>*> mcparticle_branches;
    std::unordered_map<std::string, std::vector<edm4hep::SimTrackerHitData>*> tracker_hit_branches;
    std::unordered_map<std::string, std::vector<edm4hep::SimCalorimeterHitData>*> calo_hit_branches;
    std::unordered_map<std::string, std::vector<edm4hep::CaloHitContributionData>*> calo_contrib_branches;
    std::unordered_map<std::string, std::vector<edm4hep::EventHeaderData>*> event_header_branches;
    
    // Branch pointers for reading ObjectID references
    std::unordered_map<std::string, std::vector<podio::ObjectID>*> tracker_hit_particle_refs;
    std::unordered_map<std::string, std::vector<podio::ObjectID>*> calo_contrib_particle_refs;
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

    // State variables
    size_t events_generated;

    // Global vectors for merged data
    std::vector<edm4hep::MCParticleData> merged_mcparticles;
    std::vector<edm4hep::EventHeaderData> merged_event_headers;
    std::vector<edm4hep::EventHeaderData> merged_sub_event_headers;
    std::unordered_map<std::string, std::vector<edm4hep::SimTrackerHitData>> merged_tracker_hits;
    std::unordered_map<std::string, std::vector<edm4hep::SimCalorimeterHitData>> merged_calo_hits;
    std::unordered_map<std::string, std::vector<edm4hep::CaloHitContributionData>> merged_calo_contributions;
    
    // Global vectors for merged ObjectID references  
    std::unordered_map<std::string, std::vector<podio::ObjectID>> merged_tracker_hit_particle_refs;
    std::unordered_map<std::string, std::vector<podio::ObjectID>> merged_calo_contrib_particle_refs;

    // Collection names discovered from first source
    std::vector<std::string> tracker_collection_names;
    std::vector<std::string> calo_collection_names;
    std::vector<std::string> calo_contrib_collection_names;

    std::vector<SourceReader> initializeInputFiles();
    bool updateInputNEvents(std::vector<SourceReader>& inputs);
    void createMergedTimeslice(std::vector<SourceReader>& inputs, std::unique_ptr<TFile>& output_file, TTree* output_tree);
    void setupOutputTree(TTree* tree);
    void writeTimesliceToTree(TTree* tree);

    // Helper methods for vector-based merging logic
    void mergeEventData(SourceReader& source, size_t event_index, const SourceConfig& sourceConfig);
    float generateTimeOffset(SourceConfig sourceConfig, float distance);
    std::vector<std::string> discoverCollectionNames(SourceReader& reader, const std::string& branch_pattern);
};