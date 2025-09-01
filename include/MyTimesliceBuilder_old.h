// Copyright 2024, Jefferson Science Associates, LLC.
// Subject to the terms in the LICENSE file found in the top-level directory.

#pragma once

// TimeframeGenerator2 - Enhanced Timeslice Builder
// Supports:
// - edm4hep::MCParticles (original functionality)
// - edm4hep::SimTrackerHits (new: applies same time offset as MCParticles)
// - edm4eic::ReconstructedParticles with MCRecoParticleAssociations (new: maintains associations to cloned MCParticles)

#include <JANA/JEventUnfolder.h>
#include <edm4hep/EventHeaderCollection.h>
#include <edm4hep/SimTrackerHitCollection.h>
#include "CollectionTabulatorsEDM4HEP.h"
#include "MyTimesliceBuilderConfig.h"

// Conditional EDM4EIC support
#ifdef HAVE_EDM4EIC
#include <edm4eic/ReconstructedParticleCollection.h>
#include <edm4eic/MCRecoParticleAssociationCollection.h>
#endif

#include <random>
#include <map>

struct MyTimesliceBuilder : public JEventUnfolder {

    PodioInput<edm4hep::MCParticle> m_event_MCParticles_in {this, {.name = "MCParticles", .is_optional = true}};
    PodioInput<edm4hep::SimTrackerHit> m_event_SimTrackerHits_in {this, {.name = "SimTrackerHits", .is_optional = true}};
#ifdef HAVE_EDM4EIC
    PodioInput<edm4eic::ReconstructedParticle> m_event_RecoParticles_in {this, {.name = "ReconstructedParticles", .is_optional = true}};
    PodioInput<edm4eic::MCRecoParticleAssociation> m_event_RecoAssociations_in {this, {.name = "MCRecoParticleAssociations", .is_optional = true}};
#endif
    // PodioOutput<edm4hep::MCParticle> m_timeslice_MCParticles_out {this, "ts_MCParticles"};
    // PodioOutput<edm4hep::EventHeader> m_timeslice_info_out {this, "ts_info"};

    std::vector<std::vector<edm4hep::MCParticle>> event_accumulator;
    std::vector<std::vector<edm4hep::SimTrackerHit>> simhit_accumulator;
#ifdef HAVE_EDM4EIC
    std::vector<std::vector<edm4eic::ReconstructedParticle>> recoparticle_accumulator;
    std::vector<std::vector<edm4eic::MCRecoParticleAssociation>> recoassoc_accumulator;
#endif
    size_t parent_idx    = 0;
    int    events_needed = 0;

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
            poisson(config.time_slice_duration * config.mean_hit_frequency),
            gaussian(0.0f, config.beam_spread)
    {
        SetTypeName(NAME_OF_THIS);
        SetChildLevel(JEventLevel::Timeslice);
        SetParentLevel(m_config.parent_level);
        if(m_config.static_number_of_hits) {
            events_needed = m_config.static_hits_per_timeslice;
        } else {
            events_needed = poisson(gen);
        }
        // m_event_MCParticles_in.SetCollectionName(m_config.tag + "MCParticles");
        // m_timeslice_MCParticles_out.SetCollectionName(m_config.tag + "ts_MCParticles");
        // m_timeslice_info_out.SetCollectionName(m_config.tag + "ts_info");
    }

    Result Unfold(const JEvent& parent, JEvent& child, int child_idx) override {

        // Accumulate particles from each parent event
        auto* particles_in = m_event_MCParticles_in();
        if (!particles_in) {
            std::cerr << "ERROR: particles_in collection not found! Returning empty collections." << std::endl;

            edm4hep::MCParticleCollection timeslice_particles_out;
            edm4hep::SimTrackerHitCollection timeslice_simhits_out;
#ifdef HAVE_EDM4EIC
            edm4eic::ReconstructedParticleCollection timeslice_recoparticles_out;
            edm4eic::MCRecoParticleAssociationCollection timeslice_recoassoc_out;
#endif
            edm4hep::EventHeaderCollection   timeslice_info_out;

            // child.InsertCollection<edm4hep::MCParticle>(std::move(timeslice_particles_out),m_config.tag + "ts_MCParticles");
            // child.InsertCollection<edm4hep::EventHeader>(std::move(timeslice_info_out),m_config.tag + "ts_info");
            child.InsertCollection<edm4hep::MCParticle>(std::move(timeslice_particles_out),"ts_MCParticles");
            if (m_config.include_sim_tracker_hits) {
                child.InsertCollection<edm4hep::SimTrackerHit>(std::move(timeslice_simhits_out),"ts_SimTrackerHits");
            }
#ifdef HAVE_EDM4EIC
            if (m_config.include_reconstructed_particles) {
                child.InsertCollection<edm4eic::ReconstructedParticle>(std::move(timeslice_recoparticles_out),"ts_ReconstructedParticles");
                child.InsertCollection<edm4eic::MCRecoParticleAssociation>(std::move(timeslice_recoassoc_out),"ts_MCRecoParticleAssociations");
            }
#endif
            child.InsertCollection<edm4hep::EventHeader>(std::move(timeslice_info_out),"ts_info");
            
            return Result::NextChildNextParent;
        }

        std::vector<edm4hep::MCParticle> particle_accumulator;
        std::vector<edm4hep::SimTrackerHit> simhit_accumulator;
#ifdef HAVE_EDM4EIC
        std::vector<edm4eic::ReconstructedParticle> recoparticle_accumulator;
        std::vector<edm4eic::MCRecoParticleAssociation> recoassoc_accumulator;
#endif

        std::cout << parent_idx << std::endl;

        for (const auto& particle : *particles_in) {
            LOG_DEBUG(GetLogger()) << "NParticles: " << particle_accumulator.size()
                << "\nCurrent particles:\n"
                << LOG_END;
            particle_accumulator.push_back(particle);
        }

        // Accumulate SimTrackerHits if enabled
        if (m_config.include_sim_tracker_hits) {
            auto* simhits_in = m_event_SimTrackerHits_in();
            if (simhits_in) {
                for (const auto& hit : *simhits_in) {
                    simhit_accumulator.push_back(hit);
                }
            }
        }

#ifdef HAVE_EDM4EIC
        // Accumulate ReconstructedParticles and associations if enabled
        if (m_config.include_reconstructed_particles) {
            auto* recoparticles_in = m_event_RecoParticles_in();
            if (recoparticles_in) {
                for (const auto& particle : *recoparticles_in) {
                    recoparticle_accumulator.push_back(particle);
                }
            }
            
            auto* recoassoc_in = m_event_RecoAssociations_in();
            if (recoassoc_in) {
                for (const auto& assoc : *recoassoc_in) {
                    recoassoc_accumulator.push_back(assoc);
                }
            }
        }
#endif

        event_accumulator.push_back(particle_accumulator);
        if (m_config.include_sim_tracker_hits) {
            this->simhit_accumulator.push_back(simhit_accumulator);
        }
#ifdef HAVE_EDM4EIC
        if (m_config.include_reconstructed_particles) {
            this->recoparticle_accumulator.push_back(recoparticle_accumulator);
            this->recoassoc_accumulator.push_back(recoassoc_accumulator);
        }
#endif

        std::cout << "AcumulatedParticles " << particle_accumulator.size() << std::endl;
        parent_idx++;

        if (event_accumulator.size() < events_needed) {
            // Not enough particles yet, keep accumulating
            std::cout << "Not enough particles yet, keep accumulating (need " << events_needed << ", have " << parent_idx << ")" << std::endl;
            
            return Result::KeepChildNextParent;
        } else if (!m_config.static_number_of_hits) {
            events_needed = poisson(gen);
        } //TODO - Gracefully handle events_needed == 0 using Result::KeepChildNextParent

        parent_idx = 0;

        edm4hep::MCParticleCollection    timeslice_particles_out;
        edm4hep::SimTrackerHitCollection timeslice_simhits_out;
#ifdef HAVE_EDM4EIC
        edm4eic::ReconstructedParticleCollection timeslice_recoparticles_out;
        edm4eic::MCRecoParticleAssociationCollection timeslice_recoassoc_out;
        
        // Map to track original MCParticles to cloned MCParticles for association updates
        std::map<edm4hep::MCParticle, edm4hep::MCParticle> mcparticle_mapping;
#endif
        edm4hep::EventHeaderCollection   timeslice_info_out;

        // Now we have particles, build the timeslice
        auto timeslice_nr = child_idx;//1000+parent.GetEventNumber() / 3;
        child.SetEventNumber(timeslice_nr);
        // child.SetParent(const_cast<JEvent*>(&parent));
        // std::cout << "Number of parents " << child.GetParentNumber(JEventLevel::PhysicsEvent) << std::endl;

        for (size_t event_idx = 0; event_idx < event_accumulator.size(); ++event_idx) {
            const auto& event = event_accumulator[event_idx];
            
            // Distribute the time of the accumulated particle randomly uniformly throughout the timeslice_duration
            float time_offset = uniform(gen);
            // If use_bunch_crossing is enabled, apply bunch crossing period
            if (m_config.use_bunch_crossing) {
                time_offset = std::floor(time_offset / m_config.bunch_crossing_period) * m_config.bunch_crossing_period;
            }
            // If attach_to_beam is enabled, apply Gaussian smearing and position based time offset
            // TODO Make this more accurate and configurable.
            if (m_config.attach_to_beam) {
                time_offset += gaussian(gen);
                // Find vertex of first particle with generator status of 1
                auto first_particle = std::find_if(event.begin(), event.end(), [](const auto& p) {
                    return p.getGeneratorStatus() == 1;
                });
                if (first_particle != event.end()) {
                    // Calculate time offset based on distance to 0,0,0 and speed
                    float distance = std::sqrt(std::pow(first_particle->getVertex().x, 2) +
                                                std::pow(first_particle->getVertex().y, 2) +
                                                std::pow(first_particle->getVertex().z, 2));
                    time_offset += distance / m_config.beam_speed;
                }
            }

            // Process MCParticles with time offset
            for (const auto& particle : event) {
                auto new_time = particle.getTime() + time_offset;
                auto new_particle = particle.clone();
                new_particle.setTime(new_time);
                new_particle.setGeneratorStatus(particle.getGeneratorStatus() + m_config.generator_status_offset);
                timeslice_particles_out.push_back(new_particle);
                
#ifdef HAVE_EDM4EIC
                // Track mapping for association updates
                if (m_config.include_reconstructed_particles) {
                    mcparticle_mapping[particle] = new_particle;
                }
#endif
            }
            
            // Process SimTrackerHits with the same time offset
            if (m_config.include_sim_tracker_hits && event_idx < simhit_accumulator.size()) {
                const auto& simhits_event = simhit_accumulator[event_idx];
                for (const auto& hit : simhits_event) {
                    auto new_time = hit.getTime() + time_offset;
                    auto new_hit = hit.clone();
                    new_hit.setTime(new_time);
                    timeslice_simhits_out.push_back(new_hit);
                }
            }
            
#ifdef HAVE_EDM4EIC
            // Process ReconstructedParticles with the same time offset
            if (m_config.include_reconstructed_particles && event_idx < recoparticle_accumulator.size()) {
                const auto& recoparticles_event = recoparticle_accumulator[event_idx];
                for (const auto& particle : recoparticles_event) {
                    auto new_time = particle.getTime() + time_offset;
                    auto new_particle = particle.clone();
                    new_particle.setTime(new_time);
                    timeslice_recoparticles_out.push_back(new_particle);
                }
                
                // Process associations (update to reference cloned MCParticles)
                if (event_idx < recoassoc_accumulator.size()) {
                    const auto& recoassoc_event = recoassoc_accumulator[event_idx];
                    for (const auto& assoc : recoassoc_event) {
                        auto new_assoc = assoc.clone();
                        
                        // Update association to reference cloned MCParticle if available
                        // Note: This depends on the EDM4EIC API - the exact method may vary
                        auto original_mc = assoc.getSim();  // Get original MCParticle from association
                        auto it = mcparticle_mapping.find(original_mc);
                        if (it != mcparticle_mapping.end()) {
                            new_assoc.setSim(it->second);  // Set to cloned MCParticle
                        }
                        
                        timeslice_recoassoc_out.push_back(new_assoc);
                    }
                }
            }
#endif
        }

        auto header = edm4hep::MutableEventHeader();
        header.setEventNumber(timeslice_nr);
        header.setRunNumber(0);
        header.setTimeStamp(timeslice_nr);
        timeslice_info_out.push_back(header);

        // LOG_DEBUG(GetLogger()) << "MyTimesliceBuilder: Built timeslice " << timeslice_nr
        //     << "\nTimeslice particles out:\n"
        //     << TabulateParticlesEDM4HEP(&timeslice_particles_out)
        //     << LOG_END;

        child.InsertCollection<edm4hep::MCParticle>(std::move(timeslice_particles_out),"ts_MCParticles");
        if (m_config.include_sim_tracker_hits) {
            child.InsertCollection<edm4hep::SimTrackerHit>(std::move(timeslice_simhits_out),"ts_SimTrackerHits");
        }
#ifdef HAVE_EDM4EIC
        if (m_config.include_reconstructed_particles) {
            child.InsertCollection<edm4eic::ReconstructedParticle>(std::move(timeslice_recoparticles_out),"ts_ReconstructedParticles");
            child.InsertCollection<edm4eic::MCRecoParticleAssociation>(std::move(timeslice_recoassoc_out),"ts_MCRecoParticleAssociations");
        }
#endif
        child.InsertCollection<edm4hep::EventHeader>(std::move(timeslice_info_out),"ts_info");
        // child.InsertCollection<edm4hep::MCParticle>(std::move(timeslice_particles_out),m_config.tag + "ts_MCParticles");
        // child.InsertCollection<edm4hep::EventHeader>(std::move(timeslice_info_out),m_config.tag + "ts_info");

        event_accumulator.clear(); // Reset for next timeslice
        if (m_config.include_sim_tracker_hits) {
            simhit_accumulator.clear();
        }
#ifdef HAVE_EDM4EIC
        if (m_config.include_reconstructed_particles) {
            recoparticle_accumulator.clear();
            recoassoc_accumulator.clear();
        }
#endif

        // std::cout << "Number of parents " << child.GetParentNumber(JEventLevel::PhysicsEvent) << std::endl;

        return Result::NextChildNextParent;
    }
};



