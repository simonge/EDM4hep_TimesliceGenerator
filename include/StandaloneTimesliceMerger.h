#pragma once

#include "StandaloneMergerConfig.h"
#include <edm4hep/EventHeaderCollection.h>
#include <edm4hep/MCParticleCollection.h>
#include <edm4hep/SimTrackerHitCollection.h>
#include <edm4hep/SimCalorimeterHitCollection.h>
#include <edm4hep/CaloHitContributionCollection.h>

// Try to include edm4eic headers if available
#ifdef HAVE_EDM4EIC
#include <edm4eic/MCParticleCollection.h>
#include <edm4eic/VertexCollection.h>
#endif

#include <podio/ROOTReader.h>
#include <podio/ROOTWriter.h>
#include <podio/Frame.h>

// Check for newer podio readEntry API
#include <podio/version.h>
#if PODIO_VERSION_MAJOR > 0 || (PODIO_VERSION_MAJOR == 0 && PODIO_VERSION_MINOR >= 17)
#define HAVE_PODIO_COLLECTION_LIST
#endif

#include <random>
#include <unordered_map>
#include <vector>
#include <string>

class StandaloneTimesliceMerger {
public:
    StandaloneTimesliceMerger(const MergerConfig& config);
    
    void run();

private:
    MergerConfig m_config;
    
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
    int inputEventsConsumed;
    std::vector<int> eventsConsumedPerSource;
    
    // Collection lists for optimized reading (when HAVE_PODIO_COLLECTION_LIST is available)
    std::vector<std::vector<std::string>> collections_per_source;

    void setupRandomGenerators();
    void processInputFiles();
    std::unique_ptr<podio::Frame> createMergedTimeslice();
    void writeOutput(std::unique_ptr<podio::ROOTWriter>& writer, std::unique_ptr<podio::Frame> frame);
    
    // Helper methods for timeslice merging logic
    void mergeCollections(const std::vector<std::unique_ptr<podio::Frame>>& frames, 
                         edm4hep::MCParticleCollection& out_particles,
                         std::unordered_map<std::string, edm4hep::SimTrackerHitCollection>& out_tracker_hits,
                         std::unordered_map<std::string, edm4hep::SimCalorimeterHitCollection>& out_calo_hits,
                         std::unordered_map<std::string, edm4hep::CaloHitContributionCollection>& out_calo_contributions);

#ifdef HAVE_EDM4EIC
    void mergeEdm4eicCollections(const std::vector<std::unique_ptr<podio::Frame>>& frames,
                                edm4eic::MCParticleCollection& out_particles,
                                std::unordered_map<std::string, edm4eic::VertexCollection>& out_vertices);
#endif
                         
    float generateTimeOffset();
    std::vector<std::string> getCollectionNames(const podio::Frame& frame, const std::string& type);
    void buildCollectionLists();
};