#pragma once

#include "MergerConfig.h"
#include "DataSource.h"
#include "OutputHandler.h"
#include <random>
#include <vector>
#include <string>
#include <memory>

/**
 * @class TimesliceMerger
 * @brief Core timeslice merging engine independent of output format
 * 
 * This class orchestrates the merging of events from multiple sources into timeslices.
 * It handles frequency sampling, timing relationships, and event selection, but delegates
 * the actual output writing to an OutputHandler implementation. This allows the same
 * merging logic to be used with different output formats (EDM4hep, HepMC3, etc.).
 */
class TimesliceMerger {
public:
    TimesliceMerger(const MergerConfig& config);
    
    /**
     * Set the output handler for writing merged data
     * @param handler Unique pointer to an OutputHandler implementation
     */
    void setOutputHandler(std::unique_ptr<OutputHandler> handler);
    
    /**
     * Run the merging process
     */
    void run();

private:
    MergerConfig m_config;
    
    // Random number generator members
    std::random_device rd;
    std::mt19937 gen;

    // State variables
    size_t events_generated;

    // Data sources
    std::vector<std::unique_ptr<DataSource>> data_sources_;
    
    // Output handler (format-specific)
    std::unique_ptr<OutputHandler> output_handler_;

    // Core functionality methods
    std::vector<std::unique_ptr<DataSource>> initializeDataSources();
    bool updateInputNEvents(std::vector<std::unique_ptr<DataSource>>& sources);
};