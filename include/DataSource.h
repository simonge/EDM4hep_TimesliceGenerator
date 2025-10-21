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
#include <any>

/**
 * EventData represents a single loaded event as a map of collection names to data.
 * This allows generic iteration over all collections without type-specific accessors.
 */
struct EventData {
    // Map of collection name to collection data (type-erased as std::any)
    std::unordered_map<std::string, std::any> collections;
    
    // Map tracking the size of each collection before merging
    std::unordered_map<std::string, size_t> collection_sizes;
    
    // Time offset calculated for this event
    float time_offset = 0.0f;
    
    // Helper to get typed collection data
    template<typename T>
    T* getCollection(const std::string& name) {
        auto it = collections.find(name);
        if (it != collections.end()) {
            return std::any_cast<T>(&it->second);
        }
        return nullptr;
    }
    
    // Helper to check if collection exists
    bool hasCollection(const std::string& name) const {
        return collections.find(name) != collections.end();
    }
    
    // Helper to determine collection type based on name pattern
    std::string getCollectionType(const std::string& name) const {
        if (name == "MCParticles") return "MCParticles";
        if (name.find("_") == 0) return "ObjectID";  // Reference collections start with _
        if (name.find("Contributions") != std::string::npos && name.find("_") == std::string::npos) return "CaloHitContribution";
        if (name.find("EventHeader") != std::string::npos) return "EventHeader";
        if (name.find("GPIntValues") != std::string::npos || 
            name.find("GPFloatValues") != std::string::npos ||
            name.find("GPDoubleValues") != std::string::npos ||
            name.find("GPStringValues") != std::string::npos) return "GPValues";
        if (name.find("GPIntKeys") == 0 || 
            name.find("GPFloatKeys") == 0 || 
            name.find("GPDoubleKeys") == 0 || 
            name.find("GPStringKeys") == 0) return "GPKeys";
        
        // Try to determine based on collection content type
        // SimTrackerHit collections don't have standard suffixes
        // SimCalorimeterHit collections don't have standard suffixes
        // These will be discovered dynamically based on the actual data type
        return "Unknown";
    }
};

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
    
    // Event loading - returns EventData map with all collections
    EventData* loadEvent(size_t event_index, float time_slice_duration, 
                         float bunch_crossing_period, std::mt19937& rng);
    
    // Get collection type name for a given collection (for generic processing)
    std::string getCollectionTypeName(const std::string& collection_name) const {
        if (collection_name == "MCParticles") return "MCParticles";
        if (collection_name.find("Contributions") != std::string::npos && collection_name.find("_") == std::string::npos) {
            return "CaloHitContribution";
        }
        // Check if it's a tracker collection
        if (tracker_collection_names_) {
            for (const auto& name : *tracker_collection_names_) {
                if (name == collection_name) return "SimTrackerHit";
            }
        }
        // Check if it's a calo collection
        if (calo_collection_names_) {
            for (const auto& name : *calo_collection_names_) {
                if (name == collection_name) return "SimCalorimeterHit";
            }
        }
        return "Unknown";
    }

    
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

    // Current loaded event data
    std::unique_ptr<EventData> current_event_data_;

    // Private helper methods
    void setupBranches();
    
    // Generic function to setup an object branch and its associated relation branches
    template<typename T>
    void setupObjectBranch(const std::string& collection_name, const std::string& type_name, T*& branch_ptr);
    
    void setupMCParticleBranches();
    void setupTrackerBranches();
    void setupCalorimeterBranches();
    void setupEventHeaderBranches();
    void setupGPBranches();
    void cleanup();
    
    // Helper methods for time offset calculations
    float generateTimeOffset(float distance, float time_slice_duration, float bunch_crossing_period, std::mt19937& rng) const;
    float calculateBeamDistance(const std::vector<edm4hep::MCParticleData>& particles) const;
    
    // Helper methods for collection name mapping
    std::string getCorrespondingContributionCollection(const std::string& calo_collection_name) const;
    std::string getCorrespondingCaloCollection(const std::string& contrib_collection_name) const;
};