#pragma once

#include "OutputHandler.h"
#include "HepMC3DataSource.h"
#include <HepMC3/WriterRootTree.h>
#include <HepMC3/GenEvent.h>
#include <memory>
#include <vector>
#include <string>

/**
 * @class HepMC3OutputHandler
 * @brief Concrete implementation of OutputHandler for HepMC3 format
 * 
 * Handles writing merged timeslice data in HepMC3 format using ROOT tree format.
 * Merges multiple HepMC3 events into single timeslice events with time offsets applied.
 */
class HepMC3OutputHandler : public OutputHandler {
public:
    HepMC3OutputHandler() = default;
    ~HepMC3OutputHandler() override = default;

    void initialize(const std::string& filename, 
                   const std::vector<std::unique_ptr<DataSource>>& sources) override;
    
    void prepareTimeslice() override;
    
    void mergeEvents(std::vector<std::unique_ptr<DataSource>>& sources,
                    size_t timeslice_number,
                    float time_slice_duration,
                    float bunch_crossing_period,
                    std::mt19937& gen) override;
    
    void writeTimeslice() override;
    
    void finalize() override;
    
    std::string getFormatName() const override { return "HepMC3"; }

private:
    std::shared_ptr<HepMC3::WriterRootTree> writer_;
    std::unique_ptr<HepMC3::GenEvent> current_timeslice_;
    
    // Store validated HepMC3 data sources (non-owning pointers)
    std::vector<HepMC3DataSource*> hepmc3_sources_;
    
    size_t current_timeslice_number_ = 0;
    
    // Speed of light constant: c = 299.792458 mm/ns
    // Used for converting time offsets (ns) to position offsets (mm) in HepMC3
    static constexpr double c_light = 299.792458;

    // Helper methods
    long insertHepMC3Event(const HepMC3::GenEvent& inevt,
                          std::unique_ptr<HepMC3::GenEvent>& hepSlice,
                          double time,
                          int baseStatus);
};
