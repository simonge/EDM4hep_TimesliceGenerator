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

    // Event management
    void setEntriesNeeded(size_t entries) { entries_needed_ = entries; }
    size_t getEntriesNeeded() const { return entries_needed_; }
    bool loadNextEvent();
    
    // Event loading
    void loadEvent(size_t event_index);
    
    // Collection accessor methods - return raw references without processing
    std::vector<edm4hep::MCParticleData>& getMCParticles() { return *mcparticle_branch_; }
    std::vector<podio::ObjectID>& getObjectIDCollection(const std::string& collection_name) { return *objectid_branches_[collection_name]; }
    std::vector<edm4hep::SimTrackerHitData>& getTrackerHits(const std::string& collection_name) { return *tracker_hit_branches_[collection_name]; }
    std::vector<edm4hep::SimCalorimeterHitData>& getCaloHits(const std::string& collection_name) { return *calo_hit_branches_[collection_name]; }
    std::vector<edm4hep::CaloHitContributionData>& getCaloContributions(const std::string& collection_name) { return *calo_contrib_branches_[collection_name]; }
    std::vector<edm4hep::EventHeaderData>& getEventHeaders(const std::string& collection_name) { return *event_header_branches_[collection_name]; }
    
    // GP branch accessors
    std::vector<std::string>& getGPBranch(const std::string& branch_name) { return *gp_key_branches_[branch_name]; }
    std::vector<std::vector<int>>& getGPIntValues() { return *gp_int_branch_; }
    std::vector<std::vector<float>>& getGPFloatValues() { return *gp_float_branch_; }
    std::vector<std::vector<double>>& getGPDoubleValues() { return *gp_double_branch_; }
    std::vector<std::vector<std::string>>& getGPStringValues() { return *gp_string_branch_; }
    
    // Helper methods for offset calculations (used by StandaloneTimesliceMerger)
    float generateTimeOffset(float distance, float time_slice_duration, float bunch_crossing_period, std::mt19937& rng) const;
    float calculateBeamDistance(const std::vector<edm4hep::MCParticleData>& particles) const;
    
    // Access to discovered OneToMany relations
    const std::map<std::string, std::vector<std::string>>& getOneToManyRelations() const { return one_to_many_relations_; }
    

    
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

    // Runtime-discovered OneToMany relation metadata
    // Maps collection name to list of field names that need index offsets
    std::map<std::string, std::vector<std::string>> one_to_many_relations_;

    // Private helper methods
    void setupBranches();
    void setupMCParticleBranches();
    void setupTrackerBranches();
    void setupCalorimeterBranches();
    void setupEventHeaderBranches();
    void setupGPBranches();
    void discoverOneToManyRelations();
    void cleanup();
    
    // Helper methods for collection name mapping
    std::string getCorrespondingContributionCollection(const std::string& calo_collection_name) const;
    std::string getCorrespondingCaloCollection(const std::string& contrib_collection_name) const;
};