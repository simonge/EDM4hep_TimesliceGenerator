#pragma once

#include "MergerConfig.h"
#include "DataSource.h"
#include "DataHandler.h"
#include <random>
#include <vector>
#include <string>
#include <memory>

/**
 * @class TimesliceMerger
 * @brief Core timeslice merging engine independent of data format
 * 
 * This class orchestrates the merging of events from multiple sources into timeslices.
 * It handles frequency sampling, timing relationships, and event selection, but delegates
 * the actual data I/O to a DataHandler implementation. This allows the same merging logic
 * to be used with different formats (EDM4hep, HepMC3, etc.).
 */
class TimesliceMerger {
public:
    TimesliceMerger(const MergerConfig& config);
    
    /**
     * Set the data handler for managing input and output
     * @param handler Unique pointer to a DataHandler implementation
     */
    void setDataHandler(std::unique_ptr<DataHandler> handler);
    
    /**
     * Run the merging process
     */
    void run();

private:
    MergerConfig m_config;
    
    // Random number generator members
    std::random_device rd;
    std::mt19937 gen;

    // Data sources (managed by data handler)
    std::vector<std::unique_ptr<DataSource>> data_sources_;
    
    // Data handler (format-specific)
    std::unique_ptr<DataHandler> data_handler_;

    // Core functionality methods
    bool updateInputNEvents(std::vector<std::unique_ptr<DataSource>>& sources);
};