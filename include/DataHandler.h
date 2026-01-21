#pragma once

#include "DataSource.h"
#include "MergerConfig.h"
#include <vector>
#include <string>
#include <memory>

/**
 * @class DataHandler
 * @brief Abstract base class for handling both input and output in different formats
 * 
 * This class provides the interface for handling data I/O in various formats (EDM4hep, HepMC3, etc.).
 * Each concrete implementation handles the format-specific details of:
 * - Creating appropriate DataSource instances for input
 * - Reading and merging events
 * - Writing merged timeslice data to output files
 */
class DataHandler {
public:
    virtual ~DataHandler() = default;

    /**
     * Initialize data sources and output file
     * @param filename Output file path
     * @param source_configs Vector of source configurations
     * @return Vector of initialized data sources
     */
    virtual std::vector<std::unique_ptr<DataSource>> initializeDataSources(
        const std::string& filename,
        const std::vector<SourceConfig>& source_configs) = 0;

    /**
     * Prepare for a new timeslice (clear buffers, etc.)
     */
    virtual void prepareTimeslice() = 0;

    /**
     * Process and merge events from all sources into the current timeslice
     * @param sources Vector of data sources
     * @param timeslice_number Current timeslice number
     * @param time_slice_duration Duration of the timeslice in ns
     * @param bunch_crossing_period Bunch crossing period in ns
     * @param gen Random number generator
     */
    virtual void mergeEvents(std::vector<std::unique_ptr<DataSource>>& sources,
                            size_t timeslice_number,
                            float time_slice_duration,
                            float bunch_crossing_period,
                            std::mt19937& gen) final;

    /**
     * Write the completed timeslice to output
     */
    virtual void writeTimeslice() = 0;

    /**
     * Finalize and close the output file
     */
    virtual void finalize() = 0;

    /**
     * Get the format name
     */
    virtual std::string getFormatName() const = 0;

protected:
    /**
     * Process a single loaded event during merging
     * Implemented by concrete handlers for format-specific logic
     * Called after loadEvent and UpdateTimeOffset have been applied
     */
    virtual void processEvent(DataSource& source) = 0;
    
    size_t current_timeslice_number_ = 0;

public:
    /**
     * Factory method to create appropriate data handler based on filename
     * @param filename Output file path
     * @return Unique pointer to appropriate DataHandler implementation
     * @throws std::runtime_error if format is not supported
     */
    static std::unique_ptr<DataHandler> create(const std::string& filename);
};
