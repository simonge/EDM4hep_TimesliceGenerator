#pragma once

#include "StandaloneMergerConfig.h"
#include <HepMC3/ReaderFactory.h>
#include <HepMC3/WriterAscii.h>
#include <HepMC3/WriterRootTree.h>
#include <HepMC3/GenEvent.h>
#include <HepMC3/GenParticle.h>
#include <HepMC3/GenVertex.h>
#include <random>
#include <memory>
#include <map>
#include <vector>
#include <tuple>

/**
 * @brief HepMC3-based timeslice merger
 * 
 * This class implements timeslice merging for HepMC3 format files,
 * using the same configuration structure as the EDM4hep merger.
 * Based on the EPIC HEPMC_Merger implementation.
 */
class HepMC3TimesliceMerger {
public:
    /**
     * @brief Constructor
     * @param config Merger configuration (same as EDM4hep merger)
     */
    explicit HepMC3TimesliceMerger(const MergerConfig& config);
    
    /**
     * @brief Run the merging process
     */
    void run();

private:
    // Configuration
    MergerConfig m_config;
    
    // Random number generation
    std::mt19937 m_rng;
    
    // Speed of light constant (mm/ns)
    const double c_light = 299.792458;
    
    // Source readers and configurations
    struct SourceData {
        std::shared_ptr<HepMC3::Reader> reader;
        SourceConfig config;
        long event_count{0};
        long particle_count{0};
        
        // For weighted sources
        std::vector<HepMC3::GenEvent> events;
        std::vector<double> weights;
        std::unique_ptr<std::piecewise_constant_distribution<>> weighted_dist;
        double avg_rate{0.0};
    };
    
    std::vector<SourceData> m_sources;
    
    // Methods
    void prepareSource(size_t source_idx);
    void prepareFrequencySource(SourceData& source);
    void prepareWeightedSource(SourceData& source);
    
    std::unique_ptr<HepMC3::GenEvent> mergeSlice(int slice_idx);
    
    void addFreqEvents(SourceData& source, std::unique_ptr<HepMC3::GenEvent>& hepSlice);
    void addWeightedEvents(SourceData& source, std::unique_ptr<HepMC3::GenEvent>& hepSlice);
    
    long insertHepmcEvent(const HepMC3::GenEvent& inevt, 
                          std::unique_ptr<HepMC3::GenEvent>& hepSlice, 
                          double time, 
                          int baseStatus);
    
    std::vector<double> poissonTimes(double mu, double endTime);
    double generateTimeOffset();
    
    void printBanner();
    void printStatistics(int slicesDone);
};
