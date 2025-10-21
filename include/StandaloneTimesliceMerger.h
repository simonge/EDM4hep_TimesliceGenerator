#pragma once

#include "StandaloneMergerConfig.h"
#include "DataSource.h"
#include "BranchRelationshipMapper.h"
#include <edm4hep/MCParticleData.h>
#include <edm4hep/SimTrackerHitData.h>
#include <edm4hep/SimCalorimeterHitData.h>
#include <edm4hep/CaloHitContributionData.h>
#include <edm4hep/EventHeaderData.h>
#include <podio/ObjectID.h>
#include <ROOT/RVec.hxx>
#include <TFile.h>
#include <TTree.h>
#include <TChain.h>
#include <random>
#include <unordered_map>
#include <vector>
#include <string>
#include <memory>

// Struct to organize all merged collections in one place
struct MergedCollections {
    // Event and particle data
    std::vector<edm4hep::MCParticleData> mcparticles;
    std::vector<edm4hep::EventHeaderData> event_headers;
    std::vector<double> event_header_weights; 
    std::vector<edm4hep::EventHeaderData> sub_event_headers;
    std::vector<double> sub_event_header_weights;
    
    // Hit data collections
    std::unordered_map<std::string, std::vector<edm4hep::SimTrackerHitData>> tracker_hits;
    std::unordered_map<std::string, std::vector<edm4hep::SimCalorimeterHitData>> calo_hits;
    std::unordered_map<std::string, std::vector<edm4hep::CaloHitContributionData>> calo_contributions;
    
    // Reference collections
    std::vector<podio::ObjectID> mcparticle_parents_refs;
    std::vector<podio::ObjectID> mcparticle_children_refs;
    std::unordered_map<std::string, std::vector<podio::ObjectID>> tracker_hit_particle_refs;
    std::unordered_map<std::string, std::vector<podio::ObjectID>> calo_contrib_particle_refs;
    std::unordered_map<std::string, std::vector<podio::ObjectID>> calo_hit_contributions_refs;
    
    // GP (Global Parameter) branches - separate containers for different types
    std::unordered_map<std::string, std::vector<std::string>> gp_key_branches;
    std::vector<std::vector<int>> gp_int_values;
    std::vector<std::vector<float>> gp_float_values;
    std::vector<std::vector<double>> gp_double_values;
    std::vector<std::vector<std::string>> gp_string_values;
    
    // Utility method to clear all collections
    void clear();
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

    // State variables
    size_t events_generated;

    // Merged collections organized in a struct
    MergedCollections merged_collections_;

    // Collection names discovered from first source
    std::vector<std::string> tracker_collection_names_;
    std::vector<std::string> calo_collection_names_;
    std::vector<std::string> calo_contrib_collection_names_;
    std::vector<std::string> gp_collection_names_;

    // Data sources
    std::vector<std::unique_ptr<DataSource>> data_sources_;
    
    // Branch relationship mapper for automatic discovery
    std::unique_ptr<BranchRelationshipMapper> relationship_mapper_;

    // Core functionality methods
    std::vector<std::unique_ptr<DataSource>> initializeDataSources();
    bool updateInputNEvents(std::vector<std::unique_ptr<DataSource>>& sources);
    void createMergedTimeslice(std::vector<std::unique_ptr<DataSource>>& sources, std::unique_ptr<TFile>& output_file, TTree* output_tree);
    void setupOutputTree(TTree* tree);
    void writeTimesliceToTree(TTree* tree);

    // Helper methods for collection discovery
    std::vector<std::string> discoverCollectionNames(DataSource& source, const std::string& branch_pattern);
    std::vector<std::string> discoverGPBranches(DataSource& source);
    void copyPodioMetadata(std::vector<std::unique_ptr<DataSource>>& sources, std::unique_ptr<TFile>& output_file);
    void copyAndUpdatePodioMetadataTree(TTree* source_metadata_tree, TFile* output_file);
    
    // Utility methods for collection name mapping
    std::string getCorrespondingContributionCollection(const std::string& calo_collection_name) const;
    std::string getCorrespondingCaloCollection(const std::string& contrib_collection_name) const;
};