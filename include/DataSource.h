#pragma once

#include "StandaloneMergerConfig.h"
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
                   const std::vector<std::string>& calo_contrib_collections);
    
    // Data access
    bool hasMoreEntries() const;
    size_t getTotalEntries() const { return total_entries_; }
    size_t getCurrentEntryIndex() const { return current_entry_index_; }
    void setCurrentEntryIndex(size_t index) { current_entry_index_ = index; }
    
    // Event management
    void setEntriesNeeded(size_t entries) { entries_needed_ = entries; }
    size_t getEntriesNeeded() const { return entries_needed_; }
    bool loadNextEvent();
    
    // Time offset generation
    float generateTimeOffset(float distance, float time_slice_duration, float bunch_crossing_period, std::mt19937& rng) const;
    
    // Data merging methods
    void mergeEventData(size_t event_index, 
                       size_t particle_index_offset,
                       float time_slice_duration,
                       float bunch_crossing_period,
                       std::mt19937& rng,
                       std::vector<edm4hep::MCParticleData>& merged_mcparticles,
                       std::unordered_map<std::string, std::vector<edm4hep::SimTrackerHitData>>& merged_tracker_hits,
                       std::unordered_map<std::string, std::vector<edm4hep::SimCalorimeterHitData>>& merged_calo_hits,
                       std::unordered_map<std::string, std::vector<edm4hep::CaloHitContributionData>>& merged_calo_contributions,
                       std::vector<podio::ObjectID>& merged_mcparticle_parents_refs,
                       std::vector<podio::ObjectID>& merged_mcparticle_children_refs,
                       std::unordered_map<std::string, std::vector<podio::ObjectID>>& merged_tracker_hit_particle_refs,
                       std::unordered_map<std::string, std::vector<podio::ObjectID>>& merged_calo_contrib_particle_refs,
                       std::unordered_map<std::string, std::vector<podio::ObjectID>>& merged_calo_hit_contributions_refs);
    
    // Configuration access
    const SourceConfig& getConfig() const { return *config_; }
    const std::string& getName() const { return config_->name; }
    size_t getSourceIndex() const { return source_index_; }

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
    const std::vector<std::string>* calo_contrib_collection_names_;
    
    // Branch pointers for reading data as vectors
    std::vector<edm4hep::MCParticleData>* mcparticle_branch_;
    std::unordered_map<std::string, std::vector<edm4hep::SimTrackerHitData>*> tracker_hit_branches_;
    std::unordered_map<std::string, std::vector<edm4hep::SimCalorimeterHitData>*> calo_hit_branches_;
    std::unordered_map<std::string, std::vector<edm4hep::CaloHitContributionData>*> calo_contrib_branches_;
    std::unordered_map<std::string, std::vector<edm4hep::EventHeaderData>*> event_header_branches_;
    
    // Branch pointers for reading ObjectID references
    std::unordered_map<std::string, std::vector<podio::ObjectID>*> tracker_hit_particle_refs_;
    std::unordered_map<std::string, std::vector<podio::ObjectID>*> calo_contrib_particle_refs_;
    
    // Branch pointers for MCParticle parent-child relationships
    std::vector<podio::ObjectID>* mcparticle_parents_refs_;
    std::vector<podio::ObjectID>* mcparticle_children_refs_;
    
    // Branch pointers for SimCalorimeterHit-CaloHitContribution relationships
    std::unordered_map<std::string, std::vector<podio::ObjectID>*> calo_hit_contributions_refs_;
    
    // Private helper methods
    void setupBranches();
    void setupMCParticleBranches();
    void setupTrackerBranches();
    void setupCalorimeterBranches();
    void setupEventHeaderBranches();
    void cleanup();
    
    // Helper methods for collection name mapping
    std::string getCorrespondingContributionCollection(const std::string& calo_collection_name) const;
    std::string getCorrespondingCaloCollection(const std::string& contrib_collection_name) const;
};