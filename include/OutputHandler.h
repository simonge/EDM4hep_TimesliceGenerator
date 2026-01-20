#pragma once

#include "DataSource.h"
#include <vector>
#include <string>
#include <memory>

/**
 * @class OutputHandler
 * @brief Abstract base class for handling output file writing in different formats
 * 
 * This class provides the interface for writing merged timeslice data to various
 * output formats (EDM4hep, HepMC3, etc.). Concrete implementations handle the
 * format-specific details of file creation, data writing, and metadata management.
 */
class OutputHandler {
public:
    virtual ~OutputHandler() = default;

    /**
     * Initialize the output file and any necessary structures
     * @param filename Output file path
     * @param sources Data sources (for metadata extraction)
     */
    virtual void initialize(const std::string& filename, 
                          const std::vector<std::unique_ptr<DataSource>>& sources) = 0;

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
                           std::mt19937& gen) = 0;

    /**
     * Write the completed timeslice to output
     */
    virtual void writeTimeslice() = 0;

    /**
     * Finalize and close the output file
     */
    virtual void finalize() = 0;

    /**
     * Get the output format name
     */
    virtual std::string getFormatName() const = 0;

    /**
     * Factory method to create appropriate output handler based on filename
     * @param filename Output file path
     * @return Unique pointer to appropriate OutputHandler implementation
     * @throws std::runtime_error if format is not supported
     */
    static std::unique_ptr<OutputHandler> create(const std::string& filename);
};
