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
    size_t parent_idx = 0;

    MyTimesliceBuilderConfig m_config;

    MyTimesliceBuilder(MyTimesliceBuilderConfig config) : m_config(config) {
        SetTypeName(NAME_OF_THIS);
        SetChildLevel(JEventLevel::Timeslice);
        SetParentLevel(m_config.parent_level);
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
            std::cout << "AcumulatedParticles " << particle_accumulator.size() << std::endl;
            LOG_DEBUG(GetLogger()) << "NParticles: " << particle_accumulator.size()
                << "\nCurrent particles:\n"
                << LOG_END;
            particle_accumulator.push_back(particle);
        }

        // Calculate the number of particles needed for a timeslice using Poisson statistics
        static std::random_device rd;
        static std::mt19937 gen(rd());
        double lambda = m_config.time_slice_duration * m_config.mean_hit_frequency;
        static std::poisson_distribution<> poisson(lambda);
        static std::uniform_real_distribution<float> uniform(0.0f, m_config.time_slice_duration);

        static size_t particles_needed = poisson(gen);

        if (particle_accumulator.size() < particles_needed) {
            // Not enough particles yet, keep accumulating
            std::cout << "Not enough particles yet, keep accumulating (need " << particles_needed << ", have " << particle_accumulator.size() << ")" << std::endl;
            return Result::KeepChildNextParent;
        }
        // Reset for next timeslice
        particles_needed = poisson(gen);
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



