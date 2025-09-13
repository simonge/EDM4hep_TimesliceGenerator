#pragma once

#include "StandaloneMergerConfig.h"
#include "DataSource.h"
#include <edm4hep/MCParticleData.h>
#include <edm4hep/SimTrackerHitData.h>
#include <edm4hep/SimCalorimeterHitData.h>
#include <edm4hep/CaloHitContributionData.h>
#include <edm4hep/EventHeaderData.h>
#include <podio/ROOTWriter.h>
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

    // Global vectors for merged data
    std::vector<edm4hep::MCParticleData> merged_mcparticles;

    std::vector<edm4hep::EventHeaderData> merged_event_headers;
    std::vector<edm4hep::EventHeaderData> merged_sub_event_headers;
    std::unordered_map<std::string, std::vector<edm4hep::SimTrackerHitData>> merged_tracker_hits;
    std::unordered_map<std::string, std::vector<edm4hep::SimCalorimeterHitData>> merged_calo_hits;
    std::unordered_map<std::string, std::vector<edm4hep::CaloHitContributionData>> merged_calo_contributions;
    
    // Global vectors for MCParticle parent-child relationships
    std::vector<podio::ObjectID> merged_mcparticle_parents_refs;
    std::vector<podio::ObjectID> merged_mcparticle_children_refs;

    // Global vectors for merged ObjectID references  
    std::unordered_map<std::string, std::vector<podio::ObjectID>> merged_tracker_hit_particle_refs;
    std::unordered_map<std::string, std::vector<podio::ObjectID>> merged_calo_contrib_particle_refs;
    
    // Global vectors for SimCalorimeterHit-CaloHitContribution relationships
    std::unordered_map<std::string, std::vector<podio::ObjectID>> merged_calo_hit_contributions_refs;

    // Collection names discovered from first source
    std::vector<std::string> tracker_collection_names;
    std::vector<std::string> calo_collection_names;
    std::vector<std::string> calo_contrib_collection_names;

    // Data sources
    std::vector<std::unique_ptr<DataSource>> data_sources_;

    std::vector<std::unique_ptr<DataSource>> initializeDataSources();
    bool updateInputNEvents(std::vector<std::unique_ptr<DataSource>>& sources);
    void createMergedTimeslice(std::vector<std::unique_ptr<DataSource>>& sources, std::unique_ptr<TFile>& output_file, TTree* output_tree);
    void setupOutputTree(TTree* tree);
    void writeTimesliceToTree(TTree* tree);

    // Helper methods for collection discovery
    std::vector<std::string> discoverCollectionNames(DataSource& source, const std::string& branch_pattern);
    void copyPodioMetadata(std::vector<std::unique_ptr<DataSource>>& sources, std::unique_ptr<TFile>& output_file);
    
    std::string getCorrespondingContributionCollection(const std::string& calo_collection_name);
    std::string getCorrespondingCaloCollection(const std::string& contrib_collection_name);
};