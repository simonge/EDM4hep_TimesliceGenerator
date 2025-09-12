#pragma once

#include "StandaloneMergerConfig.h"

// ROOT DataFrames support when available
#ifdef USE_ROOT
#include <ROOT/RDataFrame.hxx>
#include <TFile.h>
#include <TTree.h>
#include <TChain.h>
#endif

#include <fstream>
#include <sstream>
#include <random>
#include <unordered_map>
#include <vector>
#include <string>
#include <memory>

// Simplified EDM4HEP-like data structures for ROOT-dataframe approach
// These represent the data that would be in ROOT dataframes

struct MCParticleData {
    int32_t PDG;
    int32_t generatorStatus;
    int32_t simulatorStatus;
    float charge;
    float time;
    float mass;
    // Position and momentum
    float vertex[3];  // x, y, z
    float momentum[3]; // px, py, pz
    // References (indices to other particles)
    std::vector<int32_t> parents;
    std::vector<int32_t> daughters;
};

struct EventHeaderData {
    int32_t eventNumber;
    int32_t runNumber;
    uint64_t timeStamp;
    float weight;
};

struct SimTrackerHitData {
    uint64_t cellID;
    float EDep;
    float time;
    float pathLength;
    int32_t quality;
    float position[3]; // x, y, z
    int32_t mcParticle_idx; // Reference to MCParticle
};

struct SimCalorimeterHitData {
    uint64_t cellID;
    float energy;
    float time;
    float position[3]; // x, y, z
    // Contributions handled separately
};

struct CaloHitContributionData {
    int32_t PDG;
    float energy;
    float time;
    float stepPosition[3]; // x, y, z
    int32_t particle_idx; // Reference to MCParticle
};

// Collection wrapper that acts like a dataframe
template<typename T>
class DataFrameCollection {
public:
    std::vector<T> data;
    std::string name;
    std::string type;
    
    void clear() { data.clear(); }
    size_t size() const { return data.size(); }
    void push_back(const T& item) { data.push_back(item); }
    T& operator[](size_t i) { return data[i]; }
    const T& operator[](size_t i) const { return data[i]; }
    
    // Iterator support for range-based loops
    typename std::vector<T>::iterator begin() { return data.begin(); }
    typename std::vector<T>::iterator end() { return data.end(); }
    typename std::vector<T>::const_iterator begin() const { return data.begin(); }
    typename std::vector<T>::const_iterator end() const { return data.end(); }
    
    // Dataframe-like operations
    void updateTiming(float offset);
    void updateReferences(size_t index_offset);
};

// Timing and reference update info for merging
struct ProcessingInfo {
    float time_offset{0.0f};
    size_t particle_index_offset{0};
    size_t collection_base_index{0};
};

struct SourceReader {
    std::string current_file;
    std::vector<std::string> input_files;
    size_t current_file_index{0};
    size_t total_entries{0};
    size_t current_entry_index{0};
    size_t entries_needed{1};
    const SourceConfig* config;
    
    // Collections as dataframes
    DataFrameCollection<MCParticleData> mcparticles;
    DataFrameCollection<EventHeaderData> eventheaders;
    DataFrameCollection<SimTrackerHitData> trackerhits;
    DataFrameCollection<SimCalorimeterHitData> calohits;
    DataFrameCollection<CaloHitContributionData> contributions;
};

class StandaloneTimesliceMerger {
public:
    StandaloneTimesliceMerger(const MergerConfig& config);
    
    void run();

private:
    MergerConfig m_config;
    
    // Random number generator members
    std::random_device rd;
    std::mt19937 gen;

    // Collections to merge
    std::vector<std::string> collections_to_merge = {"MCParticles", "EventHeader"};
    std::vector<std::string> hit_collection_types = {"SimTrackerHit", "SimCalorimeterHit", "CaloHitContribution"};

    // State variables
    size_t events_generated;

    std::vector<SourceReader> initializeInputFiles();
    bool updateInputNEvents(std::vector<SourceReader>& inputs);
    void createMergedTimeslice(std::vector<SourceReader>& inputs, std::ofstream& output_file);

    // Helper methods for dataframe-based merging
    void mergeCollectionsFromDataFrames(SourceReader& source, 
                                       ProcessingInfo& proc_info,
                                       DataFrameCollection<MCParticleData>& merged_particles,
                                       DataFrameCollection<EventHeaderData>& merged_headers,
                                       DataFrameCollection<SimTrackerHitData>& merged_tracker_hits,
                                       DataFrameCollection<SimCalorimeterHitData>& merged_calo_hits,
                                       DataFrameCollection<CaloHitContributionData>& merged_contributions);
    
    // Helper functions for time updates and reference handling  
    void updateParticleDataTiming(DataFrameCollection<MCParticleData>& particles, 
                                 float time_offset, int32_t status_offset);
    void updateTrackerHitDataTiming(DataFrameCollection<SimTrackerHitData>& hits, 
                                   float time_offset);
    void updateTrackerHitReferences(DataFrameCollection<SimTrackerHitData>& hits, 
                                   size_t particle_offset);
    void updateCaloHitDataTiming(DataFrameCollection<SimCalorimeterHitData>& hits, 
                                float time_offset);
    void updateContributionReferences(DataFrameCollection<CaloHitContributionData>& contribs, 
                                     size_t particle_offset);
                         
    float generateTimeOffset(SourceConfig sourceConfig, float distance);
    
    // File I/O for demonstration (would be replaced with ROOT dataframe I/O)
    bool readDataFrameFromFile(SourceReader& reader, size_t entry_index);
    void writeDataFramesToFile(std::ofstream& file,
                              const DataFrameCollection<MCParticleData>& particles,
                              const DataFrameCollection<EventHeaderData>& headers,
                              const DataFrameCollection<SimTrackerHitData>& tracker_hits,
                              const DataFrameCollection<SimCalorimeterHitData>& calo_hits,
                              const DataFrameCollection<CaloHitContributionData>& contributions);
};

// Template specializations for dataframe operations
template<>
void DataFrameCollection<MCParticleData>::updateTiming(float offset);

template<>
void DataFrameCollection<SimTrackerHitData>::updateTiming(float offset);

template<>
void DataFrameCollection<SimCalorimeterHitData>::updateTiming(float offset);

template<>
void DataFrameCollection<SimTrackerHitData>::updateReferences(size_t index_offset);

template<>
void DataFrameCollection<CaloHitContributionData>::updateReferences(size_t index_offset);