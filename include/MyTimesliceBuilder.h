// Copyright 2024, Jefferson Science Associates, LLC.
// Subject to the terms in the LICENSE file found in the top-level directory.

#pragma once

#include <JANA/JEventUnfolder.h>
#include <edm4hep/EventHeaderCollection.h>
#include <edm4hep/MCParticleCollection.h>
#include <edm4hep/SimTrackerHitCollection.h>
#include <edm4hep/SimCalorimeterHitCollection.h>
#include <edm4hep/CaloHitContributionCollection.h>
#include "MyTimesliceBuilderConfig.h"

#include <random>
#include <unordered_map>

// Remove the event struct since we'll store parent event pointers directly

struct MyTimesliceBuilder : public JEventUnfolder {

    PodioInput<edm4hep::MCParticle> m_event_MCParticles_in {this, {.name = "MCParticles", .is_optional = true}};

    std::vector<const JEvent*> parent_event_accumulator;
    bool try_accumulating_hits_setup = true;
    std::vector<std::string> tracker_hit_collection_names;
    std::vector<std::string> calorimeter_hit_collection_names;

    int    events_needed    = 0;
    size_t events_generated = 0;

    MyTimesliceBuilderConfig m_config;

    // Random number generator members
    std::random_device rd;
    std::mt19937 gen;
    std::uniform_real_distribution<float> uniform;
    std::poisson_distribution<> poisson;
    std::normal_distribution<> gaussian;

    MyTimesliceBuilder(MyTimesliceBuilderConfig config)  
          : m_config(config), 
            gen(rd()),
            uniform(0.0f, config.time_slice_duration),
            poisson(config.time_slice_duration * config.mean_event_frequency),
            gaussian(0.0f, config.beam_spread)
    {
        SetTypeName(NAME_OF_THIS);
        SetChildLevel(JEventLevel::Timeslice);
        SetParentLevel(m_config.parent_level);
        if(m_config.static_number_of_events) {
            events_needed = m_config.static_events_per_timeslice;
        } else {
            events_needed = poisson(gen);
        }
    }

    Result Unfold(const JEvent& parent, JEvent& child, int child_idx) override {

        // Accumulate particles from each parent event
        auto* particles_in = m_event_MCParticles_in();
        if (!particles_in) {
            std::cerr << "ERROR: particles_in collection not found! Returning empty collections." << std::endl;

            edm4hep::MCParticleCollection timeslice_particles_out;
            edm4hep::EventHeaderCollection   timeslice_info_out;

            child.InsertCollection<edm4hep::MCParticle>(std::move(timeslice_particles_out),"ts_MCParticles");
            child.InsertCollection<edm4hep::EventHeader>(std::move(timeslice_info_out),"ts_info");
            
            return Result::NextChildNextParent;
        }

        // Loop over all collections in the jana event checking if they are edm4hep::SimTrackerHit types
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


        // Add parent event pointer to accumulator
        parent_event_accumulator.push_back(&parent);

        // std::cout << "AccumulatedEvents " << parent_event_accumulator.size() << std::endl;
        

        if (parent_event_accumulator.size() < events_needed) {
            // Not enough events yet, keep accumulating
            // std::cout << "Not enough events yet, keep accumulating (need " << events_needed << ", have " << parent_event_accumulator.size() << ")" << std::endl;
            
            return Result::KeepChildNextParent;
        } else if (!m_config.static_number_of_events) {
            events_needed = poisson(gen);
        } //TODO - Gracefully handle events_needed == 0 using Result::KeepChildNextParent


        edm4hep::MCParticleCollection    timeslice_particles_out;
        edm4hep::EventHeaderCollection   timeslice_info_out;
        std::unordered_map<std::string, edm4hep::SimTrackerHitCollection> timeslice_tracker_hits_out;
        for (const auto& name : tracker_hit_collection_names) {
            timeslice_tracker_hits_out[name] = edm4hep::SimTrackerHitCollection();
        }
        std::unordered_map<std::string, edm4hep::SimCalorimeterHitCollection> timeslice_calorimeter_hits_out;
        std::unordered_map<std::string, edm4hep::CaloHitContributionCollection> timeslice_calo_contributions_out;
        for (const auto& name : calorimeter_hit_collection_names) {
            timeslice_calorimeter_hits_out[name] = edm4hep::SimCalorimeterHitCollection();
            timeslice_calo_contributions_out[name] = edm4hep::CaloHitContributionCollection();
        }

        // Now we have particles, build the timeslice
        child.SetEventNumber(events_generated);

        for (const auto& parent_event : parent_event_accumulator) {

            // Get collections from the parent event
            auto* particles = parent_event->GetCollection<edm4hep::MCParticle>("MCParticles");

            // Distribute the time of the accumulated particle randomly uniformly throughout the timeslice_duration
            float time_offset = uniform(gen);
            // std::cout << "Time offset for event " << (parent_event - &parent_event_accumulator[0]) << ": " << time_offset << std::endl;
            // If use_bunch_crossing is enabled, apply bunch crossing period
            if (m_config.use_bunch_crossing) {
                time_offset = std::floor(time_offset / m_config.bunch_crossing_period) * m_config.bunch_crossing_period;
            }
            // If attach_to_beam is enabled, apply Gaussian smearing and position based time offset
            // TODO Make this more accurate and configurable.
            if (m_config.attach_to_beam) {
                time_offset += gaussian(gen);
                // Find vertex of first particle with generator status of 1
                auto first_particle = std::find_if(particles->begin(), particles->end(), [](const auto& p) {
                    return p.getGeneratorStatus() == 1;
                });
                if (first_particle != particles->end()) {
                    // Calculate time offset based on distance to 0,0,0 and speed
                    float distance = std::sqrt(first_particle->getVertex().x*first_particle->getVertex().x +
                                                first_particle->getVertex().y*first_particle->getVertex().y +
                                                first_particle->getVertex().z*first_particle->getVertex().z);
                    time_offset += distance / m_config.beam_speed;
                }
            }

            // Map from original particle handle -> cloned particle handle
            std::unordered_map<const edm4hep::MCParticle*, edm4hep::MCParticle> new_old_particle_map;

            // Create new MCParticles
            for (const auto& particle : *particles) {
                auto new_time = particle.getTime() + time_offset;
                auto new_particle = particle.clone();
                new_particle.setTime(new_time);
                new_particle.setGeneratorStatus(particle.getGeneratorStatus() + m_config.generator_status_offset);
                timeslice_particles_out.push_back(new_particle);
                new_old_particle_map[&particle] = new_particle;
            } //TODO: update parent/child map too

            // Create new SimTrackerHits
            for (const auto& collection_name : tracker_hit_collection_names) {
                const auto* hits_collection = parent_event->GetCollection<edm4hep::SimTrackerHit>(collection_name);                
                for(const auto& hit : *hits_collection) {
                    auto new_hit = hit.clone();
                    new_hit.setTime(hit.getTime() + time_offset);
                    auto orig_particle = hit.getParticle();
                    new_hit.setParticle(new_old_particle_map[&orig_particle]);
                    timeslice_tracker_hits_out[collection_name].push_back(new_hit);
                }
            }

            // Create new CaloHits
            for (const auto& collection_name : calorimeter_hit_collection_names) {
                const auto* hits_collection = parent_event->GetCollection<edm4hep::SimCalorimeterHit>(collection_name);
                for (const auto& hit : *hits_collection) {
                    edm4hep::MutableSimCalorimeterHit new_hit;
                    new_hit.setEnergy(hit.getEnergy());
                    new_hit.setPosition(hit.getPosition());
                    new_hit.setCellID(hit.getCellID());

                    // Loop through contributions, cloning, setting new time, MCParticle and replacing in the list
                    for (const auto& contrib : hit.getContributions()) {
                        auto new_contrib = contrib.clone();
                        new_contrib.setTime(contrib.getTime() + time_offset);
                        // Use getParticle instead of deprecated getMCParticle
                        auto orig_particle = contrib.getParticle();
                        new_contrib.setParticle(new_old_particle_map[&orig_particle]);
                        timeslice_calo_contributions_out[collection_name].push_back(new_contrib);
                        new_hit.addToContributions(new_contrib);
                    }
                    timeslice_calorimeter_hits_out[collection_name].push_back(new_hit);
                }
            }

        }

        auto header = edm4hep::MutableEventHeader();
        header.setEventNumber(events_generated);
        header.setRunNumber(0);
        header.setTimeStamp(events_generated);
        timeslice_info_out.push_back(header);


        child.InsertCollection<edm4hep::MCParticle>(std::move(timeslice_particles_out),"MCParticles");
        child.InsertCollection<edm4hep::EventHeader>(std::move(timeslice_info_out),"info");
        for(auto& [collection_name, hit_collection] : timeslice_tracker_hits_out) {
            child.InsertCollection<edm4hep::SimTrackerHit>(std::move(hit_collection), collection_name);
        }
        for (auto& [collection_name, hit_collection] : timeslice_calorimeter_hits_out) {
            child.InsertCollection<edm4hep::SimCalorimeterHit>(std::move(hit_collection), collection_name);
        }
        for (auto& [collection_name, hit_collection] : timeslice_calo_contributions_out) {
            child.InsertCollection<edm4hep::CaloHitContribution>(std::move(hit_collection), collection_name+"Contributions");
        }

        events_generated++;

        parent_event_accumulator.clear(); // Reset for next timeslice
        // parent_event_accumulator.shrink_to_fit();

        return Result::NextChildNextParent;
    }
};



