#pragma once

#include "StandaloneMergerConfig.h"
#include "BranchRelationshipMapper.h"
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
                   const std::vector<std::string>& gp_collections,
                   const BranchRelationshipMapper* relationship_mapper);
    
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
    
    // Generic data processing methods
    void loadEvent(size_t event_index);
    
    /**
     * @brief Generic method to process any collection type
     * 
     * Uses metadata from BranchRelationshipMapper to determine what processing is needed:
     * - Updates time fields if collection has them
     * - Updates index range fields (begin/end) if collection has them
     * 
     * @param collection_name Name of the collection to process
     * @param index_offset Offset to add to index values
     * @param time_offset Time offset to add (if collection has time field)
     * @param totalEventsConsumed Total events processed so far
     * @return Pointer to the processed data (caller must cast to appropriate type)
     */
    void* processCollection(const std::string& collection_name, 
                           size_t index_offset,
                           float time_offset,
                           int totalEventsConsumed);
    
    /**
     * @brief Template helper to get processed collection data with proper type
     */
    template<typename T>
    std::vector<T>& getProcessedCollection(const std::string& collection_name,
                                          size_t index_offset,
                                          float time_offset,
                                          int totalEventsConsumed) {
        void* ptr = processCollection(collection_name, index_offset, time_offset, totalEventsConsumed);
        return *static_cast<std::vector<T>*>(ptr);
    }
    
    /**
     * @brief Process ObjectID relationship branches
     * 
     * Updates indices in ObjectID vectors by adding the offset
     * 
     * @param branch_name Name of the relationship branch
     * @param index_offset Offset to add to indices
     * @param totalEventsConsumed Total events processed so far
     * @return Reference to the processed ObjectID vector
     */
    std::vector<podio::ObjectID>& processObjectID(const std::string& branch_name, size_t index_offset, int totalEventsConsumed);

    /**
     * @brief Get generic branch data without processing
     * Used for collections that don't need time/index updates
     */
    void* getBranchData(const std::string& branch_name);
    
    std::vector<std::string>& processGPBranch(const std::string& branch_name);
    std::vector<std::vector<int>>& processGPIntValues();
    std::vector<std::vector<float>>& processGPFloatValues();
    std::vector<std::vector<double>>& processGPDoubleValues();
    std::vector<std::vector<std::string>>& processGPStringValues();
    

    
    // Configuration access
    const SourceConfig& getConfig() const { return *config_; }
    const std::string& getName() const { return config_->name; }
    size_t getSourceIndex() const { return source_index_; }
    
    // Status and diagnostics
    void printStatus() const;
    bool isInitialized() const { return chain_ != nullptr; }

    // Helper to calculate time offset for current event
    void calculateTimeOffsetForEvent(const std::string& mcparticle_collection,
                                     float time_slice_duration,
                                     float bunch_crossing_period,
                                     std::mt19937& rng);

private:
    // Configuration
    const SourceConfig* config_;
    size_t source_index_;
    
    // Relationship mapper (not owned by this class)
    const BranchRelationshipMapper* relationship_mapper_;
    
    // ROOT chain and state
    std::unique_ptr<TChain> chain_;
    size_t total_entries_;
    size_t current_entry_index_;
    size_t entries_needed_;
    
    // Collection names (references to shared data)
    const std::vector<std::string>* gp_collection_names_;
    
    // Generic branch storage - stores void* to branch data, keyed by branch name
    // The actual type is determined by the BranchRelationshipMapper metadata
    std::unordered_map<std::string, void*> generic_branches_;
    
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
    
    
    // Private helper methods
    void setupBranches();
    void setupGenericBranches();
    void setupRelationshipBranches();
    void setupGPBranches();
    void cleanup();
    
    // Helper methods for physics calculations
    float calculateBeamDistance(const std::vector<edm4hep::MCParticleData>& particles) const;
};