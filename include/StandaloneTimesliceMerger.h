    #pragma once

#include "StandaloneMergerConfig.h"

// ROOT DataFrames support - always available
#include <ROOT/RDataFrame.hxx>
#include <TFile.h>
#include <TTree.h>
#include <TChain.h>

#include <fstream>
#include <sstream>
#include <random>
#include <unordered_map>
#include <map>
#include <vector>
#include <string>
#include <memory>
#include <podio/ObjectID.h>

// EDM4HEP includes
#include "edm4hep/MCParticle.h"
#include "edm4hep/EventHeaderCollection.h"
#include "edm4hep/MCParticleCollection.h"
#include "edm4hep/SimTrackerHitCollection.h"
#include "edm4hep/SimCalorimeterHitCollection.h"
#include "edm4hep/CaloHitContributionCollection.h"

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
    
    // Collections as dataframes - now supports multiple collections of same type
    DataFrameCollection<edm4hep::MCParticleData> mcparticles;
    DataFrameCollection<edm4hep::EventHeaderData> eventheaders;
    
    // Multiple collections of each hit type, discovered from input files
    std::map<std::string, DataFrameCollection<edm4hep::SimTrackerHitData>> trackerhits_collections;
    std::map<std::string, DataFrameCollection<edm4hep::SimCalorimeterHitData>> calohits_collections;
    std::map<std::string, DataFrameCollection<edm4hep::CaloHitContributionData>> contributions_collections;
    
    // Discovered collection names and types
    std::vector<std::string> discovered_trackerhit_names;
    std::vector<std::string> discovered_calohit_names;
    std::vector<std::string> discovered_contribution_names;
    std::vector<std::string> discovered_reference_columns;
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

    // ROOT-specific methods for proper file I/O with multiple collections
    void createMergedTimesliceROOT(std::vector<SourceReader>& inputs, TFile* output_file);
    void createMergedDataFrameROOT(std::vector<SourceReader>& inputs, TFile* output_file);
    void writeDataFramesToROOTFile(TFile* file,
                                  const DataFrameCollection<edm4hep::MCParticleData>& particles,
                                  const DataFrameCollection<edm4hep::EventHeaderData>& headers,
                                  const std::map<std::string, DataFrameCollection<edm4hep::SimTrackerHitData>>& tracker_collections,
                                  const std::map<std::string, DataFrameCollection<edm4hep::SimCalorimeterHitData>>& calo_collections,
                                  const std::map<std::string, DataFrameCollection<edm4hep::CaloHitContributionData>>& contribution_collections);
    
    // Copy podio metadata from input files
    void copyPodioMetadata(TFile* output_file, const std::vector<SourceReader>& inputs);
    
    // Discover collections from input dataframes
    void discoverCollectionsFromDataFrame(const std::string& input_filename, SourceReader& reader);

    // Helper methods for generating merged collections using RVec
    ROOT::VecOps::RVec<edm4hep::MCParticleData> generateMergedMCParticles(std::vector<SourceReader>& inputs, int event_id);
    ROOT::VecOps::RVec<edm4hep::EventHeaderData> generateMergedEventHeaders(std::vector<SourceReader>& inputs, int event_id);
    ROOT::VecOps::RVec<edm4hep::SimTrackerHitData> generateMergedTrackerHits(std::vector<SourceReader>& inputs, int event_id, const std::string& collection_name);
    ROOT::VecOps::RVec<edm4hep::SimCalorimeterHitData> generateMergedCaloHits(std::vector<SourceReader>& inputs, int event_id, const std::string& collection_name);
    ROOT::VecOps::RVec<edm4hep::CaloHitContributionData> generateMergedContributions(std::vector<SourceReader>& inputs, int event_id, const std::string& collection_name);
    ROOT::VecOps::RVec<podio::ObjectID> generateMergedReferenceColumn(std::vector<SourceReader>& inputs, int event_id, const std::string& column_name);

    // Helper methods for dataframe-based merging with multiple collections
    void mergeCollectionsFromDataFrames(SourceReader& source, 
                                       ProcessingInfo& proc_info,
                                       DataFrameCollection<edm4hep::MCParticleData>& merged_particles,
                                       DataFrameCollection<edm4hep::EventHeaderData>& merged_headers,
                                       std::map<std::string, DataFrameCollection<edm4hep::SimTrackerHitData>>& merged_tracker_collections,
                                       std::map<std::string, DataFrameCollection<edm4hep::SimCalorimeterHitData>>& merged_calo_collections,
                                       std::map<std::string, DataFrameCollection<edm4hep::CaloHitContributionData>>& merged_contribution_collections);
    
    // Helper functions for time updates and reference handling  
    void updateParticleDataTiming(DataFrameCollection<edm4hep::MCParticleData>& particles, 
                                 float time_offset, int32_t status_offset);
    void updateTrackerHitDataTiming(DataFrameCollection<edm4hep::SimTrackerHitData>& hits, 
                                   float time_offset);
    void updateTrackerHitReferences(DataFrameCollection<edm4hep::SimTrackerHitData>& hits, 
                                   size_t particle_offset);
    void updateCaloHitDataTiming(DataFrameCollection<edm4hep::SimCalorimeterHitData>& hits, 
                                float time_offset);
    void updateContributionReferences(DataFrameCollection<edm4hep::CaloHitContributionData>& contribs, 
                                     size_t particle_offset);
                         
    float generateTimeOffset(SourceConfig sourceConfig, float distance);
    
    // File I/O for demonstration (would be replaced with ROOT dataframe I/O)
    bool readDataFrameFromFile(SourceReader& reader, size_t entry_index);
};

// Template specializations for dataframe operations
template<>
void DataFrameCollection<edm4hep::MCParticle>::updateTiming(float offset);

template<>
void DataFrameCollection<edm4hep::SimTrackerHit>::updateTiming(float offset);

template<>
void DataFrameCollection<edm4hep::SimCalorimeterHit>::updateTiming(float offset);

template<>
void DataFrameCollection<edm4hep::SimTrackerHit>::updateReferences(size_t index_offset);

template<>
void DataFrameCollection<edm4hep::CaloHitContribution>::updateReferences(size_t index_offset);