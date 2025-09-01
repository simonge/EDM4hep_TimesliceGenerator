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
    std::vector<std::string> m_collections_to_read {"MCParticles", "EventHeader"};    
    std::vector<std::string> m_sim_tracker_hit_collections;
    std::vector<std::string> m_sim_calorimeter_hit_collections;

    MyEventFileReader(const std::string& filename) : m_filename(filename) {
        SetTypeName(NAME_OF_THIS);
        SetResourceName(filename);
        SetCallbackStyle(CallbackStyle::ExpertMode);
        m_reader.openFile(m_filename);
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

        if(m_event_in_file_counter >= m_total_events) {
            if(m_loop_forever) {
                m_event_in_file_counter = 0;
                // std::shuffle(event_indices.begin(), event_indices.end(), std::mt19937{std::random_device{}()}); //Shuffling and reading out of order is a bad plan
            } else {
                return Result::FailureFinished;
            }
        }

        auto frame_data = m_reader.readEntry("events", event_indices[m_event_in_file_counter]);//, m_collections_to_read); // TODO update with newer podio version
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

        
        // If m_sim_tracker_hit_collections is empty, loop over all collection in the frame, finding those which are edm4hep::SimTrackerHits and inserting them into the event
        if (m_sim_tracker_hit_collections.empty()) {
            for (const auto& coll_name : frame->getAvailableCollections()) {
                const podio::CollectionBase* coll = frame->get(coll_name);
                const auto& coll_type = coll->getValueTypeName();

                if (coll_type == "edm4hep::SimTrackerHit") {
                    event.InsertCollectionAlreadyInFrame<edm4hep::SimTrackerHit>(coll, coll_name);
                }
            }
        } else {
            for (const auto& coll_name : m_sim_tracker_hit_collections) {
                const podio::CollectionBase* coll = frame->get(coll_name);
                if (coll) {
                    event.InsertCollectionAlreadyInFrame<edm4hep::SimTrackerHit>(coll, coll_name);
                }
            }
        }

        // If m_sim_calorimeter_hit_collections is empty, loop over all collection in the frame, finding those which are edm4hep::SimCalorimeterHits and inserting them into the event
        if (m_sim_calorimeter_hit_collections.empty()) {
            for (const auto& coll_name : frame->getAvailableCollections()) {
                const podio::CollectionBase* coll = frame->get(coll_name);
                const auto& coll_type = coll->getValueTypeName();

                if (coll_type == "edm4hep::SimCalorimeterHit") {
                    // Ensure there is an edm4hep::CalorimeterHitContribution collection associated with this collection                    
                    std::string contribution_name = coll_name + "Contributions";
                    std::cout << "Looking for contribution collection: " << contribution_name << std::endl;

                    const podio::CollectionBase* contribution_coll = frame->get(contribution_name);
                    if (contribution_coll) {
                        event.InsertCollectionAlreadyInFrame<edm4hep::CaloHitContribution>(contribution_coll, contribution_name);
                        event.InsertCollectionAlreadyInFrame<edm4hep::SimCalorimeterHit>(coll, coll_name);
                    }
                }
               
            }
        } else {
            for (const auto& coll_name : m_sim_calorimeter_hit_collections) {
                const podio::CollectionBase* coll = frame->get(coll_name);
                if (coll) {
                    // Ensure there is an edm4hep::CalorimeterHitContribution collection associated with this collection                    
                    std::string contribution_name = coll_name + "Contribution";
                    const podio::CollectionBase* contribution_coll = frame->get(contribution_name);
                    if (contribution_coll) {
                        event.InsertCollectionAlreadyInFrame<edm4hep::CaloHitContribution>(contribution_coll, contribution_name);
                        event.InsertCollectionAlreadyInFrame<edm4hep::SimCalorimeterHit>(coll, coll_name);
                    }
                }
            }
        }

        event.Insert(frame.release());
        // std::cout << "Read event " << m_event_counter << " from " << m_filename << std::endl;
        m_event_counter++;
        m_event_in_file_counter++;
        return Result::Success;
    }
};