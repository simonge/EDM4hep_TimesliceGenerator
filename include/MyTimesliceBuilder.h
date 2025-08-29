// Copyright 2024, Jefferson Science Associates, LLC.
// Subject to the terms in the LICENSE file found in the top-level directory.

#pragma once

#include <JANA/JEventUnfolder.h>
#include <edm4hep/EventHeaderCollection.h>
#include "CollectionTabulatorsEDM4HEP.h"
#include "MyTimesliceBuilderConfig.h"

#include <random>

struct MyTimesliceBuilder : public JEventUnfolder {

    PodioInput<edm4hep::MCParticle> m_event_MCParticles_in {this, {.name = "MCParticles", .is_optional = true}};
    // PodioOutput<edm4hep::MCParticle> m_timeslice_MCParticles_out {this, "ts_MCParticles"};
    // PodioOutput<edm4hep::EventHeader> m_timeslice_info_out {this, "ts_info"};

    std::vector<edm4hep::MCParticle> particle_accumulator;
    size_t parent_idx    = 0;
    int    events_needed = 0;

    MyTimesliceBuilderConfig m_config;

    // Random number generator members
    std::random_device rd;
    std::mt19937 gen;
    std::uniform_real_distribution<float> uniform;
    std::poisson_distribution<> poisson;

    MyTimesliceBuilder(MyTimesliceBuilderConfig config)  
          : m_config(config), 
            gen(rd()),
            uniform(0.0f, config.time_slice_duration),
            poisson(config.time_slice_duration * config.mean_hit_frequency) 
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
            edm4hep::EventHeaderCollection   timeslice_info_out;

            // child.InsertCollection<edm4hep::MCParticle>(std::move(timeslice_particles_out),m_config.tag + "ts_MCParticles");
            // child.InsertCollection<edm4hep::EventHeader>(std::move(timeslice_info_out),m_config.tag + "ts_info");
            child.InsertCollection<edm4hep::MCParticle>(std::move(timeslice_particles_out),"ts_MCParticles");
            child.InsertCollection<edm4hep::EventHeader>(std::move(timeslice_info_out),"ts_info");
            
            return Result::NextChildNextParent;
        }



        std::cout << parent_idx << std::endl;

        for (const auto& particle : *particles_in) {
            LOG_DEBUG(GetLogger()) << "NParticles: " << particle_accumulator.size()
                << "\nCurrent particles:\n"
                << LOG_END;
            particle_accumulator.push_back(particle);
        }

        std::cout << "AcumulatedParticles " << particle_accumulator.size() << std::endl;
        parent_idx++;

        if (parent_idx < events_needed) {
            // Not enough particles yet, keep accumulating
            std::cout << "Not enough particles yet, keep accumulating (need " << events_needed << ", have " << parent_idx << ")" << std::endl;
            
            return Result::KeepChildNextParent;
        } else if (!m_config.static_number_of_hits) {
            events_needed = poisson(gen);
        } //TODO - Gracefully handle events_needed == 0 using Result::KeepChildNextParent

        parent_idx = 0;

        edm4hep::MCParticleCollection    timeslice_particles_out;
        edm4hep::EventHeaderCollection   timeslice_info_out;

        // Now we have particles, build the timeslice
        auto timeslice_nr = child_idx;//1000+parent.GetEventNumber() / 3;
        child.SetEventNumber(timeslice_nr);
        // child.SetParent(const_cast<JEvent*>(&parent));
        // std::cout << "Number of parents " << child.GetParentNumber(JEventLevel::PhysicsEvent) << std::endl;

        for (const auto& particle : particle_accumulator) {
            // Distribute the time of the accumulated particle randomly uniformly throughout the timeslice_duration
            float time_offset = uniform(gen);
            // If use_bunch_crossing is enabled, apply bunch crossing period
            if (m_config.use_bunch_crossing) {
                time_offset = std::floor(time_offset / m_config.bunch_crossing_period) * m_config.bunch_crossing_period;
            }
            auto new_time = particle.getTime() + time_offset;
            auto new_particle = particle.clone();
            new_particle.setTime(new_time);
            timeslice_particles_out.push_back(new_particle);
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
        child.InsertCollection<edm4hep::EventHeader>(std::move(timeslice_info_out),"ts_info");
        // child.InsertCollection<edm4hep::MCParticle>(std::move(timeslice_particles_out),m_config.tag + "ts_MCParticles");
        // child.InsertCollection<edm4hep::EventHeader>(std::move(timeslice_info_out),m_config.tag + "ts_info");

        particle_accumulator.clear(); // Reset for next timeslice

        // std::cout << "Number of parents " << child.GetParentNumber(JEventLevel::PhysicsEvent) << std::endl;

        return Result::NextChildNextParent;
    }
};



