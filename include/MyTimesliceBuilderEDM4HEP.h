#pragma once

#include <JANA/JEventUnfolder.h>
#include <edm4hep/EventHeaderCollection.h>
#include <edm4hep/MCParticleCollection.h>
#include <edm4hep/SimTrackerHitCollection.h>
#include <edm4hep/SimCalorimeterHitCollection.h>
#include <edm4hep/CaloHitContributionCollection.h>
#include "CollectionTabulatorsEDM4HEP.h"
#include "MyTimesliceBuilderConfig.h"

#include <random>

struct timeslice_event{
    const edm4hep::MCParticleCollection* particles = nullptr;
    std::map<std::string, const edm4hep::SimTrackerHitCollection*> trackerHits;
    std::map<std::string, const edm4hep::SimCalorimeterHitCollection*> calorimeterHits;
    std::map<std::string, const edm4hep::CaloHitContributionCollection*> caloContributions;
    std::string original_file_tag; // Store the original file tag to preserve original names
};

struct MyTimesliceBuilderEDM4HEP : public JEventUnfolder {

    PodioInput<edm4hep::MCParticle> m_timeslice_MCParticles_in {this, {.name = "ts_MCParticles", .is_optional = true}};
    PodioInput<edm4hep::EventHeader> m_timeslice_info_in {this, {.name = "ts_info", .is_optional = true}};

    std::vector<timeslice_event> timeslice_accumulator;
    bool try_accumulating_hits_setup = true;
    std::vector<std::string> tracker_hit_collection_names;
    std::vector<std::string> calorimeter_hit_collection_names;

    size_t parent_idx = 0;
    int timeslices_needed = 1; // For merger, we process one timeslice at a time

    MyTimesliceBuilderConfig m_config;

    MyTimesliceBuilderEDM4HEP(MyTimesliceBuilderConfig config)  
          : m_config(config)
    {
        SetTypeName(NAME_OF_THIS);
        SetChildLevel(JEventLevel::Timeslice);
        SetParentLevel(m_config.parent_level);
    }

    Result Unfold(const JEvent& parent, JEvent& child, int child_idx) override {

        // For merger, we're reading timeslices and merging them
        auto* particles_in = m_timeslice_MCParticles_in();
        
        // Loop over all collections in the jana event checking if they are hit types
        if (try_accumulating_hits_setup) {
            for (const auto& coll_name : parent.GetAllCollectionNames()) {
                const podio::CollectionBase* coll = parent.GetCollectionBase(coll_name);
                const auto& coll_type = coll->getValueTypeName();

                if (coll_type == "edm4hep::SimTrackerHit") {
                    tracker_hit_collection_names.push_back(coll_name);
                }
                if (coll_type == "edm4hep::SimCalorimeterHit") {
                    calorimeter_hit_collection_names.push_back(coll_name);
                }
            }
            try_accumulating_hits_setup = false; // Only do this once
        }

        timeslice_event ts_evt;
        // Store pointer to MCParticle collection (const)
        ts_evt.particles = particles_in;

        // Get the original file tag to preserve names
        auto source = parent.GetEventSource();
        if (source) {
            // Try to get tag from source
            ts_evt.original_file_tag = source->GetTag();
        }
        
        // Store pointers to SimTrackerHit collections (const)
        for (const auto& hit_collection_name : tracker_hit_collection_names) {
            try {
                const auto& hit_collection = parent.GetCollection<edm4hep::SimTrackerHit>(hit_collection_name);
                ts_evt.trackerHits[hit_collection_name] = hit_collection;
            } catch (const std::exception& e) {
                // Collection might not exist in this timeslice
                continue;
            }
        }

        // Store pointers to SimCalorimeterHit collections (const)
        for (const auto& hit_collection_name : calorimeter_hit_collection_names) {
            try {
                const auto& hit_collection = parent.GetCollection<edm4hep::SimCalorimeterHit>(hit_collection_name);
                ts_evt.calorimeterHits[hit_collection_name] = hit_collection;
                std::string hit_contribution_name = hit_collection_name + "Contributions";
                try {
                    const auto& hit_contribution = parent.GetCollection<edm4hep::CaloHitContribution>(hit_contribution_name);
                    ts_evt.caloContributions[hit_collection_name] = hit_contribution;
                } catch (const std::exception& e) {
                    // Contribution collection might not exist
                }
            } catch (const std::exception& e) {
                // Collection might not exist in this timeslice
                continue;
            }
        }

        // Add to accumulator
        timeslice_accumulator.push_back(std::move(ts_evt));

        std::cout << "Accumulated timeslices: " << timeslice_accumulator.size() << std::endl;
        parent_idx++;

        if (timeslice_accumulator.size() < timeslices_needed) {
            // Not enough timeslices yet, keep accumulating
            return Result::KeepChildNextParent;
        }

        parent_idx = 0;

        // Now create merged collections
        edm4hep::MCParticleCollection merged_particles_out;
        edm4hep::EventHeaderCollection merged_info_out;
        std::map<std::string, edm4hep::SimTrackerHitCollection> merged_tracker_hits_out;
        std::map<std::string, edm4hep::SimCalorimeterHitCollection> merged_calorimeter_hits_out;
        std::map<std::string, edm4hep::CaloHitContributionCollection> merged_calo_contributions_out;

        // Initialize collections
        for (const auto& name : tracker_hit_collection_names) {
            merged_tracker_hits_out[name] = edm4hep::SimTrackerHitCollection();
        }
        for (const auto& name : calorimeter_hit_collection_names) {
            merged_calorimeter_hits_out[name] = edm4hep::SimCalorimeterHitCollection();
            merged_calo_contributions_out[name] = edm4hep::CaloHitContributionCollection();
        }

        auto merged_timeslice_nr = child_idx;
        child.SetEventNumber(merged_timeslice_nr);

        // Merge all accumulated timeslices
        for (const auto& ts_event : timeslice_accumulator) {
            
            // Merge MCParticles - preserve original names by prepending file tag
            if (ts_event.particles) {
                for (const auto& particle : *(ts_event.particles)) {
                    auto merged_particle = particle.clone();
                    // Could add file tag information as metadata if needed
                    merged_particles_out.push_back(merged_particle);
                }
            }

            // Merge SimTrackerHits - sum all hits together preserving original collection names
            for (const auto& [collection_name, hits_collection] : ts_event.trackerHits) {
                if (hits_collection) {
                    for(const auto& hit : *hits_collection) {
                        auto merged_hit = hit.clone();
                        // Remove "ts_" prefix to restore original name
                        std::string original_name = collection_name;
                        if (original_name.substr(0, 3) == "ts_") {
                            original_name = original_name.substr(3);
                        }
                        
                        // Initialize collection if it doesn't exist
                        if (merged_tracker_hits_out.find(original_name) == merged_tracker_hits_out.end()) {
                            merged_tracker_hits_out[original_name] = edm4hep::SimTrackerHitCollection();
                        }
                        merged_tracker_hits_out[original_name].push_back(merged_hit);
                    }
                }
            }

            // Merge CaloHits
            for (const auto& [collection_name, hits_collection] : ts_event.calorimeterHits) {
                if (hits_collection) {
                    for (const auto& hit : *hits_collection) {
                        auto merged_hit = hit.clone();
                        
                        // Remove "ts_" prefix to restore original name
                        std::string original_name = collection_name;
                        if (original_name.substr(0, 3) == "ts_") {
                            original_name = original_name.substr(3);
                        }
                        
                        // Initialize collection if it doesn't exist
                        if (merged_calorimeter_hits_out.find(original_name) == merged_calorimeter_hits_out.end()) {
                            merged_calorimeter_hits_out[original_name] = edm4hep::SimCalorimeterHitCollection();
                        }
                        merged_calorimeter_hits_out[original_name].push_back(merged_hit);
                    }
                }
            }

            // Merge CaloHitContributions
            for (const auto& [collection_name, contrib_collection] : ts_event.caloContributions) {
                if (contrib_collection) {
                    for (const auto& contrib : *contrib_collection) {
                        auto merged_contrib = contrib.clone();
                        
                        // Remove "ts_" prefix to restore original name
                        std::string original_name = collection_name;
                        if (original_name.substr(0, 3) == "ts_") {
                            original_name = original_name.substr(3);
                        }
                        
                        // Initialize collection if it doesn't exist
                        if (merged_calo_contributions_out.find(original_name) == merged_calo_contributions_out.end()) {
                            merged_calo_contributions_out[original_name] = edm4hep::CaloHitContributionCollection();
                        }
                        merged_calo_contributions_out[original_name].push_back(merged_contrib);
                    }
                }
            }
        }

        auto header = edm4hep::MutableEventHeader();
        header.setEventNumber(merged_timeslice_nr);
        header.setRunNumber(0);
        header.setTimeStamp(merged_timeslice_nr);
        merged_info_out.push_back(header);

        // Insert merged collections with original names (without ts_ prefix)
        child.InsertCollection<edm4hep::MCParticle>(std::move(merged_particles_out),"ts_hits");
        child.InsertCollection<edm4hep::EventHeader>(std::move(merged_info_out),"ts_info");
        
        for(auto& [collection_name, hit_collection] : merged_tracker_hits_out) {
            child.InsertCollection<edm4hep::SimTrackerHit>(std::move(hit_collection), collection_name);
        }
        for (auto& [collection_name, hit_collection] : merged_calorimeter_hits_out) {
            child.InsertCollection<edm4hep::SimCalorimeterHit>(std::move(hit_collection), collection_name);
        }
        for (auto& [collection_name, hit_collection] : merged_calo_contributions_out) {
            child.InsertCollection<edm4hep::CaloHitContribution>(std::move(hit_collection), collection_name + "Contributions");
        }

        timeslice_accumulator.clear(); // Reset for next merge
        
        return Result::NextChildNextParent;
    }
};