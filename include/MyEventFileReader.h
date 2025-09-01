#pragma once

#include <JANA/JEventSource.h>
#include <JANA/Components/JPodioOutput.h>
#include <edm4hep/EventHeaderCollection.h>
#include <edm4hep/MCParticleCollection.h>
#include <edm4hep/SimTrackerHitCollection.h>
#include <podio/ROOTReader.h>
#include <iostream>
#include <random>
#include <numeric>

// Conditional EDM4EIC support
#ifdef HAVE_EDM4EIC
#include <edm4eic/ReconstructedParticleCollection.h>
#include <edm4eic/MCRecoParticleAssociationCollection.h>
#endif

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
    
    // Collection lists for timeframe generation
    std::vector<std::string> m_sim_tracker_hit_collections;
    std::vector<std::string> m_reconstructed_particle_collections;

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
    
    // Configuration methods for collection lists
    void SetSimTrackerHitCollections(const std::vector<std::string>& collections) {
        m_sim_tracker_hit_collections = collections;
        // Add these collections to the read list
        for (const auto& coll : collections) {
            if (std::find(m_collections_to_read.begin(), m_collections_to_read.end(), coll) == m_collections_to_read.end()) {
                m_collections_to_read.push_back(coll);
            }
        }
    }
    
    void SetReconstructedParticleCollections(const std::vector<std::string>& collections) {
        m_reconstructed_particle_collections = collections;
        // Add these collections to the read list
        for (const auto& coll : collections) {
            if (std::find(m_collections_to_read.begin(), m_collections_to_read.end(), coll) == m_collections_to_read.end()) {
                m_collections_to_read.push_back(coll);
            }
        }
        
        // Also add associated collection names if they follow naming conventions
        // For ReconstructedParticles, we typically also want MCRecoParticleAssociations
        std::string assoc_name = "MCRecoParticleAssociations";
        if (std::find(m_collections_to_read.begin(), m_collections_to_read.end(), assoc_name) == m_collections_to_read.end()) {
            m_collections_to_read.push_back(assoc_name);
        }
    }
    
    const std::vector<std::string>& GetSimTrackerHitCollections() const { return m_sim_tracker_hit_collections; }
    const std::vector<std::string>& GetReconstructedParticleCollections() const { return m_reconstructed_particle_collections; }

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
            else if(coll_type == "edm4hep::SimTrackerHit") {
                event.InsertCollectionAlreadyInFrame<edm4hep::SimTrackerHit>(coll, coll_name);
            }
#ifdef HAVE_EDM4EIC
            else if(coll_type == "edm4eic::ReconstructedParticle") {
                event.InsertCollectionAlreadyInFrame<edm4eic::ReconstructedParticle>(coll, coll_name);
            }
            else if(coll_type == "edm4eic::MCRecoParticleAssociation") {
                event.InsertCollectionAlreadyInFrame<edm4eic::MCRecoParticleAssociation>(coll, coll_name);
            }
#endif
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
        m_event_in_file_counter++;
        return Result::Success;
    }
};