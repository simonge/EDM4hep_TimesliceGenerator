#pragma once

#include <JANA/JEventSource.h>
#include <JANA/Components/JPodioOutput.h>
#include <edm4hep/EventHeaderCollection.h>
#include <edm4hep/MCParticleCollection.h>
#include <edm4hep/SimTrackerHitCollection.h>
#include <edm4hep/SimCalorimeterHitCollection.h>
#include <edm4hep/CaloHitContributionCollection.h>

#include <podio/ROOTReader.h>
#include <iostream>
#include <random>
#include <numeric>

struct MyEventFileReader : public JEventSource {

    std::string m_tag{""};
    std::string m_filename;
    podio::ROOTReader m_reader;
    size_t m_event_counter = 0;
    size_t m_event_in_file_counter = 0;
    size_t m_total_events = 0;
    bool m_loop_forever = true;
    std::vector<size_t> event_indices;
    std::vector<std::pair<std::string, std::string>> m_collections_to_read;// {{"MCParticles", "edm4hep::MCParticle"}, {"EventHeader", "edm4hep::EventHeader"}};    


    MyEventFileReader(const std::string& filename) : m_filename(filename) {
        SetTypeName(NAME_OF_THIS);
        SetResourceName(filename);
        SetCallbackStyle(CallbackStyle::ExpertMode);
        m_reader.openFile(filename);
        m_total_events = m_reader.getEntries("events");
        event_indices.resize(m_total_events);
        std::iota(event_indices.begin(), event_indices.end(), 0);
        // std::shuffle(event_indices.begin(), event_indices.end(), std::mt19937{std::random_device{}()});
    }

    void SetTag(std::string tag) { m_tag = std::move(tag); }
    const std::string& GetTag() const { return m_tag; }

    void Open() override { /* Already opened in constructor */ }
    void Close() override { 
        // m_reader.closeFile(); 
    }

    Result Emit(JEvent& event) override {
        using clock = std::chrono::high_resolution_clock;
        auto t_emit_start = clock::now();

        if(m_event_in_file_counter >= m_total_events) {
            if(m_loop_forever) {
                m_event_in_file_counter = 0;
                // std::shuffle(event_indices.begin(), event_indices.end(), std::mt19937{std::random_device{}()}); //Shuffling and reading out of order is a bad plan
            } else {
                return Result::FailureFinished;
            }
        }

    // auto t_read_start = clock::now();
    auto frame_data = m_reader.readEntry("events", event_indices[m_event_in_file_counter]);//, m_collections_to_read); // TODO update with newer podio version
    // auto t_read_end = clock::now();
    // std::cout << "[MyEventFileReader] Time to readEntry: " << std::chrono::duration<double, std::milli>(t_read_end - t_read_start).count() << " ms\n";

    // auto t_frame_start = clock::now();
    auto frame      = std::make_unique<podio::Frame>(std::move(frame_data));
    // auto t_frame_end = clock::now();
    // std::cout << "[MyEventFileReader] Time to construct Frame: " << std::chrono::duration<double, std::milli>(t_frame_end - t_frame_start).count() << " ms\n";

    // auto t_collections_start = clock::now();
    // For the first event fill pairs of collection names and types to move to JEvent
    if(m_collections_to_read.empty()) {
        for (const auto& coll_name : frame->getAvailableCollections()) {
            const podio::CollectionBase* coll = frame->get(coll_name);
            const auto& coll_type = coll->getValueTypeName();
            if(coll_type == "edm4hep::MCParticle" || coll_type == "edm4hep::EventHeader" || coll_type == "edm4hep::SimTrackerHit" || coll_type == "edm4hep::SimCalorimeterHit" || coll_type == "edm4hep::CaloHitContribution") {
                m_collections_to_read.push_back({std::string(coll_name), std::string(coll_type)});
            }
        }
    }
    
    // auto t_collections_end = clock::now();
    // std::cout << "[MyEventFileReader] Time to identify collections to read: " << std::chrono::duration<double, std::milli>(t_collections_end - t_collections_start).count() << " ms\n";
    for (const auto& [coll_name, coll_type] : m_collections_to_read) {
        // auto t_coll_start = clock::now();

            if(coll_type == "edm4hep::MCParticle") {
                event.InsertCollectionAlreadyInFrame<edm4hep::MCParticle>(frame->get(coll_name), coll_name);
            }
            else if(coll_type == "edm4hep::EventHeader") {
                event.InsertCollectionAlreadyInFrame<edm4hep::EventHeader>(frame->get(coll_name), coll_name);
            }
            else if (coll_type == "edm4hep::SimTrackerHit") {
                event.InsertCollectionAlreadyInFrame<edm4hep::SimTrackerHit>(frame->get(coll_name), coll_name);
            }
            else if (coll_type == "edm4hep::SimCalorimeterHit") {
                event.InsertCollectionAlreadyInFrame<edm4hep::SimCalorimeterHit>(frame->get(coll_name), coll_name);
            }
            else if (coll_type == "edm4hep::CaloHitContribution")
            {
                event.InsertCollectionAlreadyInFrame<edm4hep::CaloHitContribution>(frame->get(coll_name), coll_name);
            }
            
            else {
                std::cerr << "Warning: Unhandled collection type '" << coll_type << "' for collection '" << coll_name << "'" << std::endl;
            }
            // auto t_coll_end = clock::now();
            // std::cout << "[MyEventFileReader] Time to handle collection '" << coll_name << "': " << std::chrono::duration<double, std::milli>(t_coll_end - t_coll_start).count() << " ms\n";

        }


    // auto t_emit_end = clock::now();
    event.Insert(frame.release());
    // std::cout << "[MyEventFileReader] Total time to emit event: " << std::chrono::duration<double, std::milli>(t_emit_end - t_emit_start).count() << " ms\n";
    // std::cout << "Read event " << m_event_counter << " from " << m_filename << std::endl;
    m_event_counter++;
    m_event_in_file_counter++;
    return Result::Success;
    }
};