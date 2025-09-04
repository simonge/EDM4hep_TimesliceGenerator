#pragma once

#include "StandaloneMergerConfig.h"
#include <edm4hep/EventHeaderCollection.h>
#include <edm4hep/MCParticleCollection.h>
#include <edm4hep/SimTrackerHitCollection.h>
#include <edm4hep/SimCalorimeterHitCollection.h>
#include <edm4hep/CaloHitContributionCollection.h>
#include <podio/ROOTReader.h>
#include <podio/ROOTWriter.h>
#include <podio/Frame.h>
#include <random>
#include <unordered_map>
#include <vector>
#include <string>

class StandaloneTimesliceMerger {
public:
    StandaloneTimesliceMerger(const StandaloneMergerConfig& config);
    
    void run();

private:
    StandaloneMergerConfig m_config;
    
    // Random number generator members
    std::random_device rd;
    std::mt19937 gen;
    std::uniform_real_distribution<float> uniform;
    std::poisson_distribution<> poisson;
    std::normal_distribution<> gaussian;
    
    // State variables
    std::vector<std::unique_ptr<podio::Frame>> accumulated_frames;
    int events_needed;
    size_t events_generated;
    
    void setupRandomGenerators();
    void processInputFiles();
    std::unique_ptr<podio::Frame> createMergedTimeslice();
    void writeOutput(std::unique_ptr<podio::ROOTWriter>& writer, std::unique_ptr<podio::Frame> frame);
    
    // Helper methods to replicate JANA merging logic
    void mergeCollections(const std::vector<std::unique_ptr<podio::Frame>>& frames, 
                         edm4hep::MCParticleCollection& out_particles,
                         std::unordered_map<std::string, edm4hep::SimTrackerHitCollection>& out_tracker_hits,
                         std::unordered_map<std::string, edm4hep::SimCalorimeterHitCollection>& out_calo_hits,
                         std::unordered_map<std::string, edm4hep::CaloHitContributionCollection>& out_calo_contributions);
                         
    float generateTimeOffset();
    std::vector<std::string> getCollectionNames(const podio::Frame& frame, const std::string& type);
};