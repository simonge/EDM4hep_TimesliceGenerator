#pragma once

#include "StandaloneMergerConfig.h"
#include "IndexOffsetHelper.h"
#include <edm4hep/MCParticleData.h>
#include <edm4hep/SimTrackerHitData.h>
#include <edm4hep/SimCalorimeterHitData.h>
#include <edm4hep/CaloHitContributionData.h>
#include <edm4hep/EventHeaderData.h>
#include <podio/ObjectID.h>
#include <TChain.h>
#include <memory>
#include <unordered_map>
#include <vector>
#include <string>
#include <random>

class DataSource {
public:
    DataSource(const SourceConfig& config, size_t source_index);
    ~DataSource();
    
    // Initialization
    void initialize(const std::vector<std::string>& tracker_collections,
                   const std::vector<std::string>& calo_collections,
                   const std::vector<std::string>& gp_collections);
    
    // Data access
    bool hasMoreEntries() const;
    size_t getTotalEntries() const { return total_entries_; }
    size_t getCurrentEntryIndex() const { return current_entry_index_; }
    void setCurrentEntryIndex(size_t index) { current_entry_index_ = index; }
    float getCurrentTimeOffset() const { return current_time_offset_; }

    // Event management
    void setEntriesNeeded(size_t entries) { entries_needed_ = entries; }
    size_t getEntriesNeeded() const { return entries_needed_; }
    bool loadNextEvent();
    
    // Time offset generation
    float generateTimeOffset(float distance, float time_slice_duration, float bunch_crossing_period, std::mt19937& rng) const;
    
    // New typed data merging methods
    void loadEvent(size_t event_index);
    
    std::vector<edm4hep::MCParticleData>& processMCParticles(size_t particle_index_offset,
                                                           float time_slice_duration,
                                                           float bunch_crossing_period,
                                                           std::mt19937& rng,
                                                           int totalEventsConsumed);
    
    std::vector<podio::ObjectID>& processObjectID(const std::string& collection_name, size_t index_offset, int totalEventsConsumed);
    
    std::vector<edm4hep::SimTrackerHitData>& processTrackerHits(const std::string& collection_name,
                                                              size_t particle_index_offset,
                                                              int totalEventsConsumed);
    
    std::vector<edm4hep::SimCalorimeterHitData>& processCaloHits(const std::string& collection_name,
                                                                size_t particle_index_offset,
                                                                int totalEventsConsumed);
    std::vector<edm4hep::CaloHitContributionData>& processCaloContributions(const std::string& collection_name,
                                                                           size_t particle_index_offset,
                                                                           int totalEventsConsumed);

    std::vector<std::string>& processGPBranch(const std::string& branch_name);
    std::vector<std::vector<int>>& processGPIntValues();
    std::vector<std::vector<float>>& processGPFloatValues();
    std::vector<std::vector<double>>& processGPDoubleValues();
    std::vector<std::vector<std::string>>& processGPStringValues();
    
    // Event header processing methods
    std::vector<edm4hep::EventHeaderData>& processEventHeaders(const std::string& collection_name);
    

    
    // Configuration access
    const SourceConfig& getConfig() const { return *config_; }
    const std::string& getName() const { return config_->name; }
    size_t getSourceIndex() const { return source_index_; }
    
    // Status and diagnostics
    void printStatus() const;
    bool isInitialized() const { return chain_ != nullptr; }

private:
    // Configuration
    const SourceConfig* config_;
    size_t source_index_;
    
    // ROOT chain and state
    std::unique_ptr<TChain> chain_;
    size_t total_entries_;
    size_t current_entry_index_;
    size_t entries_needed_;
    
    // Collection names (references to shared data)
    const std::vector<std::string>* tracker_collection_names_;
    const std::vector<std::string>* calo_collection_names_;
    const std::vector<std::string>* gp_collection_names_;
    
    // Branch pointers for reading data as vectors
    std::vector<edm4hep::MCParticleData>* mcparticle_branch_;
    std::unordered_map<std::string, std::vector<edm4hep::SimTrackerHitData>*> tracker_hit_branches_;
    std::unordered_map<std::string, std::vector<edm4hep::SimCalorimeterHitData>*> calo_hit_branches_;
    std::unordered_map<std::string, std::vector<edm4hep::CaloHitContributionData>*> calo_contrib_branches_;
    std::unordered_map<std::string, std::vector<edm4hep::EventHeaderData>*> event_header_branches_;
    
    // Branch pointers for reading ObjectID references - consolidated into single map
    std::unordered_map<std::string, std::vector<podio::ObjectID>*> objectid_branches_;
    
    // Branch pointers for reading GP (Global Parameter) branches
    std::unordered_map<std::string, std::vector<std::string>*> gp_key_branches_;
    std::vector<std::vector<int>>* gp_int_branch_;
    std::vector<std::vector<float>>* gp_float_branch_;
    std::vector<std::vector<double>>* gp_double_branch_;
    std::vector<std::vector<std::string>>* gp_string_branch_;

    // Current event processing state
    float current_time_offset_;
    size_t current_particle_index_offset_;
    
    // Private helper methods
    void setupBranches();
    void setupMCParticleBranches();
    void setupTrackerBranches();
    void setupCalorimeterBranches();
    void setupEventHeaderBranches();
    void setupGPBranches();
    void cleanup();
    
    // Helper methods for physics calculations
    float calculateBeamDistance(const std::vector<edm4hep::MCParticleData>& particles) const;
    
    // Helper methods for collection name mapping
    std::string getCorrespondingContributionCollection(const std::string& calo_collection_name) const;
    std::string getCorrespondingCaloCollection(const std::string& contrib_collection_name) const;
};