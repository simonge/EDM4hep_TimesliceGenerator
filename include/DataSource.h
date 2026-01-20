#pragma once

#include "MergerConfig.h"
#include <memory>
#include <vector>
#include <string>
#include <random>

/**
 * @class DataSource
 * @brief Abstract base class for input data sources with pluggable format support
 * 
 * This abstract class defines the interface for reading event data from various
 * input formats (EDM4hep, HepMC3, etc.). Concrete implementations handle
 * format-specific logic while the merger remains format-agnostic.
 */
class DataSource {
public:
    virtual ~DataSource() = default;
    
    // Initialization
    virtual void initialize(const std::vector<std::string>& tracker_collections,
                           const std::vector<std::string>& calo_collections,
                           const std::vector<std::string>& gp_collections) = 0;
    
    // Data access
    virtual bool hasMoreEntries() const = 0;
    virtual size_t getTotalEntries() const = 0;
    virtual size_t getCurrentEntryIndex() const = 0;
    virtual void setCurrentEntryIndex(size_t index) = 0;
    virtual float getCurrentTimeOffset() const = 0;

    // Event management
    virtual void setEntriesNeeded(size_t entries) = 0;
    virtual size_t getEntriesNeeded() const = 0;
    virtual bool loadNextEvent() = 0;
    
    // Time offset generation
    virtual float generateTimeOffset(float distance, float time_slice_duration, 
                                    float bunch_crossing_period, std::mt19937& rng) const = 0;
    
    // Event loading and time offset update
    virtual void loadEvent(size_t event_index) = 0;
    virtual void UpdateTimeOffset(float time_slice_duration, float bunch_crossing_period, 
                                 std::mt19937& rng) = 0;
    
    // Configuration access
    virtual const SourceConfig& getConfig() const = 0;
    virtual const std::string& getName() const = 0;
    virtual size_t getSourceIndex() const = 0;
    
    // Status and diagnostics
    virtual void printStatus() const = 0;
    virtual bool isInitialized() const = 0;
    
    // Format-specific getters (to be implemented by concrete classes)
    virtual std::string getFormatName() const = 0;
};