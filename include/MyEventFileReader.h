#pragma once

#include <JANA/JEventSource.h>
#include <JANA/Components/JPodioOutput.h>
#include <edm4hep/EventHeaderCollection.h>
#include <edm4hep/MCParticleCollection.h>
#include <podio/ROOTReader.h>
#include <iostream>

struct MyEventFileReader : public JEventSource {

    std::string m_tag{""};
    std::string m_filename;
    podio::ROOTReader m_reader;
    int m_event_counter = 0;
    int m_total_events = 0;
    std::vector<std::string> m_collections_to_read {"MCParticles", "EventHeader"};

    MyEventFileReader(const std::string& filename) : m_filename(filename) {
        SetTypeName(NAME_OF_THIS);
        SetResourceName(filename);
        SetCallbackStyle(CallbackStyle::ExpertMode);
        m_reader.openFile(m_filename);
        m_total_events = m_reader.getEntries("events");
    }

    void SetTag(std::string tag) { m_tag = std::move(tag); }
    const std::string& GetTag() const { return m_tag; }

    void Open() override { /* Already opened in constructor */ }
    void Close() override { 
        // m_reader.closeFile(); 
    }

    Result Emit(JEvent& event) override {
        // if (m_event_counter >= m_total_events) {
        //     return Result::Finished;
        // }    

        auto frame_data = m_reader.readNextEntry("events");//, m_collections_to_read);
        auto frame      = std::make_unique<podio::Frame>(std::move(frame_data));
        

        for (const std::string& coll_name : m_collections_to_read) {
            const podio::CollectionBase* coll = frame->get(coll_name);
            const auto& coll_type = coll->getValueTypeName();

            if(coll_type == "edm4hep::MCParticle") {
                event.InsertCollectionAlreadyInFrame<edm4hep::MCParticle>(coll, coll_name);
            }
            else if(coll_type == "edm4hep::EventHeader") {
                event.InsertCollectionAlreadyInFrame<edm4hep::EventHeader>(coll, coll_name);
            }
            else {
                std::cerr << "Warning: Unhandled collection type '" << coll_type << "' for collection '" << coll_name << "'" << std::endl;
            }

        }

        // const auto& mc_particles = frame->get<edm4hep::MCParticleCollection>("MCParticles");

        // const auto& event_headers = frame->get<edm4hep::EventHeaderCollection>("EventHeader");

        // event.InsertCollection<edm4hep::MCParticle>(std::move(mc_particles), "MCParticles");
        // event.InsertCollection<edm4hep::EventHeader>(std::move(event_headers), "EventHeader");


        event.Insert(frame.release());
        // std::cout << "Read event " << m_event_counter << " from " << m_filename << std::endl;
        m_event_counter++;
        return Result::Success;
    }
};