#pragma once

#include "DataSource.h"
#include "MergerConfig.h"
#include <HepMC3/ReaderFactory.h>
#include <HepMC3/GenEvent.h>
#include <memory>
#include <vector>
#include <string>
#include <random>

/**
 * @class HepMC3DataSource
 * @brief Concrete implementation for reading HepMC3 format event data
 * 
 * Handles all HepMC3-specific logic for reading events from ROOT tree files
 * (.hepmc3.tree.root), applying time offsets, and providing data for merging.
 */
class HepMC3DataSource : public DataSource {
public:
    HepMC3DataSource(const SourceConfig& config, size_t source_index);
    ~HepMC3DataSource();
    
    // Initialization
    void initialize(const std::vector<std::string>& tracker_collections,
                   const std::vector<std::string>& calo_collections,
                   const std::vector<std::string>& gp_collections) override;
    
    // Data access
    bool hasMoreEntries() const override;
    size_t getTotalEntries() const override { return total_entries_; }
    size_t getCurrentEntryIndex() const override { return current_entry_index_; }
    void setCurrentEntryIndex(size_t index) override { current_entry_index_ = index; }
    float getCurrentTimeOffset() const override { return current_time_offset_; }

    // Event management
    void setEntriesNeeded(size_t entries) override { entries_needed_ = entries; }
    size_t getEntriesNeeded() const override { return entries_needed_; }
    bool loadNextEvent() override;
    
    // Event loading
    void loadEvent(size_t event_index) override;

    // HepMC3-specific data access methods
    const HepMC3::GenEvent& getCurrentEvent() const { return current_event_; }
    
    // Configuration access
    const SourceConfig& getConfig() const override { return *config_; }
    const std::string& getName() const override { return config_->name; }
    size_t getSourceIndex() const override { return source_index_; }
    
    // Status and diagnostics
    void printStatus() const override;
    bool isInitialized() const override { return reader_ != nullptr; }
    std::string getFormatName() const override { return "HepMC3"; }

private:
    // Configuration
    const SourceConfig* config_;
    size_t source_index_;
    
    // HepMC3 reader and state
    std::shared_ptr<HepMC3::Reader> reader_;
    size_t total_entries_;
    size_t current_entry_index_;
    size_t entries_needed_;
    
    // Current event
    HepMC3::GenEvent current_event_;
    
    // Private helper methods
    void openInputFiles();
    void cleanup();
    
    // Format-specific vertex extraction from HepMC3 events (overrides base class)
    VertexPosition getBeamVertexPosition() const override;
};
