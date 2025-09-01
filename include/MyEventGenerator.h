// Copyright 2024, Jefferson Science Associates, LLC.
// Subject to the terms in the LICENSE file found in the top-level directory.

#pragma once

#include <JANA/JEventSource.h>
// #include <JANA/Components/JHasInputs.h>
#include <JANA/Components/JPodioOutput.h>
#include <edm4hep/EventHeaderCollection.h>
#include <edm4hep/MCParticleCollection.h>
#include "CollectionTabulatorsEDM4HEP.h"

#include <random>

struct MyEventGenerator : public JEventSource {

    // PodioOutput<edm4hep::MCParticle> m_particles_out {this, "MCParticles"};

    MyEventGenerator() {
        SetTypeName(NAME_OF_THIS);
        SetCallbackStyle(CallbackStyle::ExpertMode);
    }

    std::string m_tag{""};

    void SetTag(std::string tag) { m_tag = std::move(tag); }
    const std::string& GetTag() const { return m_tag; }

    void Open() override { }

    void Close() override { }

    Result Emit(JEvent& event) override {
        auto event_nr = event.GetEventNumber();

        if(event.GetLevel() == JEventLevel::Timeslice) {
            return Result::Success;
        }

        edm4hep::MCParticleCollection mc_particles_out;

        // Set time to a random value from a gaussian with a width of 1 and mean 0
        
        std::random_device rd;
        std::mt19937 gen(rd());
        std::normal_distribution<float> d(0, 1);
        float time = d(gen);

        // Emit particles per event/timeslice
        edm4hep::MutableMCParticle mc_particle1;
        mc_particle1.setPDG(22);
        mc_particle1.setTime(time);
        mc_particles_out.push_back(mc_particle1);

        // LOG_DEBUG(GetLogger()) << "MySource: Emitted " << GetLevel() << " " << event.GetEventNumber() << "\n"
        //     << TabulateParticlesEDM4HEP(&mc_particles_out)
        //     << LOG_END;

        std::cout << "Emitting event " << event_nr << " with " << mc_particles_out.size() << " particles." << std::endl;
        std::cout << "At Level " << static_cast<int>(event.GetLevel()) << " Tag " << m_tag << std::endl;

        event.InsertCollection<edm4hep::MCParticle>(std::move(mc_particles_out), "MCParticles");

        edm4hep::EventHeaderCollection info;
        info.push_back(edm4hep::MutableEventHeader(event_nr, 0, 0, 0));
        event.InsertCollection<edm4hep::EventHeader>(std::move(info), m_tag+"evt_info");
        return Result::Success;
    }
};
