#pragma once

#include "OutputHandler.h"
#include <edm4hep/MCParticleData.h>
#include <edm4hep/SimTrackerHitData.h>
#include <edm4hep/SimCalorimeterHitData.h>
#include <edm4hep/CaloHitContributionData.h>
#include <edm4hep/EventHeaderData.h>
#include <podio/ObjectID.h>
#include <TFile.h>
#include <TTree.h>
#include <unordered_map>
#include <vector>
#include <string>
#include <memory>

// Struct to organize all merged EDM4hep collections
struct EDM4hepMergedCollections {
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
    std::vector<podio::ObjectID> mcparticle_daughters_refs;
    std::unordered_map<std::string, std::vector<podio::ObjectID>> tracker_hit_particle_refs;
    std::unordered_map<std::string, std::vector<podio::ObjectID>> calo_contrib_particle_refs;
    std::unordered_map<std::string, std::vector<podio::ObjectID>> calo_hit_contributions_refs;
    
    // GP (Global Parameter) branches
    std::unordered_map<std::string, std::vector<std::string>> gp_key_branches;
    std::vector<std::vector<int>> gp_int_values;
    std::vector<std::vector<float>> gp_float_values;
    std::vector<std::vector<double>> gp_double_values;
    std::vector<std::vector<std::string>> gp_string_values;
    
    void clear();
};

/**
 * @class EDM4hepOutputHandler
 * @brief Concrete implementation of OutputHandler for EDM4hep format
 * 
 * Handles writing merged timeslice data in EDM4hep format using ROOT TTree.
 */
class EDM4hepOutputHandler : public OutputHandler {
public:
    EDM4hepOutputHandler() = default;
    ~EDM4hepOutputHandler() override = default;

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
    
    std::string getFormatName() const override { return "EDM4hep"; }

private:
    std::unique_ptr<TFile> output_file_;
    TTree* output_tree_ = nullptr;
    EDM4hepMergedCollections collections_;
    
    // Collection names discovered from sources
    std::vector<std::string> tracker_collection_names_;
    std::vector<std::string> calo_collection_names_;
    std::vector<std::string> gp_collection_names_;
    
    size_t current_timeslice_number_ = 0;

    // Helper methods
    void setupOutputTree();
    void discoverCollections(const std::vector<std::unique_ptr<DataSource>>& sources);
    std::vector<std::string> discoverCollectionNames(DataSource& source, const std::string& branch_pattern);
    std::vector<std::string> discoverGPBranches(DataSource& source);
    void copyPodioMetadata(const std::vector<std::unique_ptr<DataSource>>& sources);
    void copyAndUpdatePodioMetadataTree(TTree* source_metadata_tree, TFile* output_file);
    std::string getCorrespondingContributionCollection(const std::string& calo_collection_name) const;
    std::string getCorrespondingCaloCollection(const std::string& contrib_collection_name) const;
};
